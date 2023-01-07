// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_registry_loader_win.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// The Registry subkey that contains information about external extensions.
const wchar_t kRegistryExtensions[] = L"Software\\Google\\Chrome\\Extensions";

// Registry value of the key that defines the installation parameter.
const wchar_t kRegistryExtensionInstallParam[] = L"install_parameter";

// Registry value of the key that defines the path to the .crx file.
const wchar_t kRegistryExtensionPath[] = L"path";

// Registry value of that key that defines the current version of the .crx file.
const wchar_t kRegistryExtensionVersion[] = L"version";

// Registry value of the key that defines an external update URL.
const wchar_t kRegistryExtensionUpdateUrl[] = L"update_url";

bool CanOpenFileForReading(const base::FilePath& path) {
  // Note: Because this ScopedFILE is used on the stack and not passed around
  // threads/sequences, this method doesn't require callers to run on tasks with
  // BLOCK_SHUTDOWN. SKIP_ON_SHUTDOWN is enough and safe because it guarantees
  // that if a task starts, it will always finish, and will block shutdown at
  // that point.
  base::ScopedFILE file_handle(base::OpenFile(path, "rb"));
  return file_handle.get() != NULL;
}

std::string MakePrefName(const std::string& extension_id,
                         const std::string& pref_name) {
  return base::StringPrintf("%s.%s", extension_id.c_str(), pref_name.c_str());
}

}  // namespace

namespace extensions {

ExternalRegistryLoader::ExternalRegistryLoader()
    : attempted_watching_registry_(false) {}

ExternalRegistryLoader::~ExternalRegistryLoader() {}

void ExternalRegistryLoader::StartLoading() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetOrCreateTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExternalRegistryLoader::LoadOnBlockingThread, this));
}

base::Value::Dict ExternalRegistryLoader::LoadPrefsOnBlockingThread() {
  base::Value::Dict prefs;

  // A map of IDs, to weed out duplicates between HKCU and HKLM.
  std::set<std::wstring> keys;
  base::win::RegistryKeyIterator iterator_machine_key(
      HKEY_LOCAL_MACHINE,
      kRegistryExtensions,
      KEY_WOW64_32KEY);
  for (; iterator_machine_key.Valid(); ++iterator_machine_key)
    keys.insert(iterator_machine_key.Name());
  base::win::RegistryKeyIterator iterator_user_key(
      HKEY_CURRENT_USER, kRegistryExtensions);
  for (; iterator_user_key.Valid(); ++iterator_user_key)
    keys.insert(iterator_user_key.Name());

  // Iterate over the keys found, first trying HKLM, then HKCU, as per Windows
  // policy conventions. We only fall back to HKCU if the HKLM key cannot be
  // opened, not if the data within the key is invalid, for example.
  for (auto it = keys.begin(); it != keys.end(); ++it) {
    base::win::RegKey key;
    std::wstring key_path = kRegistryExtensions;
    key_path.append(L"\\");
    key_path.append(*it);
    if (key.Open(HKEY_LOCAL_MACHINE,
                 key_path.c_str(),
                 KEY_READ | KEY_WOW64_32KEY) != ERROR_SUCCESS &&
        key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_READ) !=
            ERROR_SUCCESS) {
      LOG(ERROR) << "Unable to read registry key at path (HKLM & HKCU): "
                 << key_path << ".";
      continue;
    }

    std::string id = base::ToLowerASCII(base::WideToASCII(*it));
    if (!crx_file::id_util::IdIsValid(id)) {
      LOG(ERROR) << "Invalid id value " << id
                 << " for key " << key_path << ".";
      continue;
    }

    std::wstring extension_dist_id;
    if (key.ReadValue(kRegistryExtensionInstallParam, &extension_dist_id) ==
        ERROR_SUCCESS) {
      prefs.SetByDottedPath(
          MakePrefName(id, ExternalProviderImpl::kInstallParam),
          base::WideToASCII(extension_dist_id));
    }

    // If there is an update URL present, copy it to prefs and ignore
    // path and version keys for this entry.
    std::wstring extension_update_url;
    if (key.ReadValue(kRegistryExtensionUpdateUrl, &extension_update_url)
        == ERROR_SUCCESS) {
      prefs.SetByDottedPath(
          MakePrefName(id, ExternalProviderImpl::kExternalUpdateUrl),
          base::WideToASCII(extension_update_url));
      continue;
    }

    std::wstring extension_path_str;
    if (key.ReadValue(kRegistryExtensionPath, &extension_path_str)
        != ERROR_SUCCESS) {
      // TODO(erikkay): find a way to get this into about:extensions
      LOG(ERROR) << "Missing value " << kRegistryExtensionPath
                 << " for key " << key_path << ".";
      continue;
    }

    base::FilePath extension_path(extension_path_str);
    if (!extension_path.IsAbsolute()) {
      LOG(ERROR) << "File path " << extension_path_str
                 << " needs to be absolute in key "
                 << key_path;
      continue;
    }

    if (!base::PathExists(extension_path)) {
      LOG(ERROR) << "File " << extension_path_str
                 << " for key " << key_path
                 << " does not exist or is not readable.";
      continue;
    }

    if (!CanOpenFileForReading(extension_path)) {
      LOG(ERROR) << "File " << extension_path_str
                 << " for key " << key_path << " can not be read. "
                 << "Check that users who should have the extension "
                 << "installed have permission to read it.";
      continue;
    }

    std::wstring extension_version;
    if (key.ReadValue(kRegistryExtensionVersion, &extension_version)
        != ERROR_SUCCESS) {
      // TODO(erikkay): find a way to get this into about:extensions
      LOG(ERROR) << "Missing value " << kRegistryExtensionVersion
                 << " for key " << key_path << ".";
      continue;
    }

    base::Version version(base::WideToASCII(extension_version));
    if (!version.IsValid()) {
      LOG(ERROR) << "Invalid version value " << extension_version
                 << " for key " << key_path << ".";
      continue;
    }

    prefs.SetByDottedPath(
        MakePrefName(id, ExternalProviderImpl::kExternalVersion),
        base::WideToASCII(extension_version));
    prefs.SetByDottedPath(MakePrefName(id, ExternalProviderImpl::kExternalCrx),
                          base::AsString16(extension_path_str));
    prefs.SetByDottedPath(
        MakePrefName(id, ExternalProviderImpl::kMayBeUntrusted), true);
  }

  return prefs;
}

void ExternalRegistryLoader::LoadOnBlockingThread() {
  DCHECK(task_runner_);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::TimeTicks start_time = base::TimeTicks::Now();
  base::Value::Dict prefs = LoadPrefsOnBlockingThread();
  LOCAL_HISTOGRAM_TIMES("Extensions.ExternalRegistryLoaderWin",
                        base::TimeTicks::Now() - start_time);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ExternalRegistryLoader::CompleteLoadAndStartWatchingRegistry, this,
          std::move(prefs)));
}

void ExternalRegistryLoader::CompleteLoadAndStartWatchingRegistry(
    base::Value::Dict prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LoadFinished(std::move(prefs));

  // Attempt to watch registry if we haven't already.
  if (attempted_watching_registry_)
    return;

  LONG result = ERROR_SUCCESS;
  if ((result = hklm_key_.Create(HKEY_LOCAL_MACHINE, kRegistryExtensions,
                                 KEY_NOTIFY | KEY_WOW64_32KEY)) ==
      ERROR_SUCCESS) {
    base::win::RegKey::ChangeCallback callback =
        base::BindOnce(&ExternalRegistryLoader::OnRegistryKeyChanged,
                       base::Unretained(this), base::Unretained(&hklm_key_));
    hklm_key_.StartWatching(std::move(callback));
  } else {
    LOG(WARNING) << "Error observing HKLM: " << result;
  }

  if ((result = hkcu_key_.Create(HKEY_CURRENT_USER, kRegistryExtensions,
                                 KEY_NOTIFY)) == ERROR_SUCCESS) {
    base::win::RegKey::ChangeCallback callback =
        base::BindOnce(&ExternalRegistryLoader::OnRegistryKeyChanged,
                       base::Unretained(this), base::Unretained(&hkcu_key_));
    hkcu_key_.StartWatching(std::move(callback));
  } else {
    LOG(WARNING) << "Error observing HKCU: " << result;
  }

  attempted_watching_registry_ = true;
}

void ExternalRegistryLoader::OnRegistryKeyChanged(base::win::RegKey* key) {
  // |OnRegistryKeyChanged| is removed as an observer when the ChangeCallback is
  // called, so we need to re-register.
  key->StartWatching(
      base::BindOnce(&ExternalRegistryLoader::OnRegistryKeyChanged,
                     base::Unretained(this), base::Unretained(key)));

  GetOrCreateTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExternalRegistryLoader::UpatePrefsOnBlockingThread,
                     this));
}

scoped_refptr<base::SequencedTaskRunner>
ExternalRegistryLoader::GetOrCreateTaskRunner() {
  if (!task_runner_.get()) {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {// Requires I/O for registry.
         base::MayBlock(),

         // Inherit priority.

         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }
  return task_runner_;
}

void ExternalRegistryLoader::UpatePrefsOnBlockingThread() {
  DCHECK(task_runner_);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::TimeTicks start_time = base::TimeTicks::Now();
  base::Value::Dict prefs = LoadPrefsOnBlockingThread();
  LOCAL_HISTOGRAM_TIMES("Extensions.ExternalRegistryLoaderWinUpdate",
                        base::TimeTicks::Now() - start_time);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ExternalRegistryLoader::OnUpdated, this,
                                std::move(prefs)));
}

}  // namespace extensions

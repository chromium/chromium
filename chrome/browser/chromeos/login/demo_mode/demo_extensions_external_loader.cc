// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_extensions_external_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/external_cache_impl.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_resources.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/extension_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

namespace {

// Arbitrary, but reasonable size limit in bytes for prefs file.
constexpr size_t kPrefsSizeLimit = 1024 * 1024;

base::Optional<base::Value> LoadPrefsFromDisk(
    const base::FilePath& prefs_path) {
  if (!base::PathExists(prefs_path)) {
    LOG(WARNING) << "Demo extensions prefs not found " << prefs_path.value();
    return base::nullopt;
  }

  std::string prefs_str;
  if (!base::ReadFileToStringWithMaxSize(prefs_path, &prefs_str,
                                         kPrefsSizeLimit)) {
    LOG(ERROR) << "Failed to read prefs " << prefs_path.value() << "; "
               << "failed after reading " << prefs_str.size() << " bytes";
    return base::nullopt;
  }

  std::unique_ptr<base::Value> prefs_value =
      base::JSONReader::ReadDeprecated(prefs_str);
  if (!prefs_value) {
    LOG(ERROR) << "Unable to parse demo extensions prefs.";
    return base::nullopt;
  }

  if (!prefs_value->is_dict()) {
    LOG(ERROR) << "Demo extensions prefs not a dictionary.";
    return base::nullopt;
  }

  return base::Value::FromUniquePtrValue(std::move(prefs_value));
}

}  // namespace

// static
bool DemoExtensionsExternalLoader::SupportedForProfile(Profile* profile) {
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile))
    return false;

  DemoSession* demo_session = DemoSession::Get();
  return demo_session && demo_session->started();
}

DemoExtensionsExternalLoader::DemoExtensionsExternalLoader(
    const base::FilePath& cache_dir)
    : cache_dir_(cache_dir) {
  DCHECK(DemoSession::Get() && DemoSession::Get()->started());
}

DemoExtensionsExternalLoader::~DemoExtensionsExternalLoader() = default;

void DemoExtensionsExternalLoader::LoadApp(const std::string& app_id) {
  app_ids_.push_back(app_id);
  base::DictionaryValue prefs;
  for (const std::string& app_id : app_ids_) {
    base::DictionaryValue app_dict;
    app_dict.SetKey(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                    base::Value(extension_urls::kChromeWebstoreUpdateURL));
    prefs.SetKey(app_id, std::move(app_dict));
  }
  if (!external_cache_) {
    external_cache_ = std::make_unique<ExternalCacheImpl>(
        cache_dir_, g_browser_process->shared_url_loader_factory(),
        extensions::GetExtensionFileTaskRunner(), this,
        true /* always_check_updates */,
        false /* wait_for_cache_initialization */);
  }

  // TODO(crbug.com/991453): In offline Demo Mode, this would overwrite the
  // prefs from the Offline Demo Resources, so we don't call LoadApp() if the
  // enrollment is offline. Instead, we should merge these prefs or treat the
  // cache as a separate provider.
  external_cache_->UpdateExtensionsList(base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(std::move(prefs))));
}

void DemoExtensionsExternalLoader::StartLoading() {
  DemoSession::Get()->EnsureOfflineResourcesLoaded(base::BindOnce(
      &DemoExtensionsExternalLoader::StartLoadingFromOfflineDemoResources,
      weak_ptr_factory_.GetWeakPtr()));
}

void DemoExtensionsExternalLoader::OnExtensionListsUpdated(
    const base::DictionaryValue* prefs) {
  DCHECK(external_cache_);
  // Notifies the provider that the extensions have either been downloaded or
  // found in cache, and are ready to be installed.
  LoadFinished(prefs->CreateDeepCopy());
}

void DemoExtensionsExternalLoader::OnExtensionLoadedInCache(
    const std::string& id) {}

void DemoExtensionsExternalLoader::OnExtensionDownloadFailed(
    const std::string& id) {}

std::string DemoExtensionsExternalLoader::GetInstalledExtensionVersion(
    const std::string& id) {
  return std::string();
}

void DemoExtensionsExternalLoader::StartLoadingFromOfflineDemoResources() {
  DemoSession* demo_session = DemoSession::Get();
  DCHECK(demo_session->resources()->loaded());

  base::FilePath demo_extension_list =
      demo_session->resources()->GetExternalExtensionsPrefsPath();
  if (demo_extension_list.empty()) {
    LoadFinished(std::make_unique<base::DictionaryValue>());
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&LoadPrefsFromDisk, demo_extension_list),
      base::BindOnce(
          &DemoExtensionsExternalLoader::DemoExternalExtensionsPrefsLoaded,
          weak_ptr_factory_.GetWeakPtr()));
}

void DemoExtensionsExternalLoader::DemoExternalExtensionsPrefsLoaded(
    base::Optional<base::Value> prefs) {
  if (!prefs.has_value()) {
    LoadFinished(std::make_unique<base::DictionaryValue>());
    return;
  }
  DCHECK(prefs.value().is_dict());

  DemoSession* demo_session = DemoSession::Get();
  DCHECK(demo_session);

  // Adjust CRX paths in the prefs. Prefs on disk contains paths relative to
  // the offline demo resources root - they have to be changed to absolute paths
  // so extensions service knows from where to load them.
  for (auto&& dict_item : prefs.value().DictItems()) {
    if (!dict_item.second.is_dict())
      continue;

    const base::Value* path = dict_item.second.FindKeyOfType(
        extensions::ExternalProviderImpl::kExternalCrx,
        base::Value::Type::STRING);
    if (!path || !path->is_string())
      continue;

    base::FilePath relative_path = base::FilePath(path->GetString());
    if (relative_path.IsAbsolute()) {
      LOG(ERROR) << "Ignoring demo extension with an absolute path "
                 << dict_item.first;
      dict_item.second.RemoveKey(
          extensions::ExternalProviderImpl::kExternalCrx);
      continue;
    }

    dict_item.second.SetKey(
        extensions::ExternalProviderImpl::kExternalCrx,
        base::Value(
            demo_session->resources()->GetAbsolutePath(relative_path).value()));
  }

  LoadFinished(base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(std::move(prefs.value()))));
}

}  // namespace chromeos

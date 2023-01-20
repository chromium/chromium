// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_extensions_external_loader.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/external_cache.h"
#include "chrome/browser/ash/extensions/external_cache_impl.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/extension_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

// Arbitrary, but reasonable size limit in bytes for prefs file.
constexpr size_t kPrefsSizeLimit = 1024 * 1024;

absl::optional<base::Value::Dict> LoadPrefsFromDisk(
    const base::FilePath& prefs_path) {
  if (!base::PathExists(prefs_path)) {
    LOG(WARNING) << "Demo extensions prefs not found " << prefs_path.value();
    return absl::nullopt;
  }

  std::string prefs_str;
  if (!base::ReadFileToStringWithMaxSize(prefs_path, &prefs_str,
                                         kPrefsSizeLimit)) {
    LOG(ERROR) << "Failed to read prefs " << prefs_path.value() << "; "
               << "failed after reading " << prefs_str.size() << " bytes";
    return absl::nullopt;
  }

  absl::optional<base::Value> prefs_value = base::JSONReader::Read(prefs_str);
  if (!prefs_value) {
    LOG(ERROR) << "Unable to parse demo extensions prefs.";
    return absl::nullopt;
  }

  if (!prefs_value->is_dict()) {
    LOG(ERROR) << "Demo extensions prefs not a dictionary.";
    return absl::nullopt;
  }

  return std::move(prefs_value).value().TakeDict();
}

}  // namespace

// static
bool DemoExtensionsExternalLoader::SupportedForProfile(Profile* profile) {
  if (!ProfileHelper::IsPrimaryProfile(profile))
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
  base::Value::Dict prefs;
  for (const std::string& app : app_ids_) {
    base::Value::Dict app_dict;
    app_dict.Set(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                 extension_urls::kChromeWebstoreUpdateURL);
    prefs.Set(app, std::move(app_dict));
  }
  if (!external_cache_) {
    external_cache_ = std::make_unique<chromeos::ExternalCacheImpl>(
        cache_dir_, g_browser_process->shared_url_loader_factory(),
        extensions::GetExtensionFileTaskRunner(), this,
        true /* always_check_updates */,
        false /* wait_for_cache_initialization */,
        false /* allow_scheduled_updates */);
  }

  // TODO(crbug.com/991453): In offline Demo Mode, this would overwrite the
  // prefs from the Offline Demo Resources, so we don't call LoadApp() if the
  // enrollment is offline. Instead, we should merge these prefs or treat the
  // cache as a separate provider.
  external_cache_->UpdateExtensionsList(std::move(prefs));
}

void DemoExtensionsExternalLoader::StartLoading() {
  DemoSession::Get()->EnsureResourcesLoaded(base::BindOnce(
      &DemoExtensionsExternalLoader::StartLoadingFromOfflineDemoResources,
      weak_ptr_factory_.GetWeakPtr()));
}

void DemoExtensionsExternalLoader::OnExtensionListsUpdated(
    const base::Value::Dict& prefs) {
  DCHECK(external_cache_);
  // Notifies the provider that the extensions have either been downloaded or
  // found in cache, and are ready to be installed.
  LoadFinished(prefs.Clone());
}

void DemoExtensionsExternalLoader::StartLoadingFromOfflineDemoResources() {
  DemoSession* demo_session = DemoSession::Get();
  DCHECK(demo_session->components()->resources_component_loaded());

  base::FilePath demo_extension_list =
      demo_session->components()->GetExternalExtensionsPrefsPath();
  if (demo_extension_list.empty()) {
    LoadFinished(base::Value::Dict());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&LoadPrefsFromDisk, demo_extension_list),
      base::BindOnce(
          &DemoExtensionsExternalLoader::DemoExternalExtensionsPrefsLoaded,
          weak_ptr_factory_.GetWeakPtr()));
}

void DemoExtensionsExternalLoader::DemoExternalExtensionsPrefsLoaded(
    absl::optional<base::Value::Dict> prefs) {
  if (!prefs.has_value()) {
    LoadFinished(base::Value::Dict());
    return;
  }
  DemoSession* demo_session = DemoSession::Get();
  DCHECK(demo_session);

  // Adjust CRX paths in the prefs. Prefs on disk contains paths relative to
  // the offline demo resources root - they have to be changed to absolute paths
  // so extensions service knows from where to load them.
  for (auto&& dict_item : prefs.value()) {
    if (!dict_item.second.is_dict())
      continue;
    base::Value::Dict& value_dict = dict_item.second.GetDict();
    const std::string* path =
        value_dict.FindString(extensions::ExternalProviderImpl::kExternalCrx);
    if (!path)
      continue;

    base::FilePath relative_path = base::FilePath(*path);
    if (relative_path.IsAbsolute()) {
      LOG(ERROR) << "Ignoring demo extension with an absolute path "
                 << dict_item.first;
      value_dict.Remove(extensions::ExternalProviderImpl::kExternalCrx);
      continue;
    }

    value_dict.Set(
        extensions::ExternalProviderImpl::kExternalCrx,
        demo_session->components()->GetAbsolutePath(relative_path).value());
  }

  LoadFinished(std::move(prefs).value());
}

}  // namespace ash

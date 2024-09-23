// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_data.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/cws_item_service.pb.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/verifier_formats.h"
#include "kiosk_app_data_base.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

using content::BrowserThread;

namespace ash {

namespace {

// Keys for local state data. See sample layout in KioskChromeAppManager.
constexpr char kKeyRequiredPlatformVersion[] = "required_platform_version";

// Returns true for valid kiosk app manifest.
bool IsValidKioskAppManifest(const extensions::Manifest& manifest) {
  return manifest.FindBoolPath(extensions::manifest_keys::kKioskEnabled)
      .value_or(false);
}

std::string ValueToString(base::ValueView value) {
  std::string json;
  base::JSONWriter::Write(value, &json);
  return json;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// KioskAppData::CrxLoader
// Loads meta data from crx file.

class KioskAppData::CrxLoader : public extensions::SandboxedUnpackerClient {
 public:
  CrxLoader(const base::WeakPtr<KioskAppData>& client,
            const base::FilePath& crx_file)
      : client_(client),
        crx_file_(crx_file),
        task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}
  CrxLoader(const CrxLoader&) = delete;
  CrxLoader& operator=(const CrxLoader&) = delete;

  void Start() {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&CrxLoader::StartInThreadPool, this));
  }

  bool success() const { return success_; }
  const base::FilePath& crx_file() const { return crx_file_; }
  const std::string& name() const { return name_; }
  const SkBitmap& icon() const { return icon_; }
  const std::string& required_platform_version() const {
    return required_platform_version_;
  }

 private:
  ~CrxLoader() override = default;

  // extensions::SandboxedUnpackerClient
  void OnUnpackSuccess(const base::FilePath& temp_dir,
                       const base::FilePath& extension_root,
                       std::unique_ptr<base::Value::Dict> original_manifest,
                       const extensions::Extension* extension,
                       const SkBitmap& install_icon,
                       base::Value::Dict ruleset_install_prefs) override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    const extensions::KioskModeInfo* info =
        extensions::KioskModeInfo::Get(extension);
    if (info == nullptr) {
      LOG(ERROR) << extension->id() << " is not a valid kiosk app.";
      success_ = false;
    } else {
      success_ = true;
      name_ = extension->name();
      icon_ = install_icon;
      required_platform_version_ = info->required_platform_version;
    }
    NotifyFinishedInThreadPool();
  }
  void OnUnpackFailure(const extensions::CrxInstallError& error) override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    success_ = false;
    NotifyFinishedInThreadPool();
  }

  void StartInThreadPool() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    if (!temp_dir_.CreateUniqueTempDir()) {
      success_ = false;
      NotifyFinishedInThreadPool();
      return;
    }

    auto unpacker = base::MakeRefCounted<extensions::SandboxedUnpacker>(
        extensions::mojom::ManifestLocation::kInternal,
        extensions::Extension::NO_FLAGS, temp_dir_.GetPath(),
        task_runner_.get(), this);
    unpacker->StartWithCrx(extensions::CRXFileInfo(
        crx_file_, extensions::GetPolicyVerifierFormat()));
  }

  void NotifyFinishedInThreadPool() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    if (!temp_dir_.Delete()) {
      LOG(WARNING) << "Can not delete temp directory at "
                   << temp_dir_.GetPath().value();
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&CrxLoader::NotifyFinishedOnUIThread, this));
  }

  void NotifyFinishedOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (client_) {
      client_->OnCrxLoadFinished(this);
    }
  }

  base::WeakPtr<KioskAppData> client_;
  base::FilePath crx_file_;
  bool success_ = false;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::ScopedTempDir temp_dir_;

  // Extracted meta data.
  std::string name_;
  SkBitmap icon_;
  std::string required_platform_version_;
};

////////////////////////////////////////////////////////////////////////////////
// KioskAppData::WebstoreDataParser
// Use WebstoreInstallHelper to parse the manifest and decode the icon.

class KioskAppData::WebstoreDataParser
    : public extensions::WebstoreInstallHelper::Delegate {
 public:
  explicit WebstoreDataParser(const base::WeakPtr<KioskAppData>& client)
      : client_(client) {}
  WebstoreDataParser(const WebstoreDataParser&) = delete;
  WebstoreDataParser& operator=(const WebstoreDataParser&) = delete;

  void Start(const std::string& app_id,
             const std::string& manifest,
             const GURL& icon_url,
             network::mojom::URLLoaderFactory* loader_factory) {
    scoped_refptr<extensions::WebstoreInstallHelper> webstore_helper =
        new extensions::WebstoreInstallHelper(this, app_id, manifest, icon_url);
    webstore_helper->Start(loader_factory);
  }

 private:
  friend class base::RefCounted<WebstoreDataParser>;

  ~WebstoreDataParser() override = default;

  void ReportFailure() {
    if (client_) {
      client_->OnWebstoreParseFailure();
    }

    delete this;
  }

  // WebstoreInstallHelper::Delegate overrides:
  void OnWebstoreParseSuccess(const std::string& id,
                              const SkBitmap& icon,
                              base::Value::Dict parsed_manifest) override {
    extensions::Manifest manifest(
        extensions::mojom::ManifestLocation::kInvalidLocation,
        std::move(parsed_manifest), id);

    if (!IsValidKioskAppManifest(manifest)) {
      ReportFailure();
      return;
    }

    std::string required_platform_version;
    if (const base::Value* temp = manifest.FindPath(
            extensions::manifest_keys::kKioskRequiredPlatformVersion)) {
      if (!temp->is_string() ||
          !extensions::KioskModeInfo::IsValidPlatformVersion(
              temp->GetString())) {
        ReportFailure();
        return;
      }
      required_platform_version = temp->GetString();
    }

    if (client_) {
      client_->OnWebstoreParseSuccess(icon, required_platform_version);
    }
    delete this;
  }
  void OnWebstoreParseFailure(const std::string& id,
                              InstallHelperResultCode result_code,
                              const std::string& error_message) override {
    ReportFailure();
  }

  base::WeakPtr<KioskAppData> client_;
};

////////////////////////////////////////////////////////////////////////////////
// KioskAppData

KioskAppData::KioskAppData(KioskAppDataDelegate* delegate,
                           const std::string& app_id,
                           const AccountId& account_id,
                           const GURL& update_url,
                           const base::FilePath& cached_crx)
    : KioskAppDataBase(KioskChromeAppManager::kKioskDictionaryName,
                       app_id,
                       account_id),
      delegate_(delegate),
      status_(Status::kInit),
      update_url_(update_url),
      crx_file_(cached_crx) {}

KioskAppData::~KioskAppData() = default;

void KioskAppData::Load() {
  SetStatus(Status::kLoading);

  if (LoadFromCache()) {
    return;
  }

  StartFetch();
}

void KioskAppData::LoadFromInstalledApp(Profile* profile,
                                        const extensions::Extension* app) {
  SetStatus(Status::kLoading);

  if (!app) {
    app = extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
        app_id());
  }

  DCHECK_EQ(app_id(), app->id());

  name_ = app->name();
  required_platform_version_ =
      extensions::KioskModeInfo::Get(app)->required_platform_version;

  const int kIconSize = extension_misc::EXTENSION_ICON_LARGE;
  extensions::ExtensionResource image = extensions::IconsInfo::GetIconResource(
      app, kIconSize, ExtensionIconSet::Match::kBigger);
  extensions::ImageLoader::Get(profile)->LoadImageAsync(
      app, image, gfx::Size(kIconSize, kIconSize),
      base::BindOnce(&KioskAppData::OnExtensionIconLoaded,
                     weak_factory_.GetWeakPtr()));
}

void KioskAppData::SetCachedCrx(const base::FilePath& crx_file) {
  if (crx_file_ == crx_file) {
    return;
  }

  crx_file_ = crx_file;
  LoadFromCrx();
}

bool KioskAppData::IsLoading() const {
  return status_ == Status::kLoading;
}

bool KioskAppData::IsFromWebStore() const {
  return update_url_.is_empty() ||
         extension_urls::IsWebstoreUpdateUrl(update_url_);
}

void KioskAppData::SetStatusForTest(Status status) {
  SetStatus(status);
}

// static
std::unique_ptr<KioskAppData> KioskAppData::CreateForTest(
    KioskAppDataDelegate* delegate,
    const std::string& app_id,
    const AccountId& account_id,
    const GURL& update_url,
    const std::string& required_platform_version) {
  std::unique_ptr<KioskAppData> data(new KioskAppData(
      delegate, app_id, account_id, update_url, base::FilePath()));
  data->status_ = Status::kLoaded;
  data->required_platform_version_ = required_platform_version;
  return data;
}

void KioskAppData::SetStatus(Status status) {
  if (status_ == status) {
    return;
  }

  status_ = status;

  if (!delegate_) {
    return;
  }

  switch (status_) {
    case Status::kInit:
    case Status::kLoading:
    case Status::kLoaded:
      delegate_->OnKioskAppDataChanged(app_id());
      break;
    case Status::kError:
      delegate_->OnKioskAppDataLoadFailure(app_id());
      break;
  }
}

network::mojom::URLLoaderFactory* KioskAppData::GetURLLoaderFactory() {
  return g_browser_process->system_network_context_manager()
      ->GetURLLoaderFactory();
}

bool KioskAppData::LoadFromCache() {
  PrefService* local_state = g_browser_process->local_state();
  const base::Value::Dict& dict = local_state->GetDict(dictionary_name());

  if (!LoadFromDictionary(dict)) {
    return false;
  }

  DecodeIcon(base::BindOnce(&KioskAppData::OnIconLoadDone,
                            weak_factory_.GetWeakPtr()));

  const std::string app_key = std::string(kKeyApps) + '.' + app_id();
  const std::string required_platform_version_key =
      app_key + '.' + kKeyRequiredPlatformVersion;

  const std::string* maybe_required_platform_version =
      dict.FindStringByDottedPath(required_platform_version_key);
  if (!maybe_required_platform_version) {
    return false;
  }

  required_platform_version_ = *maybe_required_platform_version;
  return true;
}

void KioskAppData::SetCache(const std::string& name,
                            const SkBitmap& icon,
                            const std::string& required_platform_version) {
  name_ = name;
  required_platform_version_ = required_platform_version;
  icon_ = gfx::ImageSkia::CreateFrom1xBitmap(icon);
  icon_.MakeThreadSafe();

  base::FilePath cache_dir;
  if (delegate_) {
    delegate_->GetKioskAppIconCacheDir(&cache_dir);
  }

  SaveIcon(icon, cache_dir);

  PrefService* local_state = g_browser_process->local_state();
  ScopedDictPrefUpdate dict_update(local_state, dictionary_name());
  SaveToDictionary(dict_update);

  const std::string app_key = std::string(kKeyApps) + '.' + app_id();
  const std::string required_platform_version_key =
      app_key + '.' + kKeyRequiredPlatformVersion;

  dict_update->SetByDottedPath(required_platform_version_key,
                               required_platform_version);
}

void KioskAppData::OnExtensionIconLoaded(const gfx::Image& icon) {
  if (icon.IsEmpty()) {
    LOG(WARNING) << "Failed to load icon from installed app"
                 << ", id=" << app_id();
    SetCache(name_, *extensions::util::GetDefaultAppIcon().bitmap(),
             required_platform_version_);
  } else {
    SetCache(name_, icon.AsBitmap(), required_platform_version_);
  }

  SetStatus(Status::kLoaded);
}

void KioskAppData::OnIconLoadDone(std::optional<gfx::ImageSkia> icon) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  kiosk_app_icon_loader_.reset();
  if (!icon.has_value()) {
    // Re-fetch data from web store when failed to load cached data.
    StartFetch();
    return;
  }

  icon_ = icon.value();
  SetStatus(Status::kLoaded);
}

void KioskAppData::OnWebstoreParseSuccess(
    const SkBitmap& icon,
    const std::string& required_platform_version) {
  SetCache(name_, icon, required_platform_version);
  SetStatus(Status::kLoaded);
}

void KioskAppData::OnWebstoreParseFailure() {
  LOG(WARNING) << "Webstore request parse failure for app_id=" << app_id();
  SetStatus(Status::kError);
}

void KioskAppData::StartFetch() {
  if (!IsFromWebStore()) {
    LoadFromCrx();
    return;
  }

  webstore_fetcher_ =
      std::make_unique<extensions::WebstoreDataFetcher>(this, GURL(), app_id());
  webstore_fetcher_->set_max_auto_retries(3);
  webstore_fetcher_->Start(g_browser_process->system_network_context_manager()
                               ->GetURLLoaderFactory());
}

void KioskAppData::OnWebstoreRequestFailure(const std::string& extension_id) {
  LOG(WARNING) << "Webstore request failure for app_id=" << extension_id;
  SetStatus(Status::kError);
}

void KioskAppData::OnWebstoreItemJSONAPIResponseParseSuccess(
    const std::string& extension_id,
    const base::Value::Dict& webstore_data) {
  const std::string* id = webstore_data.FindString(kIdKey);
  if (!id) {
    LOG(ERROR) << "Webstore response error (" << kIdKey
               << "): " << ValueToString(webstore_data);
    OnWebstoreResponseParseFailure(extension_id, kInvalidWebstoreResponseError);
    return;
  }
  if (extension_id != *id) {
    LOG(ERROR) << "Webstore response error (" << kIdKey
               << "): " << ValueToString(webstore_data);
    LOG(ERROR) << "Received extension id " << *id
               << " does not equal expected extension id " << extension_id;
    OnWebstoreResponseParseFailure(extension_id, kInvalidWebstoreResponseError);
    return;
  }
  webstore_fetcher_.reset();

  std::string manifest;
  if (!CheckResponseKeyValue(*id, webstore_data, kManifestKey, &manifest)) {
    return;
  }

  if (!CheckResponseKeyValue(*id, webstore_data, kLocalizedNameKey, &name_)) {
    return;
  }

  std::string icon_url_string;
  if (!CheckResponseKeyValue(*id, webstore_data, kIconUrlKey,
                             &icon_url_string)) {
    return;
  }

  GURL icon_url =
      extension_urls::GetWebstoreLaunchURL().Resolve(icon_url_string);
  if (!icon_url.is_valid()) {
    LOG(ERROR) << "Webstore response error (icon url): "
               << ValueToString(webstore_data);
    OnWebstoreResponseParseFailure(extension_id, kInvalidWebstoreResponseError);
    return;
  }

  // WebstoreDataParser deletes itself when done.
  (new WebstoreDataParser(weak_factory_.GetWeakPtr()))
      ->Start(app_id(), manifest, icon_url, GetURLLoaderFactory());
}

void KioskAppData::OnFetchItemSnippetParseSuccess(
    const std::string& extension_id,
    extensions::FetchItemSnippetResponse item_snippet) {
  if (extension_id != item_snippet.item_id()) {
    LOG(ERROR) << "Webstore response error (itemId):"
               << " received extension id " << item_snippet.item_id()
               << " does not equal expected extension id " << extension_id;
    OnWebstoreResponseParseFailure(extension_id, kInvalidWebstoreResponseError);
    return;
  }

  webstore_fetcher_.reset();

  GURL icon_url =
      extension_urls::GetWebstoreLaunchURL().Resolve(item_snippet.logo_uri());
  if (!icon_url.is_valid()) {
    LOG(ERROR) << "Webstore response error (iconUri):"
               << " the provided icon url " << item_snippet.logo_uri()
               << " is not valid.";
    OnWebstoreResponseParseFailure(extension_id, kInvalidWebstoreResponseError);
    return;
  }

  name_ = item_snippet.title();

  // WebstoreDataParser deletes itself when done.
  (new WebstoreDataParser(weak_factory_.GetWeakPtr()))
      ->Start(app_id(), item_snippet.manifest(), icon_url,
              GetURLLoaderFactory());
}

void KioskAppData::OnWebstoreResponseParseFailure(
    const std::string& extension_id,
    const std::string& error) {
  LOG(ERROR) << "Webstore failed for kiosk app " << app_id() << ", " << error;
  webstore_fetcher_.reset();
  SetStatus(Status::kError);
}

bool KioskAppData::CheckResponseKeyValue(const std::string& extension_id,
                                         const base::Value::Dict& response,
                                         const char* key,
                                         std::string* value) {
  const std::string* value_ptr = response.FindString(key);
  if (!value_ptr) {
    LOG(ERROR) << "Webstore response error (" << key
               << "): " << ValueToString(response);
    OnWebstoreResponseParseFailure(extension_id, kInvalidWebstoreResponseError);
    return false;
  }
  *value = *value_ptr;
  return true;
}

void KioskAppData::LoadFromCrx() {
  if (crx_file_.empty()) {
    return;
  }

  scoped_refptr<CrxLoader> crx_loader(
      new CrxLoader(weak_factory_.GetWeakPtr(), crx_file_));
  crx_loader->Start();
}

void KioskAppData::OnCrxLoadFinished(const CrxLoader* crx_loader) {
  DCHECK(crx_loader);

  if (crx_loader->crx_file() != crx_file_) {
    return;
  }

  if (!crx_loader->success()) {
    LOG(ERROR) << "Failed to load cached extension data for app_id="
               << app_id();
    // If after unpacking the cached extension we received an error, schedule
    // a redownload upon next session start(kiosk or login).
    if (delegate_) {
      delegate_->OnExternalCacheDamaged(app_id());
    }

    SetStatus(Status::kInit);
    return;
  }

  SkBitmap icon = crx_loader->icon();
  if (icon.empty()) {
    icon = *extensions::util::GetDefaultAppIcon().bitmap();
  }
  SetCache(crx_loader->name(), icon, crx_loader->required_platform_version());

  SetStatus(Status::kLoaded);
}

}  // namespace ash

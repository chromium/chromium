// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_data.h"

#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/verifier_formats.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Keys for local state data. See sample layout in KioskAppManager.
constexpr char kKeyRequiredPlatformVersion[] = "required_platform_version";

constexpr char kInvalidWebstoreResponseError[] =
    "Invalid Chrome Web Store reponse";

bool ignore_kiosk_app_data_load_failures_for_testing = false;

// Returns true for valid kiosk app manifest.
bool IsValidKioskAppManifest(const extensions::Manifest& manifest) {
  bool kiosk_enabled;
  if (manifest.GetBoolean(extensions::manifest_keys::kKioskEnabled,
                          &kiosk_enabled)) {
    return kiosk_enabled;
  }

  return false;
}

std::string ValueToString(const base::Value& value) {
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
        success_(false),
        task_runner_(base::CreateSequencedTaskRunner(
            {base::ThreadPool(), base::MayBlock(),
             base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

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
  void OnUnpackSuccess(
      const base::FilePath& temp_dir,
      const base::FilePath& extension_root,
      std::unique_ptr<base::DictionaryValue> original_manifest,
      const extensions::Extension* extension,
      const SkBitmap& install_icon,
      const base::Optional<int>& dnr_ruleset_checksum) override {
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
        extensions::Manifest::INTERNAL, extensions::Extension::NO_FLAGS,
        temp_dir_.GetPath(), task_runner_.get(), this);
    unpacker->StartWithCrx(extensions::CRXFileInfo(
        crx_file_, extensions::GetPolicyVerifierFormat()));
  }

  void NotifyFinishedInThreadPool() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    if (!temp_dir_.Delete()) {
      LOG(WARNING) << "Can not delete temp directory at "
                   << temp_dir_.GetPath().value();
    }

    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&CrxLoader::NotifyFinishedOnUIThread, this));
  }

  void NotifyFinishedOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (client_)
      client_->OnCrxLoadFinished(this);
  }

  base::WeakPtr<KioskAppData> client_;
  base::FilePath crx_file_;
  bool success_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::ScopedTempDir temp_dir_;

  // Extracted meta data.
  std::string name_;
  SkBitmap icon_;
  std::string required_platform_version_;

  DISALLOW_COPY_AND_ASSIGN(CrxLoader);
};

////////////////////////////////////////////////////////////////////////////////
// KioskAppData::WebstoreDataParser
// Use WebstoreInstallHelper to parse the manifest and decode the icon.

class KioskAppData::WebstoreDataParser
    : public extensions::WebstoreInstallHelper::Delegate {
 public:
  explicit WebstoreDataParser(const base::WeakPtr<KioskAppData>& client)
      : client_(client) {}

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
    if (client_)
      client_->OnWebstoreParseFailure();

    delete this;
  }

  // WebstoreInstallHelper::Delegate overrides:
  void OnWebstoreParseSuccess(
      const std::string& id,
      const SkBitmap& icon,
      std::unique_ptr<base::DictionaryValue> parsed_manifest) override {
    extensions::Manifest manifest(extensions::Manifest::INVALID_LOCATION,
                                  std::move(parsed_manifest));

    if (!IsValidKioskAppManifest(manifest)) {
      ReportFailure();
      return;
    }

    std::string required_platform_version;
    if (manifest.HasPath(
            extensions::manifest_keys::kKioskRequiredPlatformVersion) &&
        (!manifest.GetString(
             extensions::manifest_keys::kKioskRequiredPlatformVersion,
             &required_platform_version) ||
         !extensions::KioskModeInfo::IsValidPlatformVersion(
             required_platform_version))) {
      ReportFailure();
      return;
    }

    if (client_)
      client_->OnWebstoreParseSuccess(icon, required_platform_version);
    delete this;
  }
  void OnWebstoreParseFailure(const std::string& id,
                              InstallHelperResultCode result_code,
                              const std::string& error_message) override {
    ReportFailure();
  }

  base::WeakPtr<KioskAppData> client_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreDataParser);
};

////////////////////////////////////////////////////////////////////////////////
// KioskAppData

KioskAppData::KioskAppData(KioskAppDataDelegate* delegate,
                           const std::string& app_id,
                           const AccountId& account_id,
                           const GURL& update_url,
                           const base::FilePath& cached_crx)
    : KioskAppDataBase(KioskAppManager::kKioskDictionaryName,
                       app_id,
                       account_id),
      delegate_(delegate),
      status_(STATUS_INIT),
      update_url_(update_url),
      crx_file_(cached_crx) {
  if (ignore_kiosk_app_data_load_failures_for_testing) {
    LOG(WARNING) << "Force KioskAppData loaded for testing.";
    SetStatus(STATUS_LOADED);
  }
}

KioskAppData::~KioskAppData() = default;

void KioskAppData::Load() {
  SetStatus(STATUS_LOADING);

  if (LoadFromCache())
    return;

  StartFetch();
}

void KioskAppData::LoadFromInstalledApp(Profile* profile,
                                        const extensions::Extension* app) {
  SetStatus(STATUS_LOADING);

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
      app, kIconSize, ExtensionIconSet::MATCH_BIGGER);
  extensions::ImageLoader::Get(profile)->LoadImageAsync(
      app, image, gfx::Size(kIconSize, kIconSize),
      base::BindOnce(&KioskAppData::OnExtensionIconLoaded,
                     weak_factory_.GetWeakPtr()));
}

void KioskAppData::SetCachedCrx(const base::FilePath& crx_file) {
  if (crx_file_ == crx_file)
    return;

  crx_file_ = crx_file;
  LoadFromCrx();
}

bool KioskAppData::IsLoading() const {
  return status_ == STATUS_LOADING;
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
  data->status_ = STATUS_LOADED;
  data->required_platform_version_ = required_platform_version;
  return data;
}

void KioskAppData::SetStatus(Status status) {
  if (status == STATUS_ERROR &&
      ignore_kiosk_app_data_load_failures_for_testing) {
    LOG(WARNING) << "Ignoring KioskAppData error for testing. Force OK.";
    status = STATUS_LOADED;
  }

  if (status_ == status)
    return;

  status_ = status;

  if (!delegate_)
    return;

  switch (status_) {
    case STATUS_INIT:
      break;
    case STATUS_LOADING:
    case STATUS_LOADED:
      delegate_->OnKioskAppDataChanged(app_id());
      break;
    case STATUS_ERROR:
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
  const base::DictionaryValue* dict =
      local_state->GetDictionary(dictionary_name());

  if (!LoadFromDictionary(*dict))
    return false;

  const std::string app_key = std::string(kKeyApps) + '.' + app_id();
  const std::string required_platform_version_key =
      app_key + '.' + kKeyRequiredPlatformVersion;

  return dict->GetString(required_platform_version_key,
                         &required_platform_version_);
}

void KioskAppData::SetCache(const std::string& name,
                            const SkBitmap& icon,
                            const std::string& required_platform_version) {
  name_ = name;
  required_platform_version_ = required_platform_version;
  icon_ = gfx::ImageSkia::CreateFrom1xBitmap(icon);
  icon_.MakeThreadSafe();

  base::FilePath cache_dir;
  if (delegate_)
    delegate_->GetKioskAppIconCacheDir(&cache_dir);

  SaveIcon(icon, cache_dir);

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(local_state, dictionary_name());
  SaveToDictionary(dict_update);

  const std::string app_key = std::string(kKeyApps) + '.' + app_id();
  const std::string required_platform_version_key =
      app_key + '.' + kKeyRequiredPlatformVersion;

  dict_update->SetString(required_platform_version_key,
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

  SetStatus(STATUS_LOADED);
}

void KioskAppData::OnIconLoadSuccess(const gfx::ImageSkia& icon) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  kiosk_app_icon_loader_.reset();
  icon_ = icon;
  SetStatus(STATUS_LOADED);
}

void KioskAppData::OnIconLoadFailure() {
  kiosk_app_icon_loader_.reset();
  // Re-fetch data from web store when failed to load cached data.
  StartFetch();
}

// static
void KioskAppData::SetIgnoreKioskAppDataLoadFailuresForTesting(bool value) {
  ignore_kiosk_app_data_load_failures_for_testing = value;
}

void KioskAppData::OnWebstoreParseSuccess(
    const SkBitmap& icon,
    const std::string& required_platform_version) {
  SetCache(name_, icon, required_platform_version);
  SetStatus(STATUS_LOADED);
}

void KioskAppData::OnWebstoreParseFailure() {
  SetStatus(STATUS_ERROR);
}

void KioskAppData::StartFetch() {
  if (!IsFromWebStore()) {
    LoadFromCrx();
    return;
  }

  webstore_fetcher_.reset(
      new extensions::WebstoreDataFetcher(this, GURL(), app_id()));
  webstore_fetcher_->set_max_auto_retries(3);
  webstore_fetcher_->Start(g_browser_process->system_network_context_manager()
                               ->GetURLLoaderFactory());
}

void KioskAppData::OnWebstoreRequestFailure() {
  SetStatus(STATUS_ERROR);
}

void KioskAppData::OnWebstoreResponseParseSuccess(
    std::unique_ptr<base::DictionaryValue> webstore_data) {
  // Takes ownership of |webstore_data|.
  webstore_fetcher_.reset();

  std::string manifest;
  if (!CheckResponseKeyValue(webstore_data.get(), kManifestKey, &manifest))
    return;

  if (!CheckResponseKeyValue(webstore_data.get(), kLocalizedNameKey, &name_))
    return;

  std::string icon_url_string;
  if (!CheckResponseKeyValue(webstore_data.get(), kIconUrlKey,
                             &icon_url_string))
    return;

  GURL icon_url =
      extension_urls::GetWebstoreLaunchURL().Resolve(icon_url_string);
  if (!icon_url.is_valid()) {
    LOG(ERROR) << "Webstore response error (icon url): "
               << ValueToString(*webstore_data);
    OnWebstoreResponseParseFailure(kInvalidWebstoreResponseError);
    return;
  }

  // WebstoreDataParser deletes itself when done.
  (new WebstoreDataParser(weak_factory_.GetWeakPtr()))
      ->Start(app_id(), manifest, icon_url, GetURLLoaderFactory());
}

void KioskAppData::OnWebstoreResponseParseFailure(const std::string& error) {
  LOG(ERROR) << "Webstore failed for kiosk app " << app_id() << ", " << error;
  webstore_fetcher_.reset();
  SetStatus(STATUS_ERROR);
}

bool KioskAppData::CheckResponseKeyValue(const base::DictionaryValue* response,
                                         const char* key,
                                         std::string* value) {
  if (!response->GetString(key, value)) {
    LOG(ERROR) << "Webstore response error (" << key
               << "): " << ValueToString(*response);
    OnWebstoreResponseParseFailure(kInvalidWebstoreResponseError);
    return false;
  }
  return true;
}

void KioskAppData::LoadFromCrx() {
  if (crx_file_.empty())
    return;

  scoped_refptr<CrxLoader> crx_loader(
      new CrxLoader(weak_factory_.GetWeakPtr(), crx_file_));
  crx_loader->Start();
}

void KioskAppData::OnCrxLoadFinished(const CrxLoader* crx_loader) {
  DCHECK(crx_loader);

  if (crx_loader->crx_file() != crx_file_)
    return;

  if (!crx_loader->success()) {
    SetStatus(STATUS_ERROR);
    return;
  }

  SkBitmap icon = crx_loader->icon();
  if (icon.empty())
    icon = *extensions::util::GetDefaultAppIcon().bitmap();
  SetCache(crx_loader->name(), icon, crx_loader->required_platform_version());

  SetStatus(STATUS_LOADED);
}

}  // namespace chromeos

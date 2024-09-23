// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/customization/customization_document.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/customization/customization_wallpaper_downloader.h"
#include "chrome/browser/ash/customization/customization_wallpaper_util.h"
#include "chrome/browser/ash/extensions/default_app_order.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/external_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension_urls.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash {
namespace {

  // Manifest attributes names.
const char kVersionAttr[] = "version";
const char kDefaultAttr[] = "default";
const char kInitialLocaleAttr[] = "initial_locale";
const char kInitialTimezoneAttr[] = "initial_timezone";
const char kKeyboardLayoutAttr[] = "keyboard_layout";
const char kHwidMapAttr[] = "hwid_map";
const char kHwidMaskAttr[] = "hwid_mask";
const char kSetupContentAttr[] = "setup_content";
const char kEulaPageAttr[] = "eula_page";
const char kDefaultWallpaperAttr[] = "default_wallpaper";
const char kDefaultAppsAttr[] = "default_apps";
const char kLocalizedContent[] = "localized_content";
const char kDefaultAppsFolderName[] = "default_apps_folder_name";
const char kIdAttr[] = "id";

const char kAcceptedManifestVersion[] = "1.0";

// This is subdirectory relative to PathService(DIR_CHROMEOS_CUSTOM_WALLPAPERS),
// where downloaded (and resized) wallpaper is stored.
const char kCustomizationDefaultWallpaperDir[] = "customization";

// The original downloaded image file is stored under this name.
const char kCustomizationDefaultWallpaperDownloadedFile[] =
    "default_downloaded_wallpaper.bin";

// Name of local state option that tracks if services customization has been
// applied.
const char kServicesCustomizationAppliedPref[] = "ServicesCustomizationApplied";

// Maximum number of retries to fetch file if network is not available.
const int kMaxFetchRetries = 3;

// Delay between file fetch retries if network is not available.
const int kRetriesDelayInSec = 2;

// Name of profile option that tracks cached version of service customization.
const char kServicesCustomizationKey[] = "customization.manifest_cache";

// Empty customization document that doesn't customize anything.
const char kEmptyServicesCustomizationManifest[] = "{ \"version\": \"1.0\" }";

constexpr net::NetworkTrafficAnnotationTag kCustomizationDocumentNetworkTag =
    net::DefineNetworkTrafficAnnotation("customization_document",
                                        R"(
        semantics {
          sender: "Customization document"
          description:
            "Get OEM customization manifest from OEM specific URLs that "
            "provide custom configuration locales, wallpaper etc."
          trigger:
            "Triggered on OOBE after user accepts EULA and everytime the "
            "device boots in. Expected to run only once at OOBE. If the "
            "network request fails, retried each boot until it succeeds."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
         cookies_allowed: NO
         setting:
           "This feature is set by OEMs and can be overridden by users."
         policy_exception_justification:
           "This request is made based on OEM customization and does not "
           "send/store any sensitive data."
        })");

struct CustomizationDocumentTestOverride {
  raw_ptr<ServicesCustomizationDocument, DanglingUntriaged>
      customization_document = nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
};

// Global overrider for ServicesCustomizationDocument for tests.
CustomizationDocumentTestOverride* g_test_overrides = nullptr;


std::string GetLocaleSpecificStringImpl(const base::Value::Dict& root,
                                        const std::string& locale,
                                        const std::string& dictionary_name,
                                        const std::string& entry_name) {
  const base::Value::Dict* dictionary_content = root.FindDict(dictionary_name);
  if (!dictionary_content)
    return std::string();

  const base::Value::Dict* locale_dictionary =
      dictionary_content->FindDict(locale);
  if (locale_dictionary) {
    const std::string* result = locale_dictionary->FindString(entry_name);
    if (result)
      return *result;
  }

  const base::Value::Dict* default_dictionary =
      dictionary_content->FindDict(kDefaultAttr);
  if (default_dictionary) {
    const std::string* result = default_dictionary->FindString(entry_name);
    if (result)
      return *result;
  }

  return std::string();
}

void CheckWallpaperCacheExists(const base::FilePath& path, bool* exists) {
  DCHECK(exists);
  *exists = base::PathExists(path);
}

std::string ReadFileInBackground(const base::FilePath& file) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string manifest;
  if (!base::ReadFileToString(file, &manifest)) {
    manifest.clear();
    LOG(ERROR) << "Failed to load services customization manifest from: "
               << file.value();
  }

  return manifest;
}

}  // anonymous namespace

// A custom extensions::ExternalLoader that the ServicesCustomizationDocument
// creates and uses to publish OEM default apps to the extensions system.
class ServicesCustomizationExternalLoader : public extensions::ExternalLoader {
 public:
  explicit ServicesCustomizationExternalLoader(Profile* profile)
      : profile_(profile) {}

  ServicesCustomizationExternalLoader(
      const ServicesCustomizationExternalLoader&) = delete;
  ServicesCustomizationExternalLoader& operator=(
      const ServicesCustomizationExternalLoader&) = delete;

  Profile* profile() { return profile_; }

  // Used by the ServicesCustomizationDocument to update the current apps.
  void SetCurrentApps(base::Value::Dict prefs) {
    apps_ = std::move(prefs);
    is_apps_set_ = true;
    StartLoading();
  }

  // Implementation of extensions::ExternalLoader:
  void StartLoading() override {
    if (!is_apps_set_) {
      ServicesCustomizationDocument::GetInstance()->StartFetching();
      // In case of missing customization ID, SetCurrentApps will be called
      // synchronously from StartFetching and this function will be called
      // recursively so we need to return to avoid calling LoadFinished twice.
      // In case of async load it is safe to return empty list because this
      // provider didn't install any app yet so no app can be removed due to
      // returning empty list.
      if (is_apps_set_)
        return;
    }

    VLOG(1) << "ServicesCustomization extension loader publishing "
            << apps_.size() << " apps.";
    LoadFinished(apps_.Clone());
  }

  base::WeakPtr<ServicesCustomizationExternalLoader> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  ~ServicesCustomizationExternalLoader() override {}

 private:
  bool is_apps_set_ = false;
  base::Value::Dict apps_;
  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<ServicesCustomizationExternalLoader> weak_ptr_factory_{
      this};
};

// CustomizationDocument implementation. ---------------------------------------

CustomizationDocument::CustomizationDocument(
    const std::string& accepted_version)
    : accepted_version_(accepted_version) {}

CustomizationDocument::~CustomizationDocument() {}

bool CustomizationDocument::LoadManifestFromFile(
    const base::FilePath& manifest_path) {
  std::string manifest;
  if (!base::ReadFileToString(manifest_path, &manifest))
    return false;
  return LoadManifestFromString(manifest);
}

bool CustomizationDocument::LoadManifestFromString(
    const std::string& manifest) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      manifest,
      base::JSON_ALLOW_TRAILING_COMMAS | base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!parsed_json.has_value()) {
    LOG(ERROR) << parsed_json.error().message;
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  if (!parsed_json->is_dict()) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  root_ =
      std::make_unique<base::Value::Dict>(std::move(*parsed_json).TakeDict());

  const std::string* result = root_->FindString(kVersionAttr);
  if (!result || *result != accepted_version_) {
    LOG(ERROR) << "Wrong customization manifest version";
    root_.reset();
    return false;
  }

  return true;
}

std::string CustomizationDocument::GetLocaleSpecificString(
    const std::string& locale,
    const std::string& dictionary_name,
    const std::string& entry_name) const {
  return GetLocaleSpecificStringImpl(*root_, locale, dictionary_name,
                                     entry_name);
}

// StartupCustomizationDocument implementation. --------------------------------

StartupCustomizationDocument::StartupCustomizationDocument()
    : CustomizationDocument(kAcceptedManifestVersion) {
  {
    // Loading manifest causes us to do blocking IO on UI thread.
    // Temporarily allow it until we fix http://crosbug.com/11103
    base::ScopedAllowBlocking allow_blocking;
    base::FilePath startup_customization_manifest;
    base::PathService::Get(FILE_STARTUP_CUSTOMIZATION_MANIFEST,
                           &startup_customization_manifest);
    LoadManifestFromFile(startup_customization_manifest);
  }
  Init(system::StatisticsProvider::GetInstance());
}

StartupCustomizationDocument::StartupCustomizationDocument(
    system::StatisticsProvider* statistics_provider,
    const std::string& manifest)
    : CustomizationDocument(kAcceptedManifestVersion) {
  LoadManifestFromString(manifest);
  Init(statistics_provider);
}

StartupCustomizationDocument::~StartupCustomizationDocument() {}

StartupCustomizationDocument* StartupCustomizationDocument::GetInstance() {
  return base::Singleton<
      StartupCustomizationDocument,
      base::DefaultSingletonTraits<StartupCustomizationDocument>>::get();
}

void StartupCustomizationDocument::Init(
    system::StatisticsProvider* statistics_provider) {
  if (IsReady()) {
    const std::string* initial_locale_ptr =
        root_->FindString(kInitialLocaleAttr);
    if (initial_locale_ptr)
      initial_locale_ = *initial_locale_ptr;

    const std::string* initial_timezone_ptr =
        root_->FindString(kInitialTimezoneAttr);
    if (initial_timezone_ptr)
      initial_timezone_ = *initial_timezone_ptr;

    const std::string* keyboard_layout_ptr =
        root_->FindString(kKeyboardLayoutAttr);
    if (keyboard_layout_ptr)
      keyboard_layout_ = *keyboard_layout_ptr;

    if (const std::optional<std::string_view> hwid =
            statistics_provider->GetMachineStatistic(
                system::kHardwareClassKey)) {
      base::Value::List* hwid_list = root_->FindList(kHwidMapAttr);
      if (hwid_list) {
        for (const base::Value& hwid_value : *hwid_list) {
          const base::Value::Dict* hwid_dictionary = nullptr;
          if (hwid_value.is_dict())
            hwid_dictionary = &hwid_value.GetDict();

          const std::string* hwid_mask =
              hwid_dictionary ? hwid_dictionary->FindString(kHwidMaskAttr)
                              : nullptr;
          if (hwid_mask) {
            if (base::MatchPattern(hwid.value(), *hwid_mask)) {
              // If HWID for this machine matches some mask, use HWID specific
              // settings.
              const std::string* initial_locale =
                  hwid_dictionary->FindString(kInitialLocaleAttr);
              if (initial_locale)
                initial_locale_ = *initial_locale;

              const std::string* initial_timezone =
                  hwid_dictionary->FindString(kInitialTimezoneAttr);
              if (initial_timezone)
                initial_timezone_ = *initial_timezone;

              const std::string* keyboard_layout =
                  hwid_dictionary->FindString(kKeyboardLayoutAttr);
              if (keyboard_layout)
                keyboard_layout_ = *keyboard_layout;
            }
            // Don't break here to allow other entires to be applied if match.
          } else {
            LOG(ERROR) << "Syntax error in customization manifest";
          }
        }
      }
    } else {
      LOG(ERROR) << "HWID is missing in machine statistics";
    }
  }

  // If manifest doesn't exist still apply values from VPD.
  if (const std::optional<std::string_view> locale_statistic =
          statistics_provider->GetMachineStatistic(system::kInitialLocaleKey)) {
    initial_locale_ = std::string(locale_statistic.value());
  }
  if (const std::optional<std::string_view> timezone_statistic =
          statistics_provider->GetMachineStatistic(
              system::kInitialTimezoneKey)) {
    initial_timezone_ = std::string(timezone_statistic.value());
  }
  if (const std::optional<std::string_view> keyboard_statistic =
          statistics_provider->GetMachineStatistic(
              system::kKeyboardLayoutKey)) {
    keyboard_layout_ = std::string(keyboard_statistic.value());
  }
  configured_locales_ = base::SplitString(
      initial_locale_, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Convert ICU locale to chrome ("en_US" to "en-US", etc.).
  base::ranges::for_each(configured_locales_, base::i18n::GetCanonicalLocale);

  // Let's always have configured_locales_.front() a valid entry.
  if (configured_locales_.size() == 0)
    configured_locales_.push_back(std::string());
}

const std::vector<std::string>&
StartupCustomizationDocument::configured_locales() const {
  return configured_locales_;
}

const std::string& StartupCustomizationDocument::initial_locale_default()
    const {
  DCHECK_GT(configured_locales_.size(), 0UL);
  return configured_locales_.front();
}

std::string StartupCustomizationDocument::GetEULAPage(
    const std::string& locale) const {
  return GetLocaleSpecificString(locale, kSetupContentAttr, kEulaPageAttr);
}

// ServicesCustomizationDocument implementation. -------------------------------

class ServicesCustomizationDocument::ApplyingTask {
 public:
  // Registers in ServicesCustomizationDocument;
  explicit ApplyingTask(ServicesCustomizationDocument* document);

  // Do not automatically deregister as we might be called on invalid thread.
  ~ApplyingTask();

  // Mark task finished and check for customization applied.
  void Finished(bool success);

 private:
  raw_ptr<ServicesCustomizationDocument> document_;

  // This is error-checking flag to prevent destroying unfinished task
  // or double finish.
  bool engaged_;
};

ServicesCustomizationDocument::ApplyingTask::ApplyingTask(
    ServicesCustomizationDocument* document)
    : document_(document), engaged_(true) {
  document->ApplyingTaskStarted();
}

ServicesCustomizationDocument::ApplyingTask::~ApplyingTask() {
  DCHECK(!engaged_);
}

void ServicesCustomizationDocument::ApplyingTask::Finished(bool success) {
  DCHECK(engaged_);
  if (engaged_) {
    engaged_ = false;
    document_->ApplyingTaskFinished(success);
  }
}

ServicesCustomizationDocument::ServicesCustomizationDocument()
    : CustomizationDocument(kAcceptedManifestVersion),
      num_retries_(0),
      load_started_(false),
      apply_tasks_started_(0),
      apply_tasks_finished_(0),
      apply_tasks_success_(0) {}

ServicesCustomizationDocument::ServicesCustomizationDocument(
    const std::string& manifest)
    : CustomizationDocument(kAcceptedManifestVersion),
      apply_tasks_started_(0),
      apply_tasks_finished_(0),
      apply_tasks_success_(0) {
  LoadManifestFromString(manifest);
}

ServicesCustomizationDocument::~ServicesCustomizationDocument() = default;

// static
ServicesCustomizationDocument* ServicesCustomizationDocument::GetInstance() {
  if (g_test_overrides)
    return g_test_overrides->customization_document;

  return base::Singleton<
      ServicesCustomizationDocument,
      base::DefaultSingletonTraits<ServicesCustomizationDocument>>::get();
}

// static
void ServicesCustomizationDocument::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kServicesCustomizationAppliedPref, false);
  registry->RegisterStringPref(prefs::kCustomizationDefaultWallpaperURL,
                               std::string());
}

// static
void ServicesCustomizationDocument::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kServicesCustomizationKey);
}

// static
bool ServicesCustomizationDocument::WasOOBECustomizationApplied() {
  PrefService* prefs = g_browser_process->local_state();
  // prefs can be NULL in some tests.
  if (prefs)
    return prefs->GetBoolean(kServicesCustomizationAppliedPref);
  else
    return false;
}

// static
void ServicesCustomizationDocument::SetApplied(bool val) {
  PrefService* prefs = g_browser_process->local_state();
  // prefs can be NULL in some tests.
  if (prefs)
    prefs->SetBoolean(kServicesCustomizationAppliedPref, val);
}

// static
base::FilePath ServicesCustomizationDocument::GetCustomizedWallpaperCacheDir() {
  base::FilePath custom_wallpaper_dir;
  if (!base::PathService::Get(chrome::DIR_CHROMEOS_CUSTOM_WALLPAPERS,
                              &custom_wallpaper_dir)) {
    LOG(DFATAL) << "Unable to get custom wallpaper dir.";
    return base::FilePath();
  }
  return custom_wallpaper_dir.Append(kCustomizationDefaultWallpaperDir);
}

// static
base::FilePath
ServicesCustomizationDocument::GetCustomizedWallpaperDownloadedFileName() {
  const base::FilePath dir = GetCustomizedWallpaperCacheDir();
  if (dir.empty()) {
    NOTREACHED_IN_MIGRATION();
    return dir;
  }
  return dir.Append(kCustomizationDefaultWallpaperDownloadedFile);
}

void ServicesCustomizationDocument::EnsureCustomizationApplied() {
  if (WasOOBECustomizationApplied())
    return;

  // When customization manifest is fetched, applying will start automatically.
  if (IsReady())
    return;

  StartFetching();
}

base::OnceClosure
ServicesCustomizationDocument::EnsureCustomizationAppliedClosure() {
  return base::BindOnce(
      &ServicesCustomizationDocument::EnsureCustomizationApplied,
      weak_ptr_factory_.GetWeakPtr());
}

void ServicesCustomizationDocument::StartFetching() {
  if (IsReady() || load_started_)
    return;

  if (!url_.is_valid()) {
    system::StatisticsProvider* provider =
        system::StatisticsProvider::GetInstance();
    const std::optional<std::string_view> customization_id =
        provider->GetMachineStatistic(system::kCustomizationIdKey);
    if (customization_id && !customization_id->empty()) {
      url_ = GURL(base::StringPrintf(
          kManifestUrl, base::ToLowerASCII(customization_id.value()).c_str()));
    } else {
      // Remember that there is no customization ID in VPD.
      OnCustomizationNotFound();
      return;
    }
  }

  if (url_.is_valid()) {
    load_started_ = true;
    if (url_.SchemeIsFile()) {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
          base::BindOnce(&ReadFileInBackground, base::FilePath(url_.path())),
          base::BindOnce(&ServicesCustomizationDocument::OnManifestRead,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      StartFileFetch();
    }
  }
}

void ServicesCustomizationDocument::OnManifestRead(
    const std::string& manifest) {
  if (!manifest.empty())
    LoadManifestFromString(manifest);

  load_started_ = false;
}

void ServicesCustomizationDocument::StartFileFetch() {
  if (custom_network_delay_) {
    DelayNetworkCallWithCustomDelay(
        base::BindOnce(&ServicesCustomizationDocument::DoStartFileFetch,
                       weak_ptr_factory_.GetWeakPtr()),
        custom_network_delay_.value());
  } else {
    DelayNetworkCall(
        base::BindOnce(&ServicesCustomizationDocument::DoStartFileFetch,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ServicesCustomizationDocument::DoStartFileFetch() {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url_;
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->headers.SetHeader("Accept", "application/json");

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(request), kCustomizationDocumentNetworkTag);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      g_test_overrides ? g_test_overrides->url_loader_factory.get()
                       : g_browser_process->shared_url_loader_factory().get(),
      base::BindOnce(&ServicesCustomizationDocument::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

bool ServicesCustomizationDocument::LoadManifestFromString(
    const std::string& manifest) {
  if (CustomizationDocument::LoadManifestFromString(manifest)) {
    OnManifestLoaded();
    return true;
  }

  return false;
}

void ServicesCustomizationDocument::OnManifestLoaded() {
  if (!WasOOBECustomizationApplied())
    ApplyOOBECustomization();

  auto prefs = GetDefaultAppsInProviderFormat(*root_);
  for (auto& external_loader : external_loaders_) {
    if (external_loader) {
      UpdateCachedManifest(external_loader->profile());
      external_loader->SetCurrentApps(prefs.Clone());
      SetOemFolderName(external_loader->profile(), *root_);
    }
  }
}

void ServicesCustomizationDocument::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  std::string mime_type;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
    url_loader_->ResponseInfo()->headers->GetMimeType(&mime_type);
  }

  if (response_body && mime_type == "application/json") {
    LoadManifestFromString(*response_body);
  } else if (response_code == net::HTTP_NOT_FOUND) {
    LOG(ERROR) << "Customization manifest is missing on server: "
               << url_.spec();
    OnCustomizationNotFound();
  } else {
    if (num_retries_ < kMaxFetchRetries) {
      num_retries_++;
      content::GetUIThreadTaskRunner({})->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ServicesCustomizationDocument::StartFileFetch,
                         weak_ptr_factory_.GetWeakPtr()),
          base::Seconds(kRetriesDelayInSec));
      return;
    }
    // This doesn't stop fetching manifest on next restart.
    LOG(ERROR) << "URL fetch for services customization failed:"
               << " response code = " << response_code
               << " URL = " << url_.spec();

  }
  load_started_ = false;
}

bool ServicesCustomizationDocument::ApplyOOBECustomization() {
  if (apply_tasks_started_)
    return false;

  CheckAndApplyWallpaper();
  return false;
}

bool ServicesCustomizationDocument::GetDefaultWallpaperUrl(
    GURL* out_url) const {
  if (!IsReady())
    return false;

  const std::string* url = root_->FindString(kDefaultWallpaperAttr);
  if (!url)
    return false;

  *out_url = GURL(*url);
  return true;
}

std::optional<base::Value::Dict> ServicesCustomizationDocument::GetDefaultApps()
    const {
  if (!IsReady())
    return std::nullopt;

  return GetDefaultAppsInProviderFormat(*root_);
}

std::string ServicesCustomizationDocument::GetOemAppsFolderName(
    const std::string& locale) const {
  if (!IsReady())
    return std::string();

  return GetOemAppsFolderNameImpl(locale, *root_);
}

base::Value::Dict ServicesCustomizationDocument::GetDefaultAppsInProviderFormat(
    const base::Value::Dict& root) {
  base::Value::Dict prefs;
  const base::Value::List* apps_list = root.FindList(kDefaultAppsAttr);
  if (apps_list) {
    for (const base::Value& app_entry_value : *apps_list) {
      std::string app_id;
      base::Value::Dict entry;
      if (app_entry_value.is_string()) {
        app_id = app_entry_value.GetString();
      } else if (app_entry_value.is_dict()) {
        const base::Value::Dict& app_entry = app_entry_value.GetDict();
        const std::string* app_id_ptr = app_entry.FindString(kIdAttr);
        if (!app_id_ptr) {
          LOG(ERROR) << "Wrong format of default application list";
          prefs.clear();
          break;
        }
        app_id = *app_id_ptr;
        entry = app_entry.Clone();
        entry.Remove(kIdAttr);
      } else {
        LOG(ERROR) << "Wrong format of default application list";
        prefs.clear();
        break;
      }
      if (!entry.Find(extensions::ExternalProviderImpl::kExternalUpdateUrl)) {
        entry.Set(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                  extension_urls::GetWebstoreUpdateUrl().spec());
      }
      prefs.SetByDottedPath(app_id, std::move(entry));
    }
  }

  return prefs;
}

void ServicesCustomizationDocument::UpdateCachedManifest(Profile* profile) {
  profile->GetPrefs()->SetDict(kServicesCustomizationKey, root_->Clone());
}

extensions::ExternalLoader* ServicesCustomizationDocument::CreateExternalLoader(
    Profile* profile) {
  ServicesCustomizationExternalLoader* loader =
      new ServicesCustomizationExternalLoader(profile);
  external_loaders_.push_back(loader->AsWeakPtr());

  if (IsReady()) {
    UpdateCachedManifest(profile);
    loader->SetCurrentApps(GetDefaultAppsInProviderFormat(*root_));
    SetOemFolderName(profile, *root_);
  } else {
    const base::Value::Dict& root =
        profile->GetPrefs()->GetDict(kServicesCustomizationKey);
    if (root.FindString(kVersionAttr)) {
      // If version exists, profile has cached version of customization.
      loader->SetCurrentApps(GetDefaultAppsInProviderFormat(root));
      SetOemFolderName(profile, root);
    } else {
      // StartFetching will be called from ServicesCustomizationExternalLoader
      // when StartLoading is called. We can't initiate manifest fetch here
      // because caller may never call StartLoading for the provider.
    }
  }

  return loader;
}

void ServicesCustomizationDocument::OnCustomizationNotFound() {
  LoadManifestFromString(kEmptyServicesCustomizationManifest);
}

void ServicesCustomizationDocument::SetOemFolderName(
    Profile* profile,
    const base::Value::Dict& root) {
  std::string locale = g_browser_process->GetApplicationLocale();
  std::string name = GetOemAppsFolderNameImpl(locale, root);
  if (name.empty())
    name = chromeos::default_app_order::GetOemAppsFolderName();
  if (!name.empty()) {
    app_list::AppListSyncableService* service =
        app_list::AppListSyncableServiceFactory::GetForProfile(profile);
    if (!service) {
      LOG(WARNING) << "AppListSyncableService is not ready for setting OEM "
                      "folder name";
      return;
    }
    service->SetOemFolderName(name);
  }
}

std::string ServicesCustomizationDocument::GetOemAppsFolderNameImpl(
    const std::string& locale,
    const base::Value::Dict& root) const {
  return GetLocaleSpecificStringImpl(root, locale, kLocalizedContent,
                                     kDefaultAppsFolderName);
}

// static
void ServicesCustomizationDocument::InitializeForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  g_test_overrides = new CustomizationDocumentTestOverride;
  g_test_overrides->customization_document = new ServicesCustomizationDocument;
  // `base::TimeDelta()` means zero time delta - i.e. the request will be
  // started immediately.
  g_test_overrides->customization_document->custom_network_delay_ =
      std::make_optional(base::TimeDelta());
  g_test_overrides->url_loader_factory = std::move(factory);
}

// static
void ServicesCustomizationDocument::ShutdownForTesting() {
  delete g_test_overrides->customization_document;
  delete g_test_overrides;
  g_test_overrides = nullptr;
}

void ServicesCustomizationDocument::StartOEMWallpaperDownload(
    const GURL& wallpaper_url,
    std::unique_ptr<ServicesCustomizationDocument::ApplyingTask> applying) {
  DCHECK(wallpaper_url.is_valid());

  const base::FilePath dir = GetCustomizedWallpaperCacheDir();
  const base::FilePath file = GetCustomizedWallpaperDownloadedFileName();
  if (dir.empty() || file.empty()) {
    NOTREACHED_IN_MIGRATION();
    applying->Finished(false);
    return;
  }

  wallpaper_downloader_ = std::make_unique<CustomizationWallpaperDownloader>(
      wallpaper_url, dir, file,
      base::BindOnce(&ServicesCustomizationDocument::OnOEMWallpaperDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(applying)));

  wallpaper_downloader_->Start();
}

void ServicesCustomizationDocument::CheckAndApplyWallpaper() {
  if (wallpaper_downloader_.get()) {
    VLOG(1) << "CheckAndApplyWallpaper(): download has already started.";
    return;
  }
  std::unique_ptr<ServicesCustomizationDocument::ApplyingTask> applying(
      new ServicesCustomizationDocument::ApplyingTask(this));

  GURL wallpaper_url;
  if (!GetDefaultWallpaperUrl(&wallpaper_url)) {
    PrefService* pref_service = g_browser_process->local_state();
    std::string current_url =
        pref_service->GetString(prefs::kCustomizationDefaultWallpaperURL);
    if (!current_url.empty()) {
      VLOG(1) << "ServicesCustomizationDocument::CheckAndApplyWallpaper() : "
              << "No wallpaper URL attribute in customization document, "
              << "but current value is non-empty: '" << current_url
              << "'. Ignored.";
    }
    applying->Finished(true);
    return;
  }

  // Should fail if this ever happens in tests.
  DCHECK(wallpaper_url.is_valid());
  if (!wallpaper_url.is_valid()) {
    if (!wallpaper_url.is_empty()) {
      LOG(WARNING) << "Invalid Customized Wallpaper URL '"
                   << wallpaper_url.spec() << "'.";
    }
    applying->Finished(false);
    return;
  }

  std::unique_ptr<bool> exists(new bool(false));

  base::OnceClosure check_file_exists = base::BindOnce(
      &CheckWallpaperCacheExists, GetCustomizedWallpaperDownloadedFileName(),
      base::Unretained(exists.get()));
  base::OnceClosure on_checked_closure = base::BindOnce(
      &ServicesCustomizationDocument::OnCheckedWallpaperCacheExists,
      weak_ptr_factory_.GetWeakPtr(), std::move(exists), std::move(applying));
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      std::move(check_file_exists), std::move(on_checked_closure));
}

void ServicesCustomizationDocument::OnCheckedWallpaperCacheExists(
    std::unique_ptr<bool> exists,
    std::unique_ptr<ServicesCustomizationDocument::ApplyingTask> applying) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(exists);
  DCHECK(applying);

  ApplyWallpaper(*exists, std::move(applying));
}

void ServicesCustomizationDocument::ApplyWallpaper(
    bool default_wallpaper_file_exists,
    std::unique_ptr<ServicesCustomizationDocument::ApplyingTask> applying) {
  GURL wallpaper_url;
  const bool wallpaper_url_present = GetDefaultWallpaperUrl(&wallpaper_url);

  PrefService* pref_service = g_browser_process->local_state();

  std::string current_url =
      pref_service->GetString(prefs::kCustomizationDefaultWallpaperURL);
  if (current_url != wallpaper_url.spec()) {
    if (wallpaper_url_present) {
      VLOG(1) << "ServicesCustomizationDocument::ApplyWallpaper() : "
              << "Wallpaper URL in customization document '"
              << wallpaper_url.spec() << "' differs from current '"
              << current_url << "'."
              << (GURL(current_url).is_valid() && default_wallpaper_file_exists
                      ? " Ignored."
                      : " Will refetch.");
    } else {
      VLOG(1) << "ServicesCustomizationDocument::ApplyWallpaper() : "
              << "No wallpaper URL attribute in customization document, "
              << "but current value is non-empty: '" << current_url
              << "'. Ignored.";
    }
  }
  if (!wallpaper_url_present) {
    applying->Finished(true);
    return;
  }

  DCHECK(wallpaper_url.is_valid());

  // Never update system-wide wallpaper (i.e. do not check
  // current_url == wallpaper_url.spec() )
  if (GURL(current_url).is_valid() && default_wallpaper_file_exists) {
    VLOG(1)
        << "ServicesCustomizationDocument::ApplyWallpaper() : reuse existing";
    OnOEMWallpaperDownloaded(std::move(applying), true, GURL(current_url));
  } else {
    VLOG(1)
        << "ServicesCustomizationDocument::ApplyWallpaper() : start download";
    StartOEMWallpaperDownload(wallpaper_url, std::move(applying));
  }
}

void ServicesCustomizationDocument::OnOEMWallpaperDownloaded(
    std::unique_ptr<ServicesCustomizationDocument::ApplyingTask> applying,
    bool success,
    const GURL& wallpaper_url) {
  if (success) {
    DCHECK(wallpaper_url.is_valid());

    VLOG(1) << "Setting default wallpaper to '"
            << GetCustomizedWallpaperDownloadedFileName().value() << "' ('"
            << wallpaper_url.spec() << "')";
    customization_wallpaper_util::StartSettingCustomizedDefaultWallpaper(
        wallpaper_url, GetCustomizedWallpaperDownloadedFileName());
  }
  wallpaper_downloader_.reset();
  applying->Finished(success);
}

void ServicesCustomizationDocument::ApplyingTaskStarted() {
  ++apply_tasks_started_;
}

void ServicesCustomizationDocument::ApplyingTaskFinished(bool success) {
  DCHECK_GT(apply_tasks_started_, apply_tasks_finished_);
  ++apply_tasks_finished_;

  apply_tasks_success_ += success;

  if (apply_tasks_started_ != apply_tasks_finished_)
    return;

  if (apply_tasks_success_ == apply_tasks_finished_)
    SetApplied(true);
}

}  // namespace ash

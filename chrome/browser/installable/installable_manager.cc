// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_manager.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/common/manifest/web_display_mode.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/shortcut_helper.h"
#endif

namespace {

const char kPngExtension[] = ".png";

// This constant is the icon size on Android (48dp) multiplied by the scale
// factor of a Nexus 5 device (3x). It is the currently advertised minimum icon
// size for triggering banners.
const int kMinimumPrimaryIconSizeInPx = 144;

#if !defined(OS_ANDROID)
const int kMinimumBadgeIconSizeInPx = 72;
#endif

int GetIdealPrimaryIconSizeInPx() {
#if defined(OS_ANDROID)
  return ShortcutHelper::GetIdealHomescreenIconSizeInPx();
#else
  return kMinimumPrimaryIconSizeInPx;
#endif
}

int GetMinimumPrimaryIconSizeInPx() {
#if defined(OS_ANDROID)
  return ShortcutHelper::GetMinimumHomescreenIconSizeInPx();
#else
  return kMinimumPrimaryIconSizeInPx;
#endif
}

int GetIdealBadgeIconSizeInPx() {
#if defined(OS_ANDROID)
  return ShortcutHelper::GetIdealBadgeIconSizeInPx();
#else
  return kMinimumBadgeIconSizeInPx;
#endif
}

// Returns true if the overall security state of |web_contents| is sufficient to
// be considered installable.
bool IsContentSecure(content::WebContents* web_contents) {
  if (!web_contents)
    return false;

  // chrome:// URLs are considered secure.
  if (web_contents->GetVisibleURL().scheme() == content::kChromeUIScheme)
    return true;

  // Whitelist localhost. Check the VisibleURL to match what the
  // SecurityStateTabHelper looks at.
  if (net::IsLocalhost(web_contents->GetVisibleURL()))
    return true;

  security_state::SecurityInfo security_info;
  SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityInfo(&security_info);
  return security_state::IsSslCertificateValid(security_info.security_level);
}

// Returns true if |manifest| specifies a PNG icon with IconPurpose::ANY and of
// height and width >= kMinimumPrimaryIconSizeInPx (or size "any").
bool DoesManifestContainRequiredIcon(const blink::Manifest& manifest) {
  for (const auto& icon : manifest.icons) {
    // The type field is optional. If it isn't present, fall back on checking
    // the src extension, and allow the icon if the extension ends with png.
    if (!base::EqualsASCII(icon.type, "image/png") &&
        !(icon.type.empty() && base::EndsWith(
            icon.src.ExtractFileName(), kPngExtension,
            base::CompareCase::INSENSITIVE_ASCII)))
      continue;

    if (!base::ContainsValue(icon.purpose,
                             blink::Manifest::ImageResource::Purpose::ANY)) {
      continue;
    }

    for (const auto& size : icon.sizes) {
      if (size.IsEmpty())  // "any"
        return true;
      if (size.width() >= kMinimumPrimaryIconSizeInPx &&
          size.height() >= kMinimumPrimaryIconSizeInPx) {
        return true;
      }
    }
  }

  return false;
}

// Returns true if |params| specifies a full PWA check.
bool IsParamsForPwaCheck(const InstallableParams& params) {
  return params.valid_manifest && params.has_worker &&
         params.valid_primary_icon;
}

}  // namespace

InstallableManager::IconProperty::IconProperty()
    : error(NO_ERROR_DETECTED), url(), icon(), fetched(false) {}

InstallableManager::IconProperty::IconProperty(IconProperty&& other) = default;

InstallableManager::IconProperty::~IconProperty() {}

InstallableManager::IconProperty& InstallableManager::IconProperty::operator=(
    InstallableManager::IconProperty&& other) = default;

InstallableManager::InstallableManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      metrics_(std::make_unique<InstallableMetrics>()),
      eligibility_(std::make_unique<EligiblityProperty>()),
      manifest_(std::make_unique<ManifestProperty>()),
      valid_manifest_(std::make_unique<ValidManifestProperty>()),
      worker_(std::make_unique<ServiceWorkerProperty>()),
      service_worker_context_(nullptr),
      has_pwa_check_(false),
      weak_factory_(this) {
  // This is null in unit tests.
  if (web_contents) {
    content::StoragePartition* storage_partition =
        content::BrowserContext::GetStoragePartition(
            Profile::FromBrowserContext(web_contents->GetBrowserContext()),
            web_contents->GetSiteInstance());
    DCHECK(storage_partition);

    service_worker_context_ = storage_partition->GetServiceWorkerContext();
    service_worker_context_->AddObserver(this);
  }
}

InstallableManager::~InstallableManager() {
  // Null in unit tests.
  if (service_worker_context_)
    service_worker_context_->RemoveObserver(this);
}

// static
int InstallableManager::GetMinimumIconSizeInPx() {
  return kMinimumPrimaryIconSizeInPx;
}

void InstallableManager::GetData(const InstallableParams& params,
                                 const InstallableCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (IsParamsForPwaCheck(params))
    has_pwa_check_ = true;

  // Return immediately if we're already working on a task. The new task will be
  // looked at once the current task is finished.
  bool was_active = task_queue_.HasCurrent();
  task_queue_.Add({params, callback});
  if (was_active)
    return;

  metrics_->Start();
  WorkOnTask();
}

void InstallableManager::RecordMenuOpenHistogram() {
  metrics_->RecordMenuOpen();
}

void InstallableManager::RecordMenuItemAddToHomescreenHistogram() {
  metrics_->RecordMenuItemAddToHomescreen();
}

void InstallableManager::RecordAddToHomescreenNoTimeout() {
  metrics_->RecordAddToHomescreenNoTimeout();
}

void InstallableManager::RecordAddToHomescreenManifestAndIconTimeout() {
  metrics_->RecordAddToHomescreenManifestAndIconTimeout();

  // If needed, explicitly trigger GetData() with a no-op callback to complete
  // the installability check. This is so we can accurately record whether or
  // not a site is a PWA, assuming that the check finishes prior to resetting.
  if (!has_pwa_check_) {
    InstallableParams params;
    params.valid_manifest = true;
    params.has_worker = true;
    params.valid_primary_icon = true;
    params.wait_for_worker = true;
    GetData(params, base::DoNothing());
  }
}

void InstallableManager::RecordAddToHomescreenInstallabilityTimeout() {
  metrics_->RecordAddToHomescreenInstallabilityTimeout();
}

bool InstallableManager::IsIconFetched(const IconPurpose purpose) const {
  const auto it = icons_.find(purpose);
  return it != icons_.end() && it->second.fetched;
}

void InstallableManager::SetIconFetched(const IconPurpose purpose) {
  icons_[purpose].fetched = true;
}

InstallableStatusCode InstallableManager::GetErrorCode(
    const InstallableParams& params) {
  if (params.check_eligibility && eligibility_->error != NO_ERROR_DETECTED)
    return eligibility_->error;

  if (manifest_->error != NO_ERROR_DETECTED)
    return manifest_->error;

  if (params.valid_manifest && valid_manifest_->error != NO_ERROR_DETECTED)
    return valid_manifest_->error;

  if (params.has_worker && worker_->error != NO_ERROR_DETECTED)
    return worker_->error;

  if (params.valid_primary_icon) {
    IconProperty& icon = icons_[IconPurpose::ANY];
    if (icon.error != NO_ERROR_DETECTED)
      return icon.error;
  }

  if (params.valid_badge_icon) {
    IconProperty& icon = icons_[IconPurpose::BADGE];

    // If the error is NO_ACCEPTABLE_ICON, there is no icon suitable as a badge
    // in the manifest. Ignore this case since we only want to fail the check if
    // there was a suitable badge icon specified and we couldn't fetch it.
    if (icon.error != NO_ERROR_DETECTED && icon.error != NO_ACCEPTABLE_ICON)
      return icon.error;
  }

  return NO_ERROR_DETECTED;
}

InstallableStatusCode InstallableManager::eligibility_error() const {
  return eligibility_->error;
}

InstallableStatusCode InstallableManager::manifest_error() const {
  return manifest_->error;
}

InstallableStatusCode InstallableManager::valid_manifest_error() const {
  return valid_manifest_->error;
}

void InstallableManager::set_valid_manifest_error(
    InstallableStatusCode error_code) {
  valid_manifest_->error = error_code;
}

InstallableStatusCode InstallableManager::worker_error() const {
  return worker_->error;
}

InstallableStatusCode InstallableManager::icon_error(
    const IconPurpose purpose) {
  return icons_[purpose].error;
}

GURL& InstallableManager::icon_url(const IconPurpose purpose) {
  return icons_[purpose].url;
}

const SkBitmap* InstallableManager::icon(const IconPurpose purpose) {
  return icons_[purpose].icon.get();
}

content::WebContents* InstallableManager::GetWebContents() {
  content::WebContents* contents = web_contents();
  if (!contents || contents->IsBeingDestroyed())
    return nullptr;
  return contents;
}

bool InstallableManager::IsComplete(const InstallableParams& params) const {
  // Returns true if for all resources:
  //  a. the params did not request it, OR
  //  b. the resource has been fetched/checked.
  return (!params.check_eligibility || eligibility_->fetched) &&
         manifest_->fetched &&
         (!params.valid_manifest || valid_manifest_->fetched) &&
         (!params.has_worker || worker_->fetched) &&
         (!params.valid_primary_icon || IsIconFetched(IconPurpose::ANY)) &&
         (!params.valid_badge_icon || IsIconFetched(IconPurpose::BADGE));
}

void InstallableManager::ResolveMetrics(const InstallableParams& params,
                                        bool check_passed) {
  // Don't do anything if we passed the check AND it was not for the full PWA
  // params. We don't yet know if the site is installable. However, if the check
  // didn't pass, we know for sure the site isn't installable, regardless of how
  // much we checked.
  if (check_passed && !IsParamsForPwaCheck(params))
    return;

  metrics_->Resolve(check_passed);
}

void InstallableManager::Reset() {
  // Prevent any outstanding callbacks to or from this object from being called.
  weak_factory_.InvalidateWeakPtrs();
  icons_.clear();

  // If we have paused tasks, we are waiting for a service worker.
  metrics_->Flush(task_queue_.HasPaused());
  task_queue_.Reset();
  has_pwa_check_ = false;

  metrics_ = std::make_unique<InstallableMetrics>();
  eligibility_ = std::make_unique<EligiblityProperty>();
  manifest_ = std::make_unique<ManifestProperty>();
  valid_manifest_ = std::make_unique<ValidManifestProperty>();
  worker_ = std::make_unique<ServiceWorkerProperty>();

  OnResetData();
}

void InstallableManager::SetManifestDependentTasksComplete() {
  valid_manifest_->fetched = true;
  worker_->fetched = true;
  SetIconFetched(IconPurpose::ANY);
  SetIconFetched(IconPurpose::BADGE);
}

void InstallableManager::RunCallback(const InstallableTask& task,
                                     InstallableStatusCode code) {
  const InstallableParams& params = task.params;
  IconProperty null_icon;
  IconProperty* primary_icon = &null_icon;
  IconProperty* badge_icon = &null_icon;

  if (params.valid_primary_icon && base::ContainsKey(icons_, IconPurpose::ANY))
    primary_icon = &icons_[IconPurpose::ANY];
  if (params.valid_badge_icon && base::ContainsKey(icons_, IconPurpose::BADGE))
    badge_icon = &icons_[IconPurpose::BADGE];

  InstallableData data = {
      code,
      manifest_url(),
      &manifest(),
      primary_icon->url,
      primary_icon->icon.get(),
      badge_icon->url,
      badge_icon->icon.get(),
      valid_manifest_->is_valid,
      worker_->has_worker,
  };

  task.callback.Run(data);
}

void InstallableManager::WorkOnTask() {
  const InstallableTask& task = task_queue_.Current();
  const InstallableParams& params = task.params;

  InstallableStatusCode code = GetErrorCode(params);
  bool check_passed = (code == NO_ERROR_DETECTED);
  if (!check_passed || IsComplete(params)) {
    ResolveMetrics(params, check_passed);
    RunCallback(task, code);

    // Sites can always register a service worker after we finish checking, so
    // don't cache a missing service worker error to ensure we always check
    // again.
    if (worker_error() == NO_MATCHING_SERVICE_WORKER)
      worker_ = std::make_unique<ServiceWorkerProperty>();

    task_queue_.Next();

    if (task_queue_.HasCurrent())
      WorkOnTask();

    return;
  }

  if (params.check_eligibility && !eligibility_->fetched) {
    CheckEligiblity();
  } else if (!manifest_->fetched) {
    FetchManifest();
  } else if (params.valid_primary_icon && !IsIconFetched(IconPurpose::ANY)) {
    CheckAndFetchBestIcon(GetIdealPrimaryIconSizeInPx(),
                          GetMinimumPrimaryIconSizeInPx(), IconPurpose::ANY);
  } else if (params.valid_manifest && !valid_manifest_->fetched) {
    CheckManifestValid();
  } else if (params.has_worker && !worker_->fetched) {
    CheckServiceWorker();
  } else if (params.valid_badge_icon && !IsIconFetched(IconPurpose::BADGE)) {
    CheckAndFetchBestIcon(GetIdealBadgeIconSizeInPx(),
                          GetIdealBadgeIconSizeInPx(), IconPurpose::BADGE);
  } else {
    NOTREACHED();
  }
}

void InstallableManager::CheckEligiblity() {
  // Fail if this is an incognito window, non-main frame, or insecure context.
  content::WebContents* web_contents = GetWebContents();
  if (Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->IsOffTheRecord()) {
    eligibility_->error = IN_INCOGNITO;
  } else if (web_contents->GetMainFrame()->GetParent()) {
    eligibility_->error = NOT_IN_MAIN_FRAME;
  } else if (!IsContentSecure(web_contents)) {
    eligibility_->error = NOT_FROM_SECURE_ORIGIN;
  }

  eligibility_->fetched = true;
  WorkOnTask();
}

void InstallableManager::FetchManifest() {
  DCHECK(!manifest_->fetched);

  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  web_contents->GetManifest(base::Bind(&InstallableManager::OnDidGetManifest,
                                       weak_factory_.GetWeakPtr()));
}

void InstallableManager::OnDidGetManifest(const GURL& manifest_url,
                                          const blink::Manifest& manifest) {
  if (!GetWebContents())
    return;

  if (manifest_url.is_empty()) {
    manifest_->error = NO_MANIFEST;
    SetManifestDependentTasksComplete();
  } else if (manifest.IsEmpty()) {
    manifest_->error = MANIFEST_EMPTY;
    SetManifestDependentTasksComplete();
  }

  manifest_->url = manifest_url;
  manifest_->manifest = manifest;
  manifest_->fetched = true;
  WorkOnTask();
}

void InstallableManager::CheckManifestValid() {
  DCHECK(!valid_manifest_->fetched);
  DCHECK(!manifest().IsEmpty());

  valid_manifest_->is_valid = IsManifestValidForWebApp(manifest());
  valid_manifest_->fetched = true;
  WorkOnTask();
}

bool InstallableManager::IsManifestValidForWebApp(
    const blink::Manifest& manifest) {
  if (manifest.IsEmpty()) {
    valid_manifest_->error = MANIFEST_EMPTY;
    return false;
  }

  if (!manifest.start_url.is_valid()) {
    valid_manifest_->error = START_URL_NOT_VALID;
    return false;
  }

  if ((manifest.name.is_null() || manifest.name.string().empty()) &&
      (manifest.short_name.is_null() || manifest.short_name.string().empty())) {
    valid_manifest_->error = MANIFEST_MISSING_NAME_OR_SHORT_NAME;
    return false;
  }

  if (manifest.display != blink::kWebDisplayModeStandalone &&
      manifest.display != blink::kWebDisplayModeFullscreen &&
      manifest.display != blink::kWebDisplayModeMinimalUi) {
    valid_manifest_->error = MANIFEST_DISPLAY_NOT_SUPPORTED;
    return false;
  }

  if (!DoesManifestContainRequiredIcon(manifest)) {
    valid_manifest_->error = MANIFEST_MISSING_SUITABLE_ICON;
    return false;
  }

  return true;
}

void InstallableManager::CheckServiceWorker() {
  DCHECK(!worker_->fetched);
  DCHECK(!manifest().IsEmpty());
  DCHECK(manifest().start_url.is_valid());

  // Check to see if there is a single service worker controlling this page
  // and the manifest's start url.
  service_worker_context_->CheckHasServiceWorker(
      GetWebContents()->GetLastCommittedURL(), manifest().start_url,
      base::Bind(&InstallableManager::OnDidCheckHasServiceWorker,
                 weak_factory_.GetWeakPtr()));
}

void InstallableManager::OnDidCheckHasServiceWorker(
    content::ServiceWorkerCapability capability) {
  if (!GetWebContents())
    return;

  switch (capability) {
    case content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER:
      worker_->has_worker = true;
      break;
    case content::ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER:
      worker_->has_worker = false;
      worker_->error = NOT_OFFLINE_CAPABLE;
      break;
    case content::ServiceWorkerCapability::NO_SERVICE_WORKER:
      InstallableTask& task = task_queue_.Current();
      if (task.params.wait_for_worker) {
        // Wait for ServiceWorkerContextObserver::OnRegistrationCompleted. Set
        // the param |wait_for_worker| to false so we only wait once per task.
        task.params.wait_for_worker = false;
        OnWaitingForServiceWorker();
        task_queue_.PauseCurrent();
        if (task_queue_.HasCurrent())
          WorkOnTask();

        return;
      }
      worker_->has_worker = false;
      worker_->error = NO_MATCHING_SERVICE_WORKER;
      break;
  }

  worker_->fetched = true;
  WorkOnTask();
}

void InstallableManager::CheckAndFetchBestIcon(int ideal_icon_size_in_px,
                                               int minimum_icon_size_in_px,
                                               const IconPurpose purpose) {
  DCHECK(!manifest().IsEmpty());

  IconProperty& icon = icons_[purpose];
  icon.fetched = true;

  GURL icon_url = blink::ManifestIconSelector::FindBestMatchingIcon(
      manifest().icons, ideal_icon_size_in_px, minimum_icon_size_in_px,
      purpose);

  if (icon_url.is_empty()) {
    icon.error = NO_ACCEPTABLE_ICON;
  } else {
    bool can_download_icon = content::ManifestIconDownloader::Download(
        GetWebContents(), icon_url, ideal_icon_size_in_px,
        minimum_icon_size_in_px,
        base::Bind(&InstallableManager::OnIconFetched,
                   weak_factory_.GetWeakPtr(), icon_url, purpose));
    if (can_download_icon)
      return;
    icon.error = CANNOT_DOWNLOAD_ICON;
  }

  WorkOnTask();
}

void InstallableManager::OnIconFetched(const GURL icon_url,
                                       const IconPurpose purpose,
                                       const SkBitmap& bitmap) {
  IconProperty& icon = icons_[purpose];

  if (!GetWebContents())
    return;

  if (bitmap.drawsNothing()) {
    icon.error = NO_ICON_AVAILABLE;
  } else {
    icon.url = icon_url;
    icon.icon.reset(new SkBitmap(bitmap));
  }

  WorkOnTask();
}

void InstallableManager::OnRegistrationCompleted(const GURL& pattern) {
  // If the scope doesn't match we keep waiting.
  if (!content::ServiceWorkerContext::ScopeMatches(pattern,
                                                   manifest().start_url)) {
    return;
  }

  bool was_active = task_queue_.HasCurrent();

  // The existence of paused tasks implies that we are waiting for a service
  // worker. We move any paused tasks back into the main queue so that the
  // pipeline will call CheckHasServiceWorker again, in order to find out if
  // the SW has a fetch handler.
  // NOTE: If there are no paused tasks, that means:
  //   a) we've already failed the check, or
  //   b) we haven't yet called CheckHasServiceWorker.
  task_queue_.UnpauseAll();
  if (was_active)
    return;  // If the pipeline was already running, we don't restart it.

  if (task_queue_.HasCurrent())
    WorkOnTask();
}

void InstallableManager::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (handle->IsInMainFrame() && handle->HasCommitted() &&
      !handle->IsSameDocument()) {
    Reset();
  }
}

void InstallableManager::DidUpdateWebManifestURL(
    const base::Optional<GURL>& manifest_url) {
  // A change in the manifest URL invalidates our entire internal state.
  Reset();
}

void InstallableManager::WebContentsDestroyed() {
  Reset();
  Observe(nullptr);
}

const GURL& InstallableManager::manifest_url() const {
  return manifest_->url;
}

const blink::Manifest& InstallableManager::manifest() const {
  return manifest_->manifest;
}

bool InstallableManager::valid_manifest() {
  return valid_manifest_->is_valid;
}

bool InstallableManager::has_worker() {
  return worker_->has_worker;
}

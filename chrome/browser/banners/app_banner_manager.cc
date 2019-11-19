// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/installation/installation.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

int gTimeDeltaInDaysForTesting = 0;

InstallableParams ParamsToGetManifest() {
  InstallableParams params;
  params.check_eligibility = true;
  return params;
}

}  // anonymous namespace

namespace banners {

// static
base::Time AppBannerManager::GetCurrentTime() {
  return base::Time::Now() +
         base::TimeDelta::FromDays(gTimeDeltaInDaysForTesting);
}

// static
void AppBannerManager::SetTimeDeltaForTesting(int days) {
  gTimeDeltaInDaysForTesting = days;
}

// static
void AppBannerManager::SetTotalEngagementToTrigger(double engagement) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(engagement);
}

class AppBannerManager::StatusReporter {
 public:
  virtual ~StatusReporter() {}

  // Reports |code| (via a mechanism which depends on the implementation).
  virtual void ReportStatus(InstallableStatusCode code) = 0;

  // Returns the WebappInstallSource to be used for this installation.
  virtual WebappInstallSource GetInstallSource(
      content::WebContents* web_contents,
      InstallTrigger trigger) = 0;
};

}  // namespace banners

namespace {

// Logs installable status codes to the console.
class ConsoleStatusReporter : public banners::AppBannerManager::StatusReporter {
 public:
  // Constructs a ConsoleStatusReporter which logs to the devtools console
  // attached to |web_contents|.
  explicit ConsoleStatusReporter(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  // Logs an error message corresponding to |code| to the devtools console.
  void ReportStatus(InstallableStatusCode code) override {
    LogErrorToConsole(web_contents_, code);
  }

  WebappInstallSource GetInstallSource(content::WebContents* web_contents,
                                       InstallTrigger trigger) override {
    return WebappInstallSource::DEVTOOLS;
  }

 private:
  content::WebContents* web_contents_;
};

// Tracks installable status codes via an UMA histogram.
class TrackingStatusReporter
    : public banners::AppBannerManager::StatusReporter {
 public:
  TrackingStatusReporter() : done_(false) {}
  ~TrackingStatusReporter() override {}

  // Records code via an UMA histogram.
  void ReportStatus(InstallableStatusCode code) override {
    // We only increment the histogram once per page load (and only if the
    // banner pipeline is triggered).
    if (!done_ && code != NO_ERROR_DETECTED)
      banners::TrackInstallableStatusCode(code);

    done_ = true;
  }

  WebappInstallSource GetInstallSource(content::WebContents* web_contents,
                                       InstallTrigger trigger) override {
    return InstallableMetrics::GetInstallSource(web_contents, trigger);
  }

 private:
  bool done_;
};

class NullStatusReporter : public banners::AppBannerManager::StatusReporter {
 public:
  void ReportStatus(InstallableStatusCode code) override {
    // In general, NullStatusReporter::ReportStatus should not be called.
    // However, it may be called in cases where Stop is called without a
    // preceding call to RequestAppBanner e.g. because the WebContents is being
    // destroyed. In that case, code should always be NO_ERROR_DETECTED.
    DCHECK(code == NO_ERROR_DETECTED);
  }

  WebappInstallSource GetInstallSource(content::WebContents* web_contents,
                                       InstallTrigger trigger) override {
    NOTREACHED();
    return WebappInstallSource::COUNT;
  }
};

}  // anonymous namespace

namespace banners {

void AppBannerManager::RequestAppBanner(const GURL& validated_url) {
  DCHECK_EQ(State::INACTIVE, state_);

  UpdateState(State::ACTIVE);
  if (ShouldBypassEngagementChecks())
    status_reporter_ = std::make_unique<ConsoleStatusReporter>(web_contents());
  else
    status_reporter_ = std::make_unique<TrackingStatusReporter>();

  if (validated_url_.is_empty())
    validated_url_ = validated_url;

  UpdateState(State::FETCHING_MANIFEST);
  manager_->GetData(
      ParamsToGetManifest(),
      base::BindOnce(&AppBannerManager::OnDidGetManifest, GetWeakPtr()));
}

void AppBannerManager::OnInstall(blink::mojom::DisplayMode display) {
  TrackInstallDisplayMode(display);
  mojo::Remote<blink::mojom::InstallationService> installation_service;
  web_contents()->GetMainFrame()->GetRemoteInterfaces()->GetInterface(
      installation_service.BindNewPipeAndPassReceiver());
  DCHECK(installation_service);
  installation_service->OnInstall();

  // App has been installed (possibly by the user), page may no longer request
  // install prompt.
  receiver_.reset();
}

void AppBannerManager::SendBannerAccepted() {
  if (event_.is_bound()) {
    event_->BannerAccepted(GetBannerType());
    event_.reset();
  }
}

void AppBannerManager::SendBannerDismissed() {
  if (event_.is_bound())
    event_->BannerDismissed();

  SendBannerPromptRequest();
}

void AppBannerManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AppBannerManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AppBannerManager::MigrateObserverListForTesting(
    content::WebContents* web_contents) {
  AppBannerManager* existing_manager = FromWebContents(web_contents);
  for (Observer& observer : existing_manager->observer_list_)
    observer.OnAppBannerManagerChanged(this);
  DCHECK(existing_manager->observer_list_.begin() ==
         existing_manager->observer_list_.end())
      << "Old observer list must be empty after transfer to test instance.";
}

bool AppBannerManager::IsPromptAvailableForTesting() const {
  return receiver_.is_bound();
}

AppBannerManager::InstallableWebAppCheckResult
AppBannerManager::GetInstallableWebAppCheckResultForTesting() {
  return installable_web_app_check_result_;
}

AppBannerManager::AppBannerManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      SiteEngagementObserver(SiteEngagementService::Get(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      state_(State::INACTIVE),
      manager_(InstallableManager::FromWebContents(web_contents)),
      has_sufficient_engagement_(false),
      load_finished_(false),
      status_reporter_(std::make_unique<NullStatusReporter>()),
      install_animation_pending_(false),
      installable_web_app_check_result_(
          InstallableWebAppCheckResult::kUnknown) {
  DCHECK(manager_);

  AppBannerSettingsHelper::UpdateFromFieldTrial();
}

AppBannerManager::~AppBannerManager() = default;

bool AppBannerManager::CheckIfShouldShowBanner() {
  if (ShouldBypassEngagementChecks())
    return true;

  InstallableStatusCode code = ShouldShowBannerCode();
  switch (code) {
    case NO_ERROR_DETECTED:
      return true;
    case PREVIOUSLY_BLOCKED:
      banners::TrackDisplayEvent(banners::DISPLAY_EVENT_BLOCKED_PREVIOUSLY);
      break;
    case PREVIOUSLY_IGNORED:
      banners::TrackDisplayEvent(banners::DISPLAY_EVENT_IGNORED_PREVIOUSLY);
      break;
    case PACKAGE_NAME_OR_START_URL_EMPTY:
      break;
    default:
      NOTREACHED();
  }
  Stop(code);
  return false;
}

bool AppBannerManager::ShouldDeferToRelatedApplication() const {
  for (const auto& related_app : manifest_.related_applications) {
    if (manifest_.prefer_related_applications &&
        IsSupportedAppPlatform(related_app.platform.string())) {
      return true;
    }
    if (IsRelatedAppInstalled(related_app))
      return true;
  }
  return false;
}

std::string AppBannerManager::GetAppIdentifier() {
  DCHECK(!manifest_.IsEmpty());
  return manifest_.start_url.spec();
}

base::string16 AppBannerManager::GetAppName() const {
  return manifest_.name.string();
}

std::string AppBannerManager::GetBannerType() {
  return "web";
}

bool AppBannerManager::HasSufficientEngagement() const {
  return has_sufficient_engagement_ || ShouldBypassEngagementChecks();
}

bool AppBannerManager::ShouldBypassEngagementChecks() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kBypassAppBannerEngagementChecks);
}

bool AppBannerManager::IsWebAppConsideredInstalled() {
  return false;
}

bool AppBannerManager::ShouldAllowWebAppReplacementInstall() {
  return false;
}

void AppBannerManager::OnDidGetManifest(const InstallableData& data) {
  UpdateState(State::ACTIVE);
  if (!data.errors.empty()) {
    Stop(data.errors[0]);
    return;
  }

  DCHECK(!data.manifest_url.is_empty());
  DCHECK(!data.manifest->IsEmpty());

  manifest_url_ = data.manifest_url;
  manifest_ = *data.manifest;

  PerformInstallableChecks();
}

InstallableParams AppBannerManager::ParamsToPerformInstallableWebAppCheck() {
  InstallableParams params;
  params.valid_primary_icon = true;
  params.valid_manifest = true;
  params.has_worker = true;
  params.wait_for_worker = true;

  return params;
}

void AppBannerManager::PerformInstallableChecks() {
  PerformInstallableWebAppCheck();
}

void AppBannerManager::PerformInstallableWebAppCheck() {
  if (!CheckIfShouldShowBanner())
    return;

  // Fetch and verify the other required information.
  UpdateState(State::PENDING_INSTALLABLE_CHECK);
  manager_->GetData(
      ParamsToPerformInstallableWebAppCheck(),
      base::BindOnce(&AppBannerManager::OnDidPerformInstallableWebAppCheck,
                     GetWeakPtr()));
}

void AppBannerManager::OnDidPerformInstallableWebAppCheck(
    const InstallableData& data) {
  UpdateState(State::ACTIVE);
  if (data.has_worker && data.valid_manifest)
    TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_REQUESTED);

  auto error = data.errors.empty() ? NO_ERROR_DETECTED : data.errors[0];
  if (error != NO_ERROR_DETECTED) {
    if (error == NO_MATCHING_SERVICE_WORKER)
      TrackDisplayEvent(DISPLAY_EVENT_LACKS_SERVICE_WORKER);

    SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kNo);
    Stop(error);
    return;
  }

  if (IsWebAppConsideredInstalled() && !ShouldAllowWebAppReplacementInstall()) {
    banners::TrackDisplayEvent(banners::DISPLAY_EVENT_INSTALLED_PREVIOUSLY);
    SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kNo);
    Stop(ALREADY_INSTALLED);
    return;
  }

  if (ShouldDeferToRelatedApplication()) {
    SetInstallableWebAppCheckResult(
        InstallableWebAppCheckResult::kByUserRequest);
    Stop(PREFER_RELATED_APPLICATIONS);
    return;
  }

  SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kPromotable);

  DCHECK(data.has_worker && data.valid_manifest);
  DCHECK(!data.primary_icon_url.is_empty());
  DCHECK(data.primary_icon);

  primary_icon_url_ = data.primary_icon_url;
  primary_icon_ = *data.primary_icon;
  has_maskable_primary_icon_ = data.has_maskable_primary_icon;

  // If we triggered the installability check on page load, then it's possible
  // we don't have enough engagement yet. If that's the case, return here but
  // don't call Terminate(). We wait for OnEngagementEvent to tell us that we
  // should trigger.
  if (!HasSufficientEngagement()) {
    UpdateState(State::PENDING_ENGAGEMENT);
    return;
  }

  SendBannerPromptRequest();
}

void AppBannerManager::RecordDidShowBanner() {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  AppBannerSettingsHelper::RecordBannerEvent(
      contents, validated_url_, GetAppIdentifier(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW,
      GetCurrentTime());
}

void AppBannerManager::ReportStatus(InstallableStatusCode code) {
  DCHECK(status_reporter_);
  status_reporter_->ReportStatus(code);
}

void AppBannerManager::ResetBindings() {
  receiver_.reset();
  event_.reset();
}

void AppBannerManager::ResetCurrentPageData() {
  load_finished_ = false;
  has_sufficient_engagement_ = false;
  active_media_players_.clear();
  manifest_ = blink::Manifest();
  manifest_url_ = GURL();
  validated_url_ = GURL();
  UpdateState(State::INACTIVE);
  SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kUnknown);
}

void AppBannerManager::Terminate() {
  if (state_ == State::PENDING_PROMPT) {
    TrackBeforeInstallEvent(
        BEFORE_INSTALL_EVENT_PROMPT_NOT_CALLED_AFTER_PREVENT_DEFAULT);
  }

  if (state_ == State::PENDING_ENGAGEMENT && !has_sufficient_engagement_)
    TrackDisplayEvent(DISPLAY_EVENT_NOT_VISITED_ENOUGH);

  Stop(TerminationCode());
}

InstallableStatusCode AppBannerManager::TerminationCode() const {
  switch (state_) {
    case State::PENDING_PROMPT:
      return RENDERER_CANCELLED;
    case State::PENDING_ENGAGEMENT:
      return has_sufficient_engagement_ ? NO_ERROR_DETECTED
                                        : INSUFFICIENT_ENGAGEMENT;
    case State::FETCHING_MANIFEST:
      return WAITING_FOR_MANIFEST;
    case State::FETCHING_NATIVE_DATA:
      return WAITING_FOR_NATIVE_DATA;
    case State::PENDING_INSTALLABLE_CHECK:
      return WAITING_FOR_INSTALLABLE_CHECK;
    case State::ACTIVE:
    case State::SENDING_EVENT:
    case State::SENDING_EVENT_GOT_EARLY_PROMPT:
    case State::INACTIVE:
    case State::COMPLETE:
      break;
  }
  return NO_ERROR_DETECTED;
}

void AppBannerManager::SetInstallableWebAppCheckResult(
    InstallableWebAppCheckResult result) {
  if (installable_web_app_check_result_ == result)
    return;

  installable_web_app_check_result_ = result;

  switch (result) {
    case InstallableWebAppCheckResult::kUnknown:
      break;
    case InstallableWebAppCheckResult::kPromotable:
      last_promotable_web_app_scope_ = manifest_.scope;
      DCHECK(!last_promotable_web_app_scope_.is_empty());
      install_animation_pending_ =
          AppBannerSettingsHelper::CanShowInstallTextAnimation(
              web_contents(), last_promotable_web_app_scope_);
      break;
    case InstallableWebAppCheckResult::kByUserRequest:
    case InstallableWebAppCheckResult::kNo:
      last_promotable_web_app_scope_ = GURL();
      install_animation_pending_ = false;
      break;
  }

  for (Observer& observer : observer_list_)
    observer.OnInstallableWebAppStatusUpdated();
}

void AppBannerManager::Stop(InstallableStatusCode code) {
  ReportStatus(code);

  if (installable_web_app_check_result_ ==
      InstallableWebAppCheckResult::kUnknown)
    SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kNo);
  InvalidateWeakPtrs();
  ResetBindings();
  UpdateState(State::COMPLETE);
  status_reporter_ = std::make_unique<NullStatusReporter>(),
  has_sufficient_engagement_ = false;
}

void AppBannerManager::SendBannerPromptRequest() {
  RecordCouldShowBanner();

  UpdateState(State::SENDING_EVENT);
  TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_CREATED);

  // Any existing binding is invalid when we send a new beforeinstallprompt.
  ResetBindings();

  mojo::Remote<blink::mojom::AppBannerController> controller;
  web_contents()->GetMainFrame()->GetRemoteInterfaces()->GetInterface(
      controller.BindNewPipeAndPassReceiver());

  // Get a raw controller pointer before we move out of the smart pointer to
  // avoid crashing with MSVC's order of evaluation.
  blink::mojom::AppBannerController* controller_ptr = controller.get();
  controller_ptr->BannerPromptRequest(
      receiver_.BindNewPipeAndPassRemote(), event_.BindNewPipeAndPassReceiver(),
      {GetBannerType()},
      base::BindOnce(&AppBannerManager::OnBannerPromptReply, GetWeakPtr(),
                     std::move(controller)));
}

void AppBannerManager::UpdateState(State state) {
  state_ = state;
}

void AppBannerManager::DidFinishNavigation(content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame() || !handle->HasCommitted() ||
      handle->IsSameDocument()) {
    return;
  }

  // If the page gets stored in the back-forward cache we will not trigger the
  // pipeline again when navigating back (DidFinishLoad will not trigger). So
  // only allow the page to enter the cache if we know for sure that no
  // installation is needed.
  // Note: this check must happen before calling Terminate as it might set the
  // installable_web_app_check_result_ to kNo.
  if (installable_web_app_check_result_ != InstallableWebAppCheckResult::kNo &&
      state_ != State::INACTIVE) {
    content::BackForwardCache::DisableForRenderFrameHost(
        handle->GetPreviousRenderFrameHostId(), "banners::AppBannerManager");
  }

  if (state_ != State::COMPLETE && state_ != State::INACTIVE)
    Terminate();
  ResetCurrentPageData();
}

void AppBannerManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // Don't start the banner flow unless the main frame has finished loading.
  if (render_frame_host->GetParent())
    return;

  load_finished_ = true;
  validated_url_ = validated_url;

  // If we already have enough engagement, or require no engagement to trigger
  // the banner, the rest of the banner pipeline should operate as if the
  // engagement threshold has been met.
  if (AppBannerSettingsHelper::HasSufficientEngagement(0) ||
      AppBannerSettingsHelper::HasSufficientEngagement(
          GetSiteEngagementService()->GetScore(validated_url))) {
    has_sufficient_engagement_ = true;
  }

  // Start the pipeline immediately if we haven't already started it.
  if (state_ == State::INACTIVE)
    RequestAppBanner(validated_url);
}

void AppBannerManager::MediaStartedPlaying(const MediaPlayerInfo& media_info,
                                           const content::MediaPlayerId& id) {
  active_media_players_.push_back(id);
}

void AppBannerManager::MediaStoppedPlaying(
    const MediaPlayerInfo& media_info,
    const content::MediaPlayerId& id,
    WebContentsObserver::MediaStoppedReason reason) {
  base::Erase(active_media_players_, id);
}

void AppBannerManager::WebContentsDestroyed() {
  Terminate();
}

void AppBannerManager::OnEngagementEvent(
    content::WebContents* contents,
    const GURL& url,
    double score,
    SiteEngagementService::EngagementType /*type*/) {
  // Only trigger a banner using site engagement if:
  //  1. engagement increased for the web contents which we are attached to; and
  //  2. there are no currently active media players; and
  //  3. we have accumulated sufficient engagement.
  if (web_contents() == contents && active_media_players_.empty() &&
      AppBannerSettingsHelper::HasSufficientEngagement(score)) {
    has_sufficient_engagement_ = true;

    if (state_ == State::PENDING_ENGAGEMENT) {
      // We have already finished the installability eligibility checks. Proceed
      // directly to sending the banner prompt request.
      UpdateState(State::ACTIVE);
      SendBannerPromptRequest();
    } else if (load_finished_ && state_ == State::INACTIVE) {
      // This performs some simple tests and starts async checks to test
      // installability. It should be safe to start in response to user input.
      // Don't call if we're already working on processing a banner request.
      RequestAppBanner(url);
    }
  }
}

bool AppBannerManager::IsRunning() const {
  switch (state_) {
    case State::INACTIVE:
    case State::PENDING_PROMPT:
    case State::PENDING_ENGAGEMENT:
    case State::COMPLETE:
      return false;
    case State::ACTIVE:
    case State::FETCHING_MANIFEST:
    case State::FETCHING_NATIVE_DATA:
    case State::PENDING_INSTALLABLE_CHECK:
    case State::SENDING_EVENT:
    case State::SENDING_EVENT_GOT_EARLY_PROMPT:
      return true;
  }
  return false;
}

// static
base::string16 AppBannerManager::GetInstallableWebAppName(
    content::WebContents* web_contents) {
  AppBannerManager* manager = FromWebContents(web_contents);
  if (!manager)
    return base::string16();
  switch (manager->installable_web_app_check_result_) {
    case InstallableWebAppCheckResult::kUnknown:
    case InstallableWebAppCheckResult::kNo:
      return base::string16();
    case InstallableWebAppCheckResult::kByUserRequest:
    case InstallableWebAppCheckResult::kPromotable:
      return manager->GetAppName();
  }
}

bool AppBannerManager::IsProbablyPromotableWebApp() const {
  switch (installable_web_app_check_result_) {
    case InstallableWebAppCheckResult::kUnknown:
      return last_promotable_web_app_scope_.is_valid() &&
             base::StartsWith(web_contents()->GetLastCommittedURL().spec(),
                              last_promotable_web_app_scope_.spec(),
                              base::CompareCase::SENSITIVE);
    case InstallableWebAppCheckResult::kNo:
    case InstallableWebAppCheckResult::kByUserRequest:
      return false;
    case InstallableWebAppCheckResult::kPromotable:
      return true;
  }
}

bool AppBannerManager::MaybeConsumeInstallAnimation() {
  DCHECK(IsProbablyPromotableWebApp());
  if (!install_animation_pending_)
    return false;
  AppBannerSettingsHelper::RecordInstallTextAnimationShown(
      web_contents(), last_promotable_web_app_scope_);
  install_animation_pending_ = false;
  return true;
}

void AppBannerManager::RecordCouldShowBanner() {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  AppBannerSettingsHelper::RecordBannerEvent(
      contents, validated_url_, GetAppIdentifier(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, GetCurrentTime());
}

InstallableStatusCode AppBannerManager::ShouldShowBannerCode() {
  if (GetAppIdentifier().empty())
    return PACKAGE_NAME_OR_START_URL_EMPTY;
  return NO_ERROR_DETECTED;
}

void AppBannerManager::OnBannerPromptReply(
    mojo::Remote<blink::mojom::AppBannerController> controller,
    blink::mojom::AppBannerPromptReply reply) {
  // The renderer might have requested the prompt to be canceled. They may
  // request that it is redisplayed later, so don't Terminate() here. However,
  // log that the cancelation was requested, so Terminate() can be called if a
  // redisplay isn't asked for.
  //
  // If the redisplay request has not been received already, we stop here and
  // wait for the prompt function to be called. If the redisplay request has
  // already been received before cancel was sent (e.g. if redisplay was
  // requested in the beforeinstallprompt event handler), we keep going and show
  // the banner immediately.
  bool event_canceled = reply == blink::mojom::AppBannerPromptReply::CANCEL;
  if (event_canceled) {
    TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_PREVENT_DEFAULT_CALLED);
    if (ShouldBypassEngagementChecks()) {
      web_contents()->GetMainFrame()->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kInfo,
          "Banner not shown: beforeinstallpromptevent.preventDefault() called. "
          "The page must call beforeinstallpromptevent.prompt() to show the "
          "banner.");
    }
  }

  if (state_ == State::SENDING_EVENT) {
    if (!event_canceled)
      MaybeShowAmbientBadge();
    UpdateState(State::PENDING_PROMPT);
    return;
  }

  DCHECK_EQ(State::SENDING_EVENT_GOT_EARLY_PROMPT, state_);

  ShowBanner();
}

void AppBannerManager::MaybeShowAmbientBadge() {}

void AppBannerManager::ShowBanner() {
  // The banner is only shown if the site explicitly requests it to be shown.
  DCHECK_NE(State::SENDING_EVENT, state_);

  content::WebContents* contents = web_contents();
  WebappInstallSource install_source;

  TrackBeforeInstallEvent(
      BEFORE_INSTALL_EVENT_PROMPT_CALLED_AFTER_PREVENT_DEFAULT);
  install_source =
      status_reporter_->GetInstallSource(contents, InstallTrigger::API);

  // If this is the first time that we are showing the banner for this site,
  // record how long it's been since the first visit.
  if (AppBannerSettingsHelper::GetSingleBannerEvent(
          web_contents(), validated_url_, GetAppIdentifier(),
          AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW)
          .is_null()) {
    AppBannerSettingsHelper::RecordMinutesFromFirstVisitToShow(
        web_contents(), validated_url_, GetAppIdentifier(), GetCurrentTime());
  }

  DCHECK(!manifest_url_.is_empty());
  DCHECK(!manifest_.IsEmpty());
  DCHECK(!primary_icon_url_.is_empty());
  DCHECK(!primary_icon_.drawsNothing());

  TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_COMPLETE);
  ShowBannerUi(install_source);
  UpdateState(State::COMPLETE);
}

void AppBannerManager::DisplayAppBanner() {
  // Prevent this from being called multiple times on the same connection.
  receiver_.reset();

  if (state_ == State::PENDING_PROMPT) {
    ShowBanner();
  } else if (state_ == State::SENDING_EVENT) {
    // Log that the prompt request was made for when we get the prompt reply.
    UpdateState(State::SENDING_EVENT_GOT_EARLY_PROMPT);
  }
}

}  // namespace banners

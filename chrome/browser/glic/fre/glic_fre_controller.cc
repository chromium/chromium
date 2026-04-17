// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre/glic_fre_controller.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/version_info/channel.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/fre/fre_util.h"
#include "chrome/browser/glic/fre/glic_fre_page_handler.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/ui/browser.h"
#endif

namespace glic {

GlicFreController::GlicFreController(Profile* profile,
                                     signin::IdentityManager* identity_manager)
    : profile_(profile) {}

GlicFreController::~GlicFreController() = default;

void GlicFreController::WebUiStateChanged(mojom::FreWebUiState new_state) {
  // UI State has changed
  webui_state_ = new_state;
  webui_state_callback_list_.Notify(webui_state_);
}

base::CallbackListSubscription GlicFreController::AddWebUiStateChangedCallback(
    WebUiStateChangedCallback callback) {
  return webui_state_callback_list_.Add(std::move(callback));
}

void GlicFreController::Shutdown() {
  DismissFre(webui_state_);
}

bool GlicFreController::ShouldShowFreDialog() {
  if (GlicEnabling::IsTrustFirstOnboardingEnabledForProfile(profile_)) {
    return false;
  }
  // If the given profile has not previously completed the FRE and is eligible,
  // then it should be shown.
  return GlicEnabling::IsEnabledForProfile(profile_) &&
         !GlicEnabling::HasConsentedForProfile(profile_);
}

#if !BUILDFLAG(IS_ANDROID)
bool GlicFreController::CanShowFreDialog(BrowserWindowInterface* bwi) {
  // The FRE can only be shown given a valid browser. If there is no browser,
  // then an OS-level entrypoint is being used, which should not be possible
  // before the FRE has been accepted.
  if (!bwi) {
    return false;
  }
  // If there is a browser, the FRE can only be shown if no other modal is
  // currently being shown on the same tab.
  tabs::TabInterface* tab = bwi->GetActiveTabInterface();
  return tab && tab->CanShowModalUI();
}
#endif

#if !BUILDFLAG(IS_ANDROID)
void GlicFreController::OpenFreDialogInNewTab(
    base::WeakPtr<BrowserWindowInterface> bwi,
    mojom::InvocationSource source) {
  if (!bwi) {
    return;
  }
  Browser* browser = bwi->GetBrowserForMigrationOnly();
  if (!ShouldShowFreDialog()) {
    return;
  }
  chrome::AddAndReturnTabAt(browser, GURL(), /*index=*/-1, /*foreground=*/true);
  if (CanShowFreDialog(bwi.get())) {
    GlicKeyedServiceFactory::GetGlicKeyedService(profile_)->ToggleUI(
        bwi.get(), /*prevent_close=*/true, source);
  }
}
#endif

void GlicFreController::MarkFreStartAttempt() {
  pending_open_start_time_ = base::TimeTicks::Now();
}

void GlicFreController::MarkSidepanelFreShown() {
  base::RecordAction(base::UserMetricsAction("Glic.Fre.Shown"));
}

void GlicFreController::RecordFrameworkStartTime() {
  pending_framework_start_time_ = base::TimeTicks::Now();
}

#if !BUILDFLAG(IS_ANDROID)
void GlicFreController::DismissFreIfOpenOnActiveTab(
    BrowserWindowInterface* browser) {
  if (!browser) {
    return;
  }

  tabs::TabInterface* tab = browser->GetActiveTabInterface();

  // If the FRE is being shown on the current tab, close it.
  if (fre_widget_ && tab_showing_modal_ == tab) {
    base::RecordAction(base::UserMetricsAction("Glic.Fre.CloseWithToggle"));
    DismissFre(webui_state_);
  }
}
#endif

void GlicFreController::AcceptFre(GlicFrePageHandler* handler) {
  // Notify other handlers that they lost the race.
  for (GlicFrePageHandler* other_handler : handlers_) {
    if (other_handler != handler) {
      other_handler->OnAcceptedByOtherInstance();
    }
  }
  base::RecordAction(base::UserMetricsAction("Glic.Fre.Accept"));
  // Update FRE related preferences.
  GlicKeyedService::Get(profile_)->enabling().SetCompletedFre(
      prefs::FreStatus::kCompleted);

#if !BUILDFLAG(IS_ANDROID)
  GlicLauncherConfiguration::CheckDefaultBrowserToEnableLauncher();

  // Dismiss the FRE window and then show the Glic panel, but store source
  // browser before it is cleared.
  BrowserWindowInterface* source_browser = source_browser_;
  CloseWithFreReason(GlicFreWidgetClosedReason::kAcceptButtonClicked);

  // Show a glic window attached to the invocation source browser.
  if (source_browser) {
    GlicKeyedServiceFactory::GetGlicKeyedService(profile_)->ToggleUI(
        source_browser, /*prevent_close=*/true, mojom::InvocationSource::kFre);
  }
#endif
}

void GlicFreController::RejectFre() {
  base::RecordAction(base::UserMetricsAction("Glic.Fre.NoThanks"));
  CloseWithFreReason(GlicFreWidgetClosedReason::kCancelButtonClicked);
}

void GlicFreController::CloseWithFreReason(GlicFreWidgetClosedReason reason) {
  base::UmaHistogramEnumeration("Glic.Fre.WidgetClosedReason2", reason);
  DismissFre(webui_state_);
}

void GlicFreController::DismissFre(mojom::FreWebUiState panel) {
  if (IsShowingDialog()) {
    switch (panel) {
      case mojom::FreWebUiState::kError:
        base::RecordAction(
            base::UserMetricsAction("Glic.Fre.ErrorPanelClosed"));
        break;
      case mojom::FreWebUiState::kDisabledByAdmin:
        base::RecordAction(
            base::UserMetricsAction("Glic.Fre.DisabledByAdminPanelClosed"));
        break;
      case mojom::FreWebUiState::kOffline:
        base::RecordAction(
            base::UserMetricsAction("Glic.Fre.OfflinePanelClosed"));
        break;
      case mojom::FreWebUiState::kBeginLoading:
      case mojom::FreWebUiState::kShowLoading:
      case mojom::FreWebUiState::kHoldLoading:
      case mojom::FreWebUiState::kFinishLoading:
        base::RecordAction(
            base::UserMetricsAction("Glic.Fre.LoadingPanelClosed"));
        break;
      case mojom::FreWebUiState::kReady:
        base::RecordAction(
            base::UserMetricsAction("Glic.Fre.ReadyPanelClosed"));
        break;
      case mojom::FreWebUiState::kUninitialized:
        base::RecordAction(
            base::UserMetricsAction("Glic.Fre.UninitializedPanelClosed"));
        break;
    }
  }
  web_contents_ = nullptr;
#if !BUILDFLAG(IS_ANDROID)
  source_browser_ = nullptr;

  if (fre_widget_) {
    base::UmaHistogramEnumeration("Glic.FreModalWebUiState.FinishState2",
                                  webui_state_);
    fre_widget_.reset();
    tab_showing_modal_ = nullptr;
    will_detach_subscription_ = {};
  }
#endif
}

void GlicFreController::PrepareForClient(
    base::OnceCallback<void(bool)> callback) {
  // TODO(b:501139710): With the unified FRE, we shouldn't need a separate auth
  // controller.
  GlicKeyedServiceFactory::GetGlicKeyedService(profile_)
      ->GetAuthController()
      .CheckAuthBeforeLoad(
          base::BindOnce([](mojom::PrepareForClientResult result) {
            switch (result) {
              case mojom::PrepareForClientResult::kErrorResyncingCookies:
                base::UmaHistogramEnumeration(
                    "Glic.FreErrorStateReason",
                    FreErrorStateReason::kErrorResyncingCookies);
                break;
              case mojom::PrepareForClientResult::kRequiresSignIn:
                base::UmaHistogramEnumeration(
                    "Glic.FreErrorStateReason",
                    FreErrorStateReason::kSignInRequired);
                break;
              case mojom::PrepareForClientResult::kSuccess:
                break;
            }
            return result == mojom::PrepareForClientResult::kSuccess;
          }).Then(std::move(callback)));
}

void GlicFreController::ExceededTimeoutError() {
  base::UmaHistogramEnumeration("Glic.FreErrorStateReason",
                                FreErrorStateReason::kTimeoutExceeded);
}

void GlicFreController::OnLinkClicked(const GURL& url) {
  if (url.DomainIs("support.google.com")) {
    if (url.GetPath().find("13594961") != std::string::npos) {
      base::RecordAction(
          base::UserMetricsAction("Glic.Fre.PrivacyNoticeLinkOpened"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("Glic.Fre.HelpCenterLinkOpened"));
    }
    return;
  }

  if (url.DomainIs("policies.google.com")) {
    base::RecordAction(base::UserMetricsAction("Glic.Fre.PolicyLinkOpened"));
    return;
  }

  if (url.DomainIs("myactivity.google.com")) {
    base::RecordAction(
        base::UserMetricsAction("Glic.Fre.MyActivityLinkOpened"));
    return;
  }
}

content::WebContents* GlicFreController::GetWebContents() {
  return web_contents_;
}

namespace {

constexpr net::NetworkTrafficAnnotationTag kGlicFrePreconnectTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("glic_fre_preconnect",
                                        R"(
    semantics {
      sender: "Gemini in Chrome"
      description:
        "This request is issued when the Gemini in Chrome first-run experience "
        "is predicted to be issued soon, to establish a connection to the "
        "server."
      trigger:
        "Hovering or focusing the Gemini button."
      data:
        "Minimal data is exchanged, though this may share network state "
        "with credentialed requests."
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          owners: "//chrome/browser/glic/OWNERS"
        }
      }
      user_data {
        type: NONE
      }
      last_reviewed: "2025-02-26"
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "There are a number of ways to prevent this request:"
        "A) Disable predictive operations under Settings > Performance "
        "   > Preload pages for faster browsing and searching,"
        "B) Disable Gemini in Chrome altogether"
      chrome_policy {
        GeminiSettings {
          GeminiSettings: 1
        }
        GenAiDefaultSettings {
          GenAiDefaultSettings: 2
        }
      }
    }
)");

BASE_FEATURE(kGlicFrePreconnect, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kGlicFrePreconnectToSubresourceDomains,
                   &kGlicFrePreconnect,
                   "GlicFrePreconnectToSubresourceDomains",
                   true);

}  // namespace

void GlicFreController::MaybePreconnect() {
  if (!ShouldShowFreDialog() ||
      !base::FeatureList::IsEnabled(kGlicFrePreconnect)) {
    return;
  }
  GURL fre_url = glic::GetFreURL(profile_);
  // We'll need this to be in the "same-site" socket pool for the FRE's site,
  // since that's the one that will be used for a real page load.
  net::NetworkAnonymizationKey anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(net::SchemefulSite(fre_url));
  predictors::LoadingPredictor* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile_);
  content::StoragePartitionConfig storage_partition_config =
      GetFreStoragePartitionConfig(profile_);
  loading_predictor->PreconnectURLIfAllowed(
      glic::GetFreURL(profile_), /*allow_credentials=*/true, anonymization_key,
      kGlicFrePreconnectTrafficAnnotation, &storage_partition_config);
  if (kGlicFrePreconnectToSubresourceDomains.Get() &&
      google_util::IsGoogleDomainUrl(fre_url, google_util::ALLOW_SUBDOMAIN,
                                     google_util::ALLOW_NON_STANDARD_PORTS)) {
    loading_predictor->PreconnectURLIfAllowed(
        GURL("https://www.gstatic.com/"), /*allow_credentials=*/true,
        anonymization_key, kGlicFrePreconnectTrafficAnnotation,
        &storage_partition_config);
  }
}

void GlicFreController::OnTabShowingModalWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  GlicFreWidgetClosedReason glic_reason;
  switch (reason) {
    case tabs::TabInterface::DetachReason::kDelete:
      base::RecordAction(
          base::UserMetricsAction("Glic.Fre.CloseByClosingHostTab"));
      glic_reason = GlicFreWidgetClosedReason::kHostTabClosed;
      break;
    case tabs::TabInterface::DetachReason::kInsertIntoOtherWindow:
      base::RecordAction(
          base::UserMetricsAction("Glic.Fre.CloseByMovingHostTab"));
      glic_reason = GlicFreWidgetClosedReason::kHostTabMoved;
      break;
  }
  base::UmaHistogramEnumeration("Glic.Fre.WidgetClosedReason2", glic_reason);
  DismissFre(webui_state_);
}

bool GlicFreController::IsShowingDialog() const {
  if (is_showing_dialog_for_testing_.has_value()) {
    return is_showing_dialog_for_testing_.value();
  }
#if !BUILDFLAG(IS_ANDROID)
  return !!fre_widget_;
#else
  return false;
#endif
}

bool GlicFreController::IsShowingDialogAndStateInitialized() const {
#if !BUILDFLAG(IS_ANDROID)
  return !!fre_widget_ &&
         (webui_state_ != mojom::FreWebUiState::kUninitialized);
#else
  return false;
#endif
}

gfx::Size GlicFreController::GetFreInitialSize() {
  return gfx::Size(features::kGlicFreInitialWidth.Get(),
                   features::kGlicFreInitialHeight.Get());
}

void GlicFreController::UpdateFreWidgetSize(const gfx::Size& new_size) {
#if !BUILDFLAG(IS_ANDROID)
  if (!fre_widget_) {
    return;
  }

  fre_widget_->SetSize(new_size);
#endif
}

GlicFreController::InitTimestamps GlicFreController::RegisterPageHandler(
    GlicFrePageHandler* handler) {
  handlers_.push_back(handler);

  InitTimestamps timestamps;
  timestamps.open_start_time =
      pending_open_start_time_.value_or(base::TimeTicks());
  timestamps.framework_start_time =
      pending_framework_start_time_.value_or(base::TimeTicks());

  pending_open_start_time_.reset();
  pending_framework_start_time_.reset();

  return timestamps;
}

void GlicFreController::UnregisterPageHandler(GlicFrePageHandler* handler) {
  std::erase(handlers_, handler);
}

}  // namespace glic

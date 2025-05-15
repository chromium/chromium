// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre/glic_fre_controller.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/version_info/channel.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/fre/fre_util.h"
#include "chrome/browser/glic/fre/glic_fre_dialog_view.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicFreController::GlicFreController(Profile* profile,
                                     signin::IdentityManager* identity_manager)
    : profile_(profile),
      auth_controller_(profile, identity_manager, /*use_for_fre=*/true) {}

GlicFreController::~GlicFreController() = default;

void GlicFreController::WebUiStateChanged(mojom::FreWebUiState new_state) {
  if (webui_state_ == new_state) {
    return;
  }

  // UI State has changed
  webui_state_ = new_state;
  webui_state_callback_list_.Notify(webui_state_);

  // It is possible for the FRE to open directly in an error state. In this
  // case, we should not record the FRE load time metric if the content is
  // loaded at a later point.
  if (new_state == mojom::FreWebUiState::kError ||
      new_state == mojom::FreWebUiState::kOffline) {
    show_start_time_ = base::TimeTicks();
  }

  RecordMetricsIfDialogIsShowingAndReady();
}

base::CallbackListSubscription GlicFreController::AddWebUiStateChangedCallback(
    WebUiStateChangedCallback callback) {
  return webui_state_callback_list_.Add(std::move(callback));
}

void GlicFreController::Shutdown() {
  DismissFre();
}

bool GlicFreController::ShouldShowFreDialog() {
  // If the given profile has not previously completed the FRE, then it should
  // be shown.
  return !GlicEnabling::HasConsentedForProfile(profile_);
}

bool GlicFreController::CanShowFreDialog(Browser* browser) {
  // The FRE can only be shown given a valid browser. If there is no browser,
  // then an OS-level entrypoint is being used, which should not be possible
  // before the FRE has been accepted.
  if (!browser) {
    return false;
  }
  // If there is a browser, the FRE can only be shown if no other modal is
  // currently being shown on the same tab.
  tabs::TabInterface* tab = browser->GetActiveTabInterface();
  return tab && tab->CanShowModalUI();
}

void GlicFreController::OpenFreDialogInNewTab(BrowserWindowInterface* bwi) {
  Browser* browser = bwi->GetBrowserForMigrationOnly();
  if (!ShouldShowFreDialog()) {
    return;
  }
  chrome::AddAndReturnTabAt(browser, GURL(), /*index=*/-1, /*foreground=*/true);
  if (CanShowFreDialog(browser)) {
    ShowFreDialog(browser);
  }
}

void GlicFreController::ShowFreDialog(Browser* browser) {
  CHECK(CanShowFreDialog(browser));

  show_start_time_ = base::TimeTicks::Now();
  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

  if (auth_controller_.CheckAuthBeforeShowSync(base::BindOnce(
          &GlicFreController::OpenFreDialogInNewTab, GetWeakPtr(), browser))) {
    ShowFreDialogAfterAuthCheck(browser->AsWeakPtr());
  } else {
    // Sign-in required and handled by AuthController. In this case, do not
    // record the FRE load time metric.
    show_start_time_ = base::TimeTicks();
  }
}

void GlicFreController::ShowFreDialogAfterAuthCheck(
    base::WeakPtr<Browser> browser) {
  // Abort if the browser was closed, to avoid crashing. Note, the user
  // shouldn't have much chance to close the browser between ShowFreDialog() and
  // ShowFreDialogAfterAuthCheck().
  if (!browser) {
    return;
  }

  // Close any existing FRE dialog before showing.
  if (IsShowingDialog()) {
    DismissFre();
  }

  source_browser_ = browser.get();

  CreateView();

  tab_showing_modal_ = browser->GetActiveTabInterface();
  // Note that this call to `CreateShowDialogAndBlockTabInteraction` is
  // necessarily preceded by a call to `CanShowModalUI`. See
  // `GlicFreController::CanShowFreDialog`.
  // TODO(crbug.com/393400004): This returned widget should be configured to
  // use a synchronous close.
  fre_widget_ = tab_showing_modal_->GetTabFeatures()
                    ->tab_dialog_manager()
                    ->CreateShowDialogAndBlockTabInteraction(
                        fre_view_.release(), /*close_on_navigation=*/false);
  GetWebContents()->Focus();
  will_detach_subscription_ = tab_showing_modal_->RegisterWillDetach(
      base::BindRepeating(&GlicFreController::OnTabShowingModalWillDetach,
                          base::Unretained(this)));
  fre_widget_->MakeCloseSynchronous(base::BindOnce(
      &GlicFreController::CloseWithReason, base::Unretained(this)));

  base::RecordAction(base::UserMetricsAction("Glic.Fre.Shown"));
  auth_controller_.OnGlicWindowOpened();

  // Recording the load latency time when FRE contents were preloaded.
  RecordMetricsIfDialogIsShowingAndReady();
}

void GlicFreController::DismissFreIfOpenOnActiveTab(Browser* browser) {
  if (!browser) {
    return;
  }

  tabs::TabInterface* tab = browser->GetActiveTabInterface();

  // If the FRE is being shown on the current tab, close it.
  if (fre_widget_ && tab_showing_modal_ == tab) {
    DismissFre();
  }
}

void GlicFreController::AcceptFre() {
  base::RecordAction(base::UserMetricsAction("Glic.Fre.Accept"));
  // Update FRE related preferences.
  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  // Enable the launcher if it is still disabled by default and the browser
  // is default or is on the stable channel.
  bool is_enabled_default = false;
  const bool is_launcher_enabled =
      GlicLauncherConfiguration::IsEnabled(&is_enabled_default);
  if (is_enabled_default && !is_launcher_enabled) {
    base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
        ->StartCheckIsDefault(
            base::BindOnce(&GlicFreController::OnCheckIsDefaultBrowserFinished,
                           chrome::GetChannel()));
  }

  // Dismiss the FRE window and then show the Glic panel, but store source
  // browser before it is cleared.
  Browser* source_browser = source_browser_;
  DismissFre();

  // Show a glic window attached to the invocation source browser.
  if (source_browser) {
    GlicKeyedServiceFactory::GetGlicKeyedService(profile_)->ToggleUI(
        source_browser, /*prevent_close=*/true, mojom::InvocationSource::kFre);
  }
}

void GlicFreController::CloseWithReason(views::Widget::ClosedReason reason) {
  DismissFre();
}

void GlicFreController::DismissFre() {
  base::UmaHistogramEnumeration("Glic.FreModalWebUiState.FinishState",
                                webui_state_);
  web_contents_ = nullptr;
  source_browser_ = nullptr;
  if (fre_view_ || fre_widget_) {
    auto* service = GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
    glic::GlicProfileManager* glic_profile_manager =
        glic::GlicProfileManager::GetInstance();
    if (glic_profile_manager) {
      glic_profile_manager->OnUnloadingClientForService(service);
    }
  }
  if (fre_widget_) {
    fre_widget_.reset();
    tab_showing_modal_ = nullptr;
    will_detach_subscription_ = {};
    show_start_time_ = base::TimeTicks();
  }
  fre_view_.reset();
}

void GlicFreController::PrepareForClient(
    base::OnceCallback<void(bool)> callback) {
  auth_controller_.CheckAuthBeforeLoad(
      base::BindOnce([](mojom::PrepareForClientResult result) {
        return result == mojom::PrepareForClientResult::kSuccess;
      }).Then(std::move(callback)));
}

void GlicFreController::OnLinkClicked(const GURL& url) {
  if (url.DomainIs("support.google.com")) {
    if (url.path().find("13594961") != std::string::npos) {
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

void GlicFreController::OnNoThanksClicked() {
  base::RecordAction(base::UserMetricsAction("Glic.Fre.NoThanks"));
  DismissFre();
}

void GlicFreController::TryPreload() {
  // Callers should not attempt to preload if the widget is showing.
  CHECK(!fre_widget_);

  if (fre_view_ || auth_controller_.RequiresSignIn()) {
    return;
  }

  CreateView();
}

bool GlicFreController::IsWarmed() const {
  return !!fre_view_;
}

content::WebContents* GlicFreController::GetWebContents() {
  return web_contents_;
}

namespace {

// TODO(jbroman): This should be updated with more specifics once more
// information about Glic is available, with updated strings and policy details.
constexpr net::NetworkTrafficAnnotationTag kGlicFrePreconnectTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("glic_fre_preconnect",
                                        R"(
    semantics {
      sender: "Glic FRE Preconnect"
      description:
        "This request is issued when the Glic first-run experience is "
        "predicted to be issued soon, to establish a connection to the "
        "server."
      trigger:
        "Hovering or focusing the Glic button."
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
        "B) Disable Glic altogether"
      chrome_policy {
        URLBlocklist {
          URLBlocklist: { entries: '*' }
        }
      }
      chrome_policy {
        URLAllowlist {
          URLAllowlist { }
        }
      }
    }
    comments:
      "This feature can be safely disabled, but enabling it may result in "
      "faster load of the Glic first-run experience. Using either "
      "URLBlocklist or URLAllowlist policies (or a combination of both) "
      "limits the scope of these requests."
)");

BASE_FEATURE(kGlicFrePreconnect,
             "GlicFrePreconnect",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// static
void GlicFreController::OnCheckIsDefaultBrowserFinished(
    version_info::Channel channel,
    shell_integration::DefaultWebClientState state) {
  // Don't do anything because a different channel is the default browser
  if (state ==
      shell_integration::DefaultWebClientState::OTHER_MODE_IS_DEFAULT) {
    return;
  }

  // Enables the launcher if the current browser is the default or
  // is on the stable channel.
  if (g_browser_process &&
      (state == shell_integration::DefaultWebClientState::IS_DEFAULT ||
       channel == version_info::Channel::STABLE)) {
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 true);
  }
}

void GlicFreController::OnTabShowingModalWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  DismissFre();
}

void GlicFreController::CreateView() {
  if (fre_view_) {
    return;
  }

  fre_view_ = std::make_unique<GlicFreDialogView>(profile_, this);
  web_contents_ = fre_view_->web_contents();
  web_contents_->Resize(gfx::Rect(GetFreInitialSize()));
  auto* service = GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
  GlicProfileManager::GetInstance()->OnLoadingClientForService(service);
}

void GlicFreController::RecordMetricsIfDialogIsShowingAndReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!!fre_widget_ && webui_state_ == mojom::FreWebUiState::kReady &&
      !show_start_time_.is_null()) {
    base::UmaHistogramMediumTimes("Glic.FrePresentationTime",
                                  (base::TimeTicks::Now() - show_start_time_));
    show_start_time_ = base::TimeTicks();
  }
}

bool GlicFreController::IsShowingDialog() const {
  return !!fre_widget_;
}

gfx::Size GlicFreController::GetFreInitialSize() {
  return gfx::Size(features::kGlicFreInitialWidth.Get(),
                   features::kGlicFreInitialHeight.Get());
}

void GlicFreController::UpdateFreWidgetSize(const gfx::Size& new_size) {
  if (!fre_widget_) {
    return;
  }

  fre_widget_->SetSize(new_size);
}

}  // namespace glic

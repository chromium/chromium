// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_fre_controller.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/version_info/channel.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/fre_util.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_fre_dialog_view.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicFreController::GlicFreController(Profile* profile,
                                     signin::IdentityManager* identity_manager)
    : profile_(profile),
      auth_controller_(profile, identity_manager, /*use_for_fre=*/true) {}

GlicFreController::~GlicFreController() = default;

void GlicFreController::WebUiStateChanged(mojom::FreWebUiState new_state) {
  if (webui_state_ != new_state) {
    // UI State has changed
    webui_state_ = new_state;
    webui_state_callback_list_.Notify(webui_state_);
  }
}

base::CallbackListSubscription GlicFreController::AddWebUiStateChangedCallback(
    WebUiStateChangedCallback callback) {
  return webui_state_callback_list_.Add(std::move(callback));
}

void GlicFreController::Shutdown() {
  DismissFre();
}

bool GlicFreController::ShouldShowFreDialog() {
  PrefService* prefs = profile_->GetPrefs();
  if (!first_time_pref_check_done_) {
    first_time_pref_check_done_ = true;
    // If `--glic-always-open-fre` is present, unset this pref to ensure the FRE
    // is shown for testing convenience. Do this only once so that we can test
    // the accept flow.
    auto* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(::switches::kGlicAlwaysOpenFre)) {
      prefs->SetBoolean(prefs::kGlicCompletedFre, false);
    }
  }

  // If the given profile has not previously completed the FRE, then it should
  // be shown.
  return !prefs->GetBoolean(prefs::kGlicCompletedFre);
}

bool GlicFreController::CanShowFreDialog(Browser* browser) {
  // The FRE can only be shown given a valid browser. If there is no browser,
  // then an OS-level entrypoint is being used, which should not be possible
  // before the FRE has been accepted.
  if (!browser) {
    return false;
  }
  // If there is a browser, the FRE can only be shown if no
  // other modal is currently being shown on the same tab.
  tabs::TabInterface* tab = browser->GetActiveTabInterface();
  return tab && tab->CanShowModalUI();
}

void GlicFreController::ShowFreDialog(Browser* browser) {
  auth_controller_.CheckAuthBeforeShow(
      base::BindOnce(&GlicFreController::ShowFreDialogAfterAuthCheck,
                     GetWeakPtr(), browser->AsWeakPtr()));
}

void GlicFreController::ShowFreDialogAfterAuthCheck(
    base::WeakPtr<Browser> browser,
    AuthController::BeforeShowResult result) {
  if (result == AuthController::BeforeShowResult::kShowingReauthSigninPage) {
    return;
  }
  // Abort if the browser was closed, to avoid crashing. Note, the user
  // shouldn't have much chance to close the browser between ShowFreDialog() and
  // ShowFreDialogAfterAuthCheck().
  if (!browser) {
    return;
  }

  // Close any existing FRE dialog before showing.
  DismissFre();

  fre_view_ = new GlicFreDialogView(
      profile_, gfx::Size(features::kGlicFreInitialWidth.Get(),
                          features::kGlicFreInitialHeight.Get()));

  tabs::TabInterface* tab_interface = browser->GetActiveTabInterface();
  // Note that this call to `CreateShowDialogAndBlockTabInteraction` is
  // necessarily preceded by a call to `CanShowModalUI`. See
  // `GlicFreController::CanShowFreDialog`.
  // TODO(crbug.com/393400004): This returned widget should be configured to
  // use a synchronous close.
  fre_widget_ = tab_interface->GetTabFeatures()
                    ->tab_dialog_manager()
                    ->CreateShowDialogAndBlockTabInteraction(fre_view_);
  tab_showing_modal_ = tab_interface;
  will_detach_subscription_ = tab_showing_modal_->RegisterWillDetach(
      base::BindRepeating(&GlicFreController::OnTabShowingModalWillDetach,
                          base::Unretained(this)));
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
  // Update FRE related preferences.
  profile_->GetPrefs()->SetBoolean(prefs::kGlicCompletedFre, true);

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

  DismissFre();

  // Show a glic window attached to the last active browser of the glic
  // profile, which should correspond to the browser used by the FRE.
  if (Browser* new_attached_browser =
          chrome::FindLastActiveWithProfile(profile_)) {
    glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile_)->ToggleUI(
        new_attached_browser, /*prevent_close=*/true, InvocationSource::kFre);
  }
}

void GlicFreController::DismissFre() {
  if (fre_widget_) {
    fre_view_ = nullptr;
    fre_widget_.reset();
    tab_showing_modal_ = nullptr;
    will_detach_subscription_ = {};
  }
}

content::WebContents* GlicFreController::GetWebContents() {
  if (!fre_view_) {
    return nullptr;
  }
  return fre_view_->web_contents();
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

bool GlicFreController::IsShowingDialogForTesting() const {
  return !!fre_widget_;
}
}  // namespace glic

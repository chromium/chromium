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
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"

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

void GlicFreController::RecordFrameworkStartTime() {
  pending_framework_start_time_ = base::TimeTicks::Now();
}

void GlicFreController::AcceptFre(GlicFrePageHandler* handler) {
  // Notify other handlers so they can update their state.
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
#endif
}

void GlicFreController::RejectFre() {
  base::RecordAction(base::UserMetricsAction("Glic.Fre.NoThanks"));
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

base::TimeTicks GlicFreController::RegisterPageHandler(
    GlicFrePageHandler* handler) {
  handlers_.push_back(handler);

  base::TimeTicks framework_start_time =
      pending_framework_start_time_.value_or(base::TimeTicks());

  pending_framework_start_time_.reset();

  return framework_start_time;
}

void GlicFreController::UnregisterPageHandler(GlicFrePageHandler* handler) {
  std::erase(handlers_, handler);
}

}  // namespace glic

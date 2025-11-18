// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/webui_contents_container.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/webview/webview.h"

namespace glic {

namespace {
content::WebContents::CreateParams MakeCreateParams(Profile* profile,
                                                    bool initially_hidden) {
  auto params = content::WebContents::CreateParams(profile);
  if (base::FeatureList::IsEnabled(
          features::kGlicGuestContentsVisibilityState)) {
    params.initially_hidden = initially_hidden;
  }
  return params;
}

}  // namespace

WebUIContentsContainer::WebUIContentsContainer(
    Profile* profile,
    GlicWindowController* glic_window_controller,
    bool initially_hidden)
    : profile_keep_alive_(profile, ProfileKeepAliveOrigin::kGlicView),
      web_contents_(content::WebContents::Create(
          MakeCreateParams(profile, initially_hidden))),
      glic_window_controller_(glic_window_controller) {
  CHECK(web_contents_);
  Observe(web_contents_.get());
  web_contents_->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);

  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL{chrome::kChromeUIGlicURL}));
}

WebUIContentsContainer::~WebUIContentsContainer() {
  web_contents_->ClosePage();
  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  if (!glic_profile_manager) {
    return;
  }
  auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(
      glic_window_controller_->profile());
  glic_profile_manager->OnUnloadingClientForService(glic_service);
}

void WebUIContentsContainer::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  base::UmaHistogramEnumeration("Glic.Session.WebUiCrash.TerminationStatus",
                                status, base::TERMINATION_STATUS_MAX_ENUM);
  if (status != base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    base::RecordAction(base::UserMetricsAction("GlicSessionWebUiCrash"));
  }
  auto* keyed_service = GlicKeyedServiceFactory::GetGlicKeyedService(
      web_contents_->GetBrowserContext());
  if (GlicEnabling::IsMultiInstanceEnabled()) {
    // TODO(crbug.com/454120908): swap for a reloaded host in case of a crash.
    keyed_service->CloseAndShutdown(web_contents_->GetPrimaryMainFrame());
  } else {
    keyed_service->CloseAndShutdown();
  }
  // WARNING: Do not do any more work, as `this` may have been destroyed.
}

}  // namespace glic

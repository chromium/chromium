// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_page_handler.h"

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition_config.h"
#include "url/gurl.h"

namespace glic {

GlicExperimentalOptInPageHandler::GlicExperimentalOptInPageHandler(
    Profile* profile,
    RequiredExperimentalOptIn required_state,
    mojo::PendingReceiver<mojom::ExperimentalOptInPageHandler> receiver)
    : receiver_(this, std::move(receiver)),
      profile_(profile),
      required_state_(required_state),
      cookie_synchronizer_(std::make_unique<GlicCookieSynchronizer>(
          profile_,
          IdentityManagerFactory::GetForProfile(profile_),
          content::StoragePartitionConfig::Create(
              profile_,
              chrome::kChromeUIGlicExperimentalOptInHost,
              /*partition_name=*/"glicexperimentalpart",
              /*in_memory=*/true))) {}

GlicExperimentalOptInPageHandler::~GlicExperimentalOptInPageHandler() = default;

GlicKeyedService* GlicExperimentalOptInPageHandler::GetGlicService() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
}

void GlicExperimentalOptInPageHandler::Accept() {
  auto* service = GetGlicService();
  auto& enabling = service->enabling();

  switch (required_state_) {
    case RequiredExperimentalOptIn::kGlic:
      enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
      enabling.SetUserEnabledActuationOnWeb(true);
      enabling.SetExperimentalTriggeringEnabled(true);
      break;
    case RequiredExperimentalOptIn::kActuation:
      enabling.SetUserEnabledActuationOnWeb(true);
      enabling.SetExperimentalTriggeringEnabled(true);
      break;
    case RequiredExperimentalOptIn::kExperimental:
      enabling.SetExperimentalTriggeringEnabled(true);
      break;
    case RequiredExperimentalOptIn::kNotNeeded:
      break;
  }

  RecordExperimentalOptInAccepted(required_state_);
  if (required_state_ == RequiredExperimentalOptIn::kGlic) {
    service->metrics()->OnOptInAccepted(OptInFlow::kExperimentalTriggering);
  }
  service->opt_in_controller().CloseDialog(true);
}

void GlicExperimentalOptInPageHandler::Reject() {
  auto* service = GetGlicService();
  RecordExperimentalOptInRejected(required_state_);
  if (required_state_ == RequiredExperimentalOptIn::kGlic) {
    service->metrics()->OnOptInRejected(OptInFlow::kExperimentalTriggering);
  }
  service->opt_in_controller().CloseDialog(false);
}

void GlicExperimentalOptInPageHandler::OnWebviewLoaded() {
  if (required_state_ == RequiredExperimentalOptIn::kGlic) {
    GetGlicService()->metrics()->OnOptInImpression(
        OptInFlow::kExperimentalTriggering);
  }
}

void GlicExperimentalOptInPageHandler::SyncCookies(
    SyncCookiesCallback callback) {
  cookie_synchronizer_->CopyCookiesToWebviewStoragePartition(
      std::move(callback));
}

void GlicExperimentalOptInPageHandler::ValidateAndOpenLinkInNewTab(
    const GURL& url) {
  if (url.DomainIs("google.com")) {
    GetGlicService()->opt_in_controller().OpenLinkInNewTab(url);
  }
}

}  // namespace glic

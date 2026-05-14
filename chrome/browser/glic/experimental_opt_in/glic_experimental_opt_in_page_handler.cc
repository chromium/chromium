// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_page_handler.h"

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace glic {

GlicExperimentalOptInPageHandler::GlicExperimentalOptInPageHandler(
    Profile* profile,
    RequiredExperimentalOptIn required_state,
    mojo::PendingReceiver<mojom::ExperimentalOptInPageHandler> receiver)
    : receiver_(this, std::move(receiver)),
      profile_(profile),
      required_state_(required_state) {}

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

  service->opt_in_controller().CloseDialog();
}

void GlicExperimentalOptInPageHandler::Reject() {
  GetGlicService()->opt_in_controller().CloseDialog();
}

}  // namespace glic

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/glic_private/glic_private_api.h"

#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "extensions/common/extension_features.h"

namespace extensions {

namespace {

api::glic_private::ProfileReadyState ConvertProfileReadyState(
    glic::mojom::ProfileReadyState state) {
  switch (state) {
    case glic::mojom::ProfileReadyState::kUnknownError:
      return api::glic_private::ProfileReadyState::kError;
    case glic::mojom::ProfileReadyState::kSignInRequired:
      return api::glic_private::ProfileReadyState::kSignInRequired;
    case glic::mojom::ProfileReadyState::kReady:
      return api::glic_private::ProfileReadyState::kReady;
    case glic::mojom::ProfileReadyState::kIneligible:
      return api::glic_private::ProfileReadyState::kIneligible;
    case glic::mojom::ProfileReadyState::kDisabledByAdmin:
      return api::glic_private::ProfileReadyState::kDisabledByAdmin;
  }
}

api::glic_private::ProfileState CreateProfileState(Profile* profile) {
  api::glic_private::ProfileState state;

  glic::GlicEnabling::ProfileEnablement enablement =
      glic::GlicEnabling::EnablementForProfile(profile);

  state.is_enabled = enablement.IsEnabled();
  state.is_enabled_and_consented = enablement.IsEnabledAndConsented();
  state.ready_state = ConvertProfileReadyState(
      glic::GlicEnabling::GetProfileReadyState(profile));

  state.live_allowed = enablement.EligibleForLive();
  state.share_image_allowed = enablement.EligibleForShareImage();

  glic::GlicKeyedService* glic_service = glic::GlicKeyedService::Get(profile);

  state.actuation_allowed =
      base::FeatureList::IsEnabled(features::kGlicActor) && glic_service &&
      glic_service->actor_policy_checker().CanActOnWeb();

  return state;
}

}  // namespace

GlicPrivateGetStateFunction::GlicPrivateGetStateFunction() = default;
GlicPrivateGetStateFunction::~GlicPrivateGetStateFunction() = default;

ExtensionFunction::ResponseAction GlicPrivateGetStateFunction::Run() {
  CHECK(base::FeatureList::IsEnabled(extensions_features::kApiGlicPrivate));

  Profile* profile = Profile::FromBrowserContext(browser_context());
  return RespondNow(ArgumentList(api::glic_private::GetState::Results::Create(
      CreateProfileState(profile))));
}

}  // namespace extensions

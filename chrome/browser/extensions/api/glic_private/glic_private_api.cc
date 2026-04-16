// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/glic_private/glic_private_api.h"

#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
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

std::string GetPromptFromId(const std::string& prompt_id) {
  // TODO(b/497936770): Implement prompt id to prompt look up.
  return "Summarize this page";
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

GlicPrivateInvokeFunction::GlicPrivateInvokeFunction() = default;
GlicPrivateInvokeFunction::~GlicPrivateInvokeFunction() = default;

ExtensionFunction::ResponseAction GlicPrivateInvokeFunction::Run() {
  std::optional<api::glic_private::Invoke::Params> params =
      api::glic_private::Invoke::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());

  // TODO(b/497966450): Add necessary eligibility checks here.

  if (!glic::GlicEnabling::IsEnabledForProfile(profile)) {
    return RespondNow(Error("Glic is not enabled."));
  }

  glic::mojom::InvocationSource source =
      glic::mojom::InvocationSource::kUnsupported;
  glic::mojom::FeatureMode feature_mode =
      glic::mojom::FeatureMode::kUnspecified;
  switch (params->details.invocation_source) {
    case api::glic_private::InvocationSource::kUniversalCart:
      if (!base::FeatureList::IsEnabled(
              extensions_features::kApiGlicAccessFromGoogleWebpage)) {
        return RespondNow(Error("Not available for universal cart."));
      }
      source = glic::mojom::InvocationSource::kUniversalCart;
      feature_mode = glic::mojom::FeatureMode::kBluedog;
      break;
    default:
      return RespondNow(Error("Unsupported invocation source."));
  }

  glic::GlicInvokeOptions options{source};
  options.prompts.push_back(GetPromptFromId(params->details.prompt_id));

  options.feature_mode = feature_mode;
  options.target.conversation = glic::NewConversation();

  tabs::TabInterface* tab_interface = nullptr;

  if (params->details.in_new_tab && *params->details.in_new_tab) {
    // Navigate to a new tab.
    NavigateParams navigate_params(profile, GURL("about:blank"),
                                   ui::PAGE_TRANSITION_LINK);
    navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    auto navigation_handle = Navigate(&navigate_params);
    tab_interface = tabs::TabInterface::MaybeGetFromContents(
        navigation_handle->GetWebContents());
  } else {
    // Find the active tab.
    // TODO(b/497936770): Find the tab from the caller. Make sure we actually
    // need it before implement.
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&](BrowserWindowInterface* browser) {
          if (browser->GetProfile() == profile) {
            tab_interface = TabListInterface::From(browser)->GetActiveTab();
            return false;  // Stop iterating.
          }
          return true;  // Continue iterating.
        });
  }

  if (!tab_interface) {
    return RespondNow(Error("No active tab found."));
  }

  options.target.surface = tab_interface;

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile,
                                                         /*create=*/true);
  CHECK(glic_service);

  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      std::move(options));

  return RespondNow(NoArguments());
}

}  // namespace extensions

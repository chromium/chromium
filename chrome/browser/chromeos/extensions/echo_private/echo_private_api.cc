// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/echo_private/echo_private_api.h"

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/echo/echo_util.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/echo_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/echo_private_ash.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chromeos/crosapi/mojom/echo_private.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace echo_api = extensions::api::echo_private;

namespace chromeos {

namespace echo_offer {

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kEchoCheckedOffers);
}

// Removes empty dictionaries from |dict|, potentially nested.
// Does not modify empty lists.
void RemoveEmptyValueDicts(base::Value::Dict& dict) {
  auto it = dict.begin();
  while (it != dict.end()) {
    base::Value& value = it->second;
    if (value.is_dict()) {
      base::Value::Dict& sub_dict = value.GetDict();
      RemoveEmptyValueDicts(sub_dict);
      if (sub_dict.empty()) {
        it = dict.erase(it);
        continue;
      }
    }
    it++;
  }
}

}  // namespace echo_offer

}  // namespace chromeos

EchoPrivateGetRegistrationCodeFunction::
    EchoPrivateGetRegistrationCodeFunction() {}

EchoPrivateGetRegistrationCodeFunction::
    ~EchoPrivateGetRegistrationCodeFunction() {}

ExtensionFunction::ResponseAction
EchoPrivateGetRegistrationCodeFunction::Run() {
  std::optional<echo_api::GetRegistrationCode::Params> params =
      echo_api::GetRegistrationCode::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Possible ECHO code type and corresponding key name in StatisticsProvider.
  const std::string kCouponType = "COUPON_CODE";
  const std::string kGroupType = "GROUP_CODE";
  std::optional<crosapi::mojom::RegistrationCodeType> type;
  if (params->type == kCouponType) {
    type = crosapi::mojom::RegistrationCodeType::kCoupon;
  } else if (params->type == kGroupType) {
    type = crosapi::mojom::RegistrationCodeType::kGroup;
  }

  if (!type) {
    return RespondNow(ArgumentList(
        echo_api::GetRegistrationCode::Results::Create(std::string())));
  }

  auto callback = base::BindOnce(
      &EchoPrivateGetRegistrationCodeFunction::RespondWithResult, this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->echo_private_ash()
      ->GetRegistrationCode(type.value(), std::move(callback));
#else
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::EchoPrivate>() &&
      static_cast<uint32_t>(
          lacros_service->GetInterfaceVersion<crosapi::mojom::EchoPrivate>()) >=
          crosapi::mojom::EchoPrivate::kGetRegistrationCodeMinVersion) {
    lacros_service->GetRemote<crosapi::mojom::EchoPrivate>()
        ->GetRegistrationCode(type.value(), std::move(callback));
  } else {
    return RespondNow(Error("EchoPrivate unavailable."));
  }
#endif
  return RespondLater();
}

void EchoPrivateGetRegistrationCodeFunction::RespondWithResult(
    const std::string& result) {
  Respond(WithArguments(result));
}

EchoPrivateSetOfferInfoFunction::EchoPrivateSetOfferInfoFunction() {}

EchoPrivateSetOfferInfoFunction::~EchoPrivateSetOfferInfoFunction() {}

ExtensionFunction::ResponseAction EchoPrivateSetOfferInfoFunction::Run() {
  std::optional<echo_api::SetOfferInfo::Params> params =
      echo_api::SetOfferInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& service_id = params->id;
  base::Value::Dict dict = params->offer_info.additional_properties.Clone();
  chromeos::echo_offer::RemoveEmptyValueDicts(dict);

  PrefService* local_state = g_browser_process->local_state();
  ScopedDictPrefUpdate offer_update(local_state, prefs::kEchoCheckedOffers);
  offer_update->Set("echo." + service_id, std::move(dict));
  return RespondNow(NoArguments());
}

EchoPrivateGetOfferInfoFunction::EchoPrivateGetOfferInfoFunction() {}

EchoPrivateGetOfferInfoFunction::~EchoPrivateGetOfferInfoFunction() {}

ExtensionFunction::ResponseAction EchoPrivateGetOfferInfoFunction::Run() {
  std::optional<echo_api::GetOfferInfo::Params> params =
      echo_api::GetOfferInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& service_id = params->id;
  PrefService* local_state = g_browser_process->local_state();
  const base::Value::Dict& offer_infos =
      local_state->GetDict(prefs::kEchoCheckedOffers);

  const base::Value* offer_info = offer_infos.Find("echo." + service_id);
  if (!offer_info || !offer_info->is_dict()) {
    return RespondNow(Error("Not found"));
  }

  echo_api::GetOfferInfo::Results::Result result;
  result.additional_properties.Merge(offer_info->GetDict().Clone());
  return RespondNow(
      ArgumentList(echo_api::GetOfferInfo::Results::Create(result)));
}

EchoPrivateGetOobeTimestampFunction::EchoPrivateGetOobeTimestampFunction() {
}

EchoPrivateGetOobeTimestampFunction::~EchoPrivateGetOobeTimestampFunction() {
}

ExtensionFunction::ResponseAction EchoPrivateGetOobeTimestampFunction::Run() {
  chromeos::echo_util::GetOobeTimestamp(base::BindOnce(
      &EchoPrivateGetOobeTimestampFunction::RespondWithResult, this));
  return RespondLater();
}

void EchoPrivateGetOobeTimestampFunction::RespondWithResult(
    std::optional<base::Time> timestamp) {
  if (!timestamp.has_value()) {
    // Returns an empty string on error.
    Respond(WithArguments(std::string()));
    return;
  }
  std::string result = base::UnlocalizedTimeFormatWithPattern(
      timestamp.value(), "y-M-d", icu::TimeZone::getGMT());
  Respond(WithArguments(std::move(result)));
}

EchoPrivateGetUserConsentFunction::EchoPrivateGetUserConsentFunction() =
    default;

EchoPrivateGetUserConsentFunction::~EchoPrivateGetUserConsentFunction() =
    default;

ExtensionFunction::ResponseAction EchoPrivateGetUserConsentFunction::Run() {
  std::optional<echo_api::GetUserConsent::Params> params =
      echo_api::GetUserConsent::Params::Create(args());

  // Verify that the passed origin URL is valid.
  GURL service_origin = GURL(params->consent_requester.origin);
  if (!service_origin.is_valid()) {
    return RespondNow(Error("Invalid origin."));
  }

  content::WebContents* web_contents = nullptr;
  if (!params->consent_requester.tab_id) {
    web_contents = GetSenderWebContents();

    if (!web_contents || extensions::GetViewType(web_contents) !=
                             extensions::mojom::ViewType::kAppWindow) {
      return RespondNow(
          Error("Not called from an app window - the tabId is required."));
    }
  } else {
    TabStripModel* tab_strip = nullptr;
    int tab_index = -1;
    if (!extensions::ExtensionTabUtil::GetTabById(
            *params->consent_requester.tab_id, browser_context(),
            false /*incognito_enabled*/, nullptr /*browser*/, &tab_strip,
            &web_contents, &tab_index)) {
      return RespondNow(Error("Tab not found."));
    }

    // Bail out if the requested tab is not active - the dialog is modal to the
    // window, so showing it for a request from an inactive tab could be
    // misleading/confusing to the user.
    if (tab_index != tab_strip->active_index()) {
      return RespondNow(Error("Consent requested from an inactive tab."));
    }
  }

  DCHECK(web_contents);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->echo_private_ash()
      ->CheckRedeemOffersAllowed(
          web_contents->GetTopLevelNativeWindow(),
          params->consent_requester.service_name,
          params->consent_requester.origin,
          base::BindOnce(&EchoPrivateGetUserConsentFunction::Finalize, this));
#else
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::EchoPrivate>()) {
    const std::string window_id = lacros_window_utility::GetRootWindowUniqueId(
        web_contents->GetTopLevelNativeWindow());
    lacros_service->GetRemote<crosapi::mojom::EchoPrivate>()
        ->CheckRedeemOffersAllowed(
            std::move(window_id), params->consent_requester.service_name,
            params->consent_requester.origin,
            base::BindOnce(&EchoPrivateGetUserConsentFunction::Finalize, this));
  } else {
    return RespondNow(Error("EchoPrivate unavailable."));
  }
#endif
  return RespondLater();
}

void EchoPrivateGetUserConsentFunction::Finalize(bool consent) {
  Respond(WithArguments(consent));
}

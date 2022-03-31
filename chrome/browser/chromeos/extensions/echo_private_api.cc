// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/echo_private_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/echo_private_ash.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/extensions/api/echo_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/system/statistics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"

namespace echo_api = extensions::api::echo_private;

namespace chromeos {

namespace echo_offer {

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kEchoCheckedOffers);
}

}  // namespace echo_offer

}  // namespace chromeos

EchoPrivateGetRegistrationCodeFunction::
    EchoPrivateGetRegistrationCodeFunction() {}

EchoPrivateGetRegistrationCodeFunction::
    ~EchoPrivateGetRegistrationCodeFunction() {}

ExtensionFunction::ResponseValue
EchoPrivateGetRegistrationCodeFunction::GetRegistrationCode(
    const std::string& type) {
  // Possible ECHO code type and corresponding key name in StatisticsProvider.
  const std::string kCouponType = "COUPON_CODE";
  const std::string kGroupType = "GROUP_CODE";

  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  std::string result;
  if (type == kCouponType) {
    provider->GetMachineStatistic(chromeos::system::kOffersCouponCodeKey,
                                  &result);
  } else if (type == kGroupType) {
    provider->GetMachineStatistic(chromeos::system::kOffersGroupCodeKey,
                                  &result);
  }

  return ArgumentList(echo_api::GetRegistrationCode::Results::Create(result));
}

ExtensionFunction::ResponseAction
EchoPrivateGetRegistrationCodeFunction::Run() {
  std::unique_ptr<echo_api::GetRegistrationCode::Params> params =
      echo_api::GetRegistrationCode::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(GetRegistrationCode(params->type));
}

EchoPrivateSetOfferInfoFunction::EchoPrivateSetOfferInfoFunction() {}

EchoPrivateSetOfferInfoFunction::~EchoPrivateSetOfferInfoFunction() {}

ExtensionFunction::ResponseAction EchoPrivateSetOfferInfoFunction::Run() {
  std::unique_ptr<echo_api::SetOfferInfo::Params> params =
      echo_api::SetOfferInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& service_id = params->id;
  std::unique_ptr<base::DictionaryValue> dict =
      params->offer_info.additional_properties.DeepCopyWithoutEmptyChildren();

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate offer_update(local_state, prefs::kEchoCheckedOffers);
  offer_update->SetKey("echo." + service_id,
                       base::Value::FromUniquePtrValue(std::move(dict)));
  return RespondNow(NoArguments());
}

EchoPrivateGetOfferInfoFunction::EchoPrivateGetOfferInfoFunction() {}

EchoPrivateGetOfferInfoFunction::~EchoPrivateGetOfferInfoFunction() {}

ExtensionFunction::ResponseAction EchoPrivateGetOfferInfoFunction::Run() {
  std::unique_ptr<echo_api::GetOfferInfo::Params> params =
      echo_api::GetOfferInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& service_id = params->id;
  PrefService* local_state = g_browser_process->local_state();
  const base::Value* offer_infos =
      local_state->GetDictionary(prefs::kEchoCheckedOffers);

  const base::Value* offer_info =
      offer_infos->FindDictKey("echo." + service_id);
  if (!offer_info) {
    return RespondNow(Error("Not found"));
  }

  echo_api::GetOfferInfo::Results::Result result;
  result.additional_properties.MergeDictionary(offer_info);
  return RespondNow(
      ArgumentList(echo_api::GetOfferInfo::Results::Create(result)));
}

EchoPrivateGetOobeTimestampFunction::EchoPrivateGetOobeTimestampFunction() {
}

EchoPrivateGetOobeTimestampFunction::~EchoPrivateGetOobeTimestampFunction() {
}

ExtensionFunction::ResponseAction EchoPrivateGetOobeTimestampFunction::Run() {
  base::PostTaskAndReplyWithResult(
      extensions::GetExtensionFileTaskRunner().get(), FROM_HERE,
      base::BindOnce(
          &EchoPrivateGetOobeTimestampFunction::GetOobeTimestampOnFileSequence,
          this),
      base::BindOnce(&EchoPrivateGetOobeTimestampFunction::RespondWithResult,
                     this));
  return RespondLater();
}

// Get the OOBE timestamp from file /home/chronos/.oobe_completed.
// The timestamp is used to determine when the user first activates the device.
// If we can get the timestamp info, return it as yyyy-mm-dd, otherwise, return
// an empty string.
std::unique_ptr<base::Value>
EchoPrivateGetOobeTimestampFunction::GetOobeTimestampOnFileSequence() {
  DCHECK(
      extensions::GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  const char kOobeTimestampFile[] = "/home/chronos/.oobe_completed";
  std::string timestamp;
  base::File::Info fileInfo;
  if (base::GetFileInfo(base::FilePath(kOobeTimestampFile), &fileInfo)) {
    base::Time::Exploded ctime;
    fileInfo.creation_time.UTCExplode(&ctime);
    timestamp += base::StringPrintf("%u-%u-%u",
                                    ctime.year,
                                    ctime.month,
                                    ctime.day_of_month);
  }
  return std::make_unique<base::Value>(timestamp);
}

void EchoPrivateGetOobeTimestampFunction::RespondWithResult(
    std::unique_ptr<base::Value> result) {
  Respond(OneArgument(base::Value::FromUniquePtrValue(std::move(result))));
}

EchoPrivateGetUserConsentFunction::EchoPrivateGetUserConsentFunction() =
    default;

EchoPrivateGetUserConsentFunction::~EchoPrivateGetUserConsentFunction() =
    default;

ExtensionFunction::ResponseAction EchoPrivateGetUserConsentFunction::Run() {
  std::unique_ptr<echo_api::GetUserConsent::Params> params =
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
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->echo_private_ash()
      ->CheckRedeemOffersAllowed(
          web_contents->GetTopLevelNativeWindow(),
          params->consent_requester.service_name,
          params->consent_requester.origin,
          base::BindOnce(&EchoPrivateGetUserConsentFunction::Finalize, this));
  return RespondLater();
}

void EchoPrivateGetUserConsentFunction::Finalize(bool consent) {
  Respond(OneArgument(base::Value(consent)));
}

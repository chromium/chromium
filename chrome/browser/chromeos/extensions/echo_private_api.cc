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
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/notifications/echo_dialog_view.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
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
      echo_api::GetRegistrationCode::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(GetRegistrationCode(params->type));
}

EchoPrivateSetOfferInfoFunction::EchoPrivateSetOfferInfoFunction() {}

EchoPrivateSetOfferInfoFunction::~EchoPrivateSetOfferInfoFunction() {}

ExtensionFunction::ResponseAction EchoPrivateSetOfferInfoFunction::Run() {
  std::unique_ptr<echo_api::SetOfferInfo::Params> params =
      echo_api::SetOfferInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& service_id = params->id;
  std::unique_ptr<base::DictionaryValue> dict =
      params->offer_info.additional_properties.DeepCopyWithoutEmptyChildren();

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate offer_update(local_state, prefs::kEchoCheckedOffers);
  offer_update->SetWithoutPathExpansion("echo." + service_id, std::move(dict));
  return RespondNow(NoArguments());
}

EchoPrivateGetOfferInfoFunction::EchoPrivateGetOfferInfoFunction() {}

EchoPrivateGetOfferInfoFunction::~EchoPrivateGetOfferInfoFunction() {}

ExtensionFunction::ResponseAction EchoPrivateGetOfferInfoFunction::Run() {
  std::unique_ptr<echo_api::GetOfferInfo::Params> params =
      echo_api::GetOfferInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& service_id = params->id;
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* offer_infos = local_state->
      GetDictionary(prefs::kEchoCheckedOffers);

  const base::DictionaryValue* offer_info = NULL;
  if (!offer_infos->GetDictionaryWithoutPathExpansion(
         "echo." + service_id, &offer_info)) {
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

EchoPrivateGetUserConsentFunction::EchoPrivateGetUserConsentFunction()
    : redeem_offers_allowed_(false) {
}

// static
scoped_refptr<EchoPrivateGetUserConsentFunction>
EchoPrivateGetUserConsentFunction::CreateForTest(
      const DialogShownTestCallback& dialog_shown_callback) {
  scoped_refptr<EchoPrivateGetUserConsentFunction> function(
      new EchoPrivateGetUserConsentFunction());
  function->dialog_shown_callback_ = dialog_shown_callback;
  return function;
}

EchoPrivateGetUserConsentFunction::~EchoPrivateGetUserConsentFunction() {}

ExtensionFunction::ResponseAction EchoPrivateGetUserConsentFunction::Run() {
  CheckRedeemOffersAllowed();
  return RespondLater();
}

void EchoPrivateGetUserConsentFunction::OnAccept() {
  Finalize(true);
}

void EchoPrivateGetUserConsentFunction::OnCancel() {
  Finalize(false);
}

void EchoPrivateGetUserConsentFunction::OnMoreInfoLinkClicked() {
  NavigateParams params(Profile::FromBrowserContext(browser_context()),
                        GURL(chrome::kEchoLearnMoreURL),
                        ui::PAGE_TRANSITION_LINK);
  // Open the link in a new window. The echo dialog is modal, so the current
  // window is useless until the dialog is closed.
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  Navigate(&params);
}

void EchoPrivateGetUserConsentFunction::CheckRedeemOffersAllowed() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      ash::CrosSettings::Get()->PrepareTrustedValues(base::BindOnce(
          &EchoPrivateGetUserConsentFunction::CheckRedeemOffersAllowed, this));
  if (status == chromeos::CrosSettingsProvider::TEMPORARILY_UNTRUSTED)
    return;

  bool allow = true;
  ash::CrosSettings::Get()->GetBoolean(
      chromeos::kAllowRedeemChromeOsRegistrationOffers, &allow);

  OnRedeemOffersAllowedChecked(allow);
}

void EchoPrivateGetUserConsentFunction::OnRedeemOffersAllowedChecked(
    bool is_allowed) {
  redeem_offers_allowed_ = is_allowed;

  std::unique_ptr<echo_api::GetUserConsent::Params> params =
      echo_api::GetUserConsent::Params::Create(*args_);

  // Verify that the passed origin URL is valid.
  GURL service_origin = GURL(params->consent_requester.origin);
  if (!service_origin.is_valid()) {
    Respond(Error("Invalid origin."));
    return;
  }

  content::WebContents* web_contents = nullptr;
  if (!params->consent_requester.tab_id) {
    web_contents = GetSenderWebContents();

    if (!web_contents || extensions::GetViewType(web_contents) !=
                             extensions::mojom::ViewType::kAppWindow) {
      Respond(Error("Not called from an app window - the tabId is required."));
      return;
    }
  } else {
    TabStripModel* tab_strip = nullptr;
    int tab_index = -1;
    if (!extensions::ExtensionTabUtil::GetTabById(
            *params->consent_requester.tab_id, browser_context(),
            false /*incognito_enabled*/, nullptr /*browser*/, &tab_strip,
            &web_contents, &tab_index)) {
      Respond(Error("Tab not found."));
      return;
    }

    // Bail out if the requested tab is not active - the dialog is modal to the
    // window, so showing it for a request from an inactive tab could be
    // misleading/confusing to the user.
    if (tab_index != tab_strip->active_index()) {
      Respond(Error("Consent requested from an inactive tab."));
      return;
    }
  }

  DCHECK(web_contents);

  // Add ref to ensure the function stays around until the dialog listener is
  // called. The reference is release in |Finalize|.
  AddRef();

  // Create and show the dialog.
  ash::EchoDialogView::Params dialog_params;
  dialog_params.echo_enabled = redeem_offers_allowed_;
  if (dialog_params.echo_enabled) {
    dialog_params.service_name =
        base::UTF8ToUTF16(params->consent_requester.service_name);
    dialog_params.origin = base::UTF8ToUTF16(params->consent_requester.origin);
  }

  ash::EchoDialogView* dialog = new ash::EchoDialogView(this, dialog_params);
  dialog->Show(web_contents->GetTopLevelNativeWindow());

  // If there is a dialog_shown_callback_, invoke it with the created dialog.
  if (!dialog_shown_callback_.is_null())
    dialog_shown_callback_.Run(dialog);
}

void EchoPrivateGetUserConsentFunction::Finalize(bool consent) {
  // Consent should not be true if offers redeeming is disabled.
  CHECK(redeem_offers_allowed_ || !consent);
  Respond(OneArgument(base::Value(consent)));

  // Release the reference added in |OnRedeemOffersAllowedChecked|, before
  // showing the dialog.
  Release();
}

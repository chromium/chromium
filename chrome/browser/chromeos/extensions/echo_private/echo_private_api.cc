// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/echo_private/echo_private_api.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/notifications/echo_dialog_view.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/echo_private/echo_private_api_util.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/extensions/api/echo_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/ash/components/report/utils/time_utils.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/aura/window.h"

namespace {
std::string GetRegistrationCode(std::string_view type) {
  // Possible ECHO code type and corresponding key name in StatisticsProvider.
  const std::string kCouponType = "COUPON_CODE";
  const std::string kGroupType = "GROUP_CODE";

  std::string_view name;
  if (type == kCouponType) {
    name = ash::system::kOffersCouponCodeKey;
  } else if (type == kGroupType) {
    name = ash::system::kOffersGroupCodeKey;
  } else {
    return std::string();
  }

  ash::system::StatisticsProvider* provider =
      ash::system::StatisticsProvider::GetInstance();
  const std::optional<std::string_view> offers_code =
      provider->GetMachineStatistic(name);

  return std::string(offers_code.value_or(""));
}
}  // namespace

namespace echo_api = extensions::api::echo_private;

EchoPrivateGetRegistrationCodeFunction::
    EchoPrivateGetRegistrationCodeFunction() = default;

EchoPrivateGetRegistrationCodeFunction::
    ~EchoPrivateGetRegistrationCodeFunction() = default;

ExtensionFunction::ResponseAction
EchoPrivateGetRegistrationCodeFunction::Run() {
  std::optional<echo_api::GetRegistrationCode::Params> params =
      echo_api::GetRegistrationCode::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  return RespondNow(ArgumentList(echo_api::GetRegistrationCode::Results::Create(
      GetRegistrationCode(params->type))));
}

EchoPrivateSetOfferInfoFunction::EchoPrivateSetOfferInfoFunction() = default;

EchoPrivateSetOfferInfoFunction::~EchoPrivateSetOfferInfoFunction() = default;

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

EchoPrivateGetOfferInfoFunction::EchoPrivateGetOfferInfoFunction() = default;

EchoPrivateGetOfferInfoFunction::~EchoPrivateGetOfferInfoFunction() = default;

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

EchoPrivateGetOobeTimestampFunction::EchoPrivateGetOobeTimestampFunction() =
    default;

EchoPrivateGetOobeTimestampFunction::~EchoPrivateGetOobeTimestampFunction() =
    default;

ExtensionFunction::ResponseAction EchoPrivateGetOobeTimestampFunction::Run() {
  std::optional<base::Time> timestamp =
      ash::report::utils::GetFirstActiveWeek();

  if (!timestamp.has_value()) {
    // Returns an empty string on error.
    return RespondNow(WithArguments(std::string()));
  }
  std::string result = base::UnlocalizedTimeFormatWithPattern(
      timestamp.value(), "y-M-d", icu::TimeZone::getGMT());
  return RespondNow(WithArguments(std::move(result)));
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
    extensions::WindowController* window = nullptr;
    int tab_index = -1;
    if (!extensions::ExtensionTabUtil::GetTabById(
            *params->consent_requester.tab_id, browser_context(),
            false /*incognito_enabled*/, &window, &web_contents, &tab_index) ||
        !window) {
      return RespondNow(Error("Tab not found."));
    }

    // Bail out if the requested tab is not active - the dialog is modal to the
    // window, so showing it for a request from an inactive tab could be
    // misleading/confusing to the user.
    if (tab_index != window->GetBrowser()->tab_strip_model()->active_index()) {
      return RespondNow(Error("Consent requested from an inactive tab."));
    }
  }

  DCHECK(web_contents);

  ash::CrosSettingsProvider::TrustedStatus status =
      ash::CrosSettings::Get()->PrepareTrustedValues(base::BindOnce(
          &EchoPrivateGetUserConsentFunction::DidPrepareTrustedValues, this,
          web_contents->GetTopLevelNativeWindow(),
          params->consent_requester.service_name,
          params->consent_requester.origin));

  if (status == ash::CrosSettingsProvider::TRUSTED) {
    // Callback was dropped in this case (because it gets called only when
    // status isn't TRUSTED). Manually invoke.
    DidPrepareTrustedValues(web_contents->GetTopLevelNativeWindow(),
                            params->consent_requester.service_name,
                            params->consent_requester.origin);
  }

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void EchoPrivateGetUserConsentFunction::DidPrepareTrustedValues(
    aura::Window* window,
    std::string_view service_name,
    std::string_view origin) {
  ash::CrosSettingsProvider::TrustedStatus status =
      ash::CrosSettings::Get()->PrepareTrustedValues(base::NullCallback());
  if (status != ash::CrosSettingsProvider::TRUSTED) {
    Respond(WithArguments(false));
    return;
  }

  bool allow = true;
  ash::CrosSettings::Get()->GetBoolean(
      ash::kAllowRedeemChromeOsRegistrationOffers, &allow);

  // Create and show the dialog.
  ash::EchoDialogView::Params dialog_params;
  dialog_params.echo_enabled = allow;
  if (allow) {
    dialog_params.service_name = base::UTF8ToUTF16(service_name);
    dialog_params.origin = base::UTF8ToUTF16(origin);
  }

  // Add ref to ensure the function stays around until the dialog listener is
  // called. The reference is released in |Finalize|.
  AddRef();
  ash::EchoDialogView* dialog = new ash::EchoDialogView(this, dialog_params);
  dialog->Show(window);
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

void EchoPrivateGetUserConsentFunction::Finalize(bool consent) {
  Respond(WithArguments(consent));

  // Release the reference added in |DidPrepareTrustedValues|, before showing
  // the dialog.
  Release();
}

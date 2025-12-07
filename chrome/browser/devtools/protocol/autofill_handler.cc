// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/autofill_handler.h"

#include <optional>
#include <variant>

#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/protocol/autofill.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/addresses/field_filling_address_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/manual_testing_import.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/features.h"

namespace {

using protocol::Response;

std::optional<autofill::FormData> FindFormDataWithField(
    autofill::ContentAutofillDriver* driver,
    autofill::FieldGlobalId id) {
  if (!driver) {
    return std::nullopt;
  }
  for (const auto& [key, form] :
       driver->GetAutofillManager().form_structures()) {
    for (const auto& field : form->fields()) {
      if (field->global_id() == id) {
        return form->ToFormData();
      }
    }
  }
  return std::nullopt;
}

std::optional<std::string> GetRenderFrameDevtoolsToken(
    const std::string& target_id,
    const std::string& frame_token) {
  auto host = content::DevToolsAgentHost::GetForId(target_id);
  CHECK(host);

  std::string result;
  host->GetWebContents()->GetOutermostWebContents()->ForEachRenderFrameHost(
      [&result, &frame_token](content::RenderFrameHost* rfh) {
        if (rfh->GetFrameToken().ToString() == frame_token) {
          result = rfh->GetDevToolsFrameToken().ToString();
        }
      });
  return result;
}

}  // namespace

AutofillHandler::AutofillHandler(protocol::UberDispatcher* dispatcher,
                                 const std::string& target_id)
    : target_id_(target_id) {
  protocol::Autofill::Dispatcher::wire(dispatcher, this);
  frontend_ =
      std::make_unique<protocol::Autofill::Frontend>(dispatcher->channel());
}

AutofillHandler::~AutofillHandler() {
  Disable();
}

void AutofillHandler::Trigger(
    int field_id,
    std::optional<String> frame_id,
    std::unique_ptr<protocol::Autofill::CreditCard> card,
    std::unique_ptr<protocol::Autofill::Address> address,
    std::unique_ptr<TriggerCallback> callback) {
  auto host = content::DevToolsAgentHost::GetForId(target_id_);
  if (!host) {
    callback->sendFailure(Response::ServerError("Target not found"));
    return;
  }

  content::RenderFrameHost* outermost_primary_rfh =
      host->GetWebContents()->GetOutermostWebContents()->GetPrimaryMainFrame();
  content::RenderFrameHost* frame_rfh = nullptr;

  if (frame_id.has_value()) {
    outermost_primary_rfh->ForEachRenderFrameHost(
        [&frame_id, &frame_rfh](content::RenderFrameHost* rfh) {
          if (rfh->GetDevToolsFrameToken().ToString() == frame_id.value()) {
            frame_rfh = rfh;
          }
        });
    if (!frame_rfh) {
      callback->sendFailure(Response::ServerError("Frame not found"));
      return;
    }
  } else {
    frame_rfh = outermost_primary_rfh;
  }

  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(frame_rfh);
  if (!autofill_driver) {
    callback->sendFailure(
        Response::ServerError("RenderFrameHost is being destroyed"));
    return;
  }

  // The form extraction is done by AutofillAgent in renderer process and might
  // not be finished yet, so we need to trigger it and to wait for it to finish
  // before continuing. See https://crbug.com/442016685.
  autofill_driver->GetAutofillManager().TriggerFormExtractionInAllFrames(
      base::BindOnce(&AutofillHandler::ContinueTrigger,
                     weak_ptr_factory_.GetWeakPtr(), frame_rfh, field_id,
                     frame_id, std::move(card), std::move(address),
                     std::move(callback)));
}

void AutofillHandler::ContinueTrigger(
    content::RenderFrameHost* frame_rfh,
    int field_id,
    std::optional<String> frame_id,
    std::unique_ptr<protocol::Autofill::CreditCard> card,
    std::unique_ptr<protocol::Autofill::Address> address,
    std::unique_ptr<TriggerCallback> callback,
    bool success) {
  autofill::LocalFrameToken frame_token(frame_rfh->GetFrameToken().value());
  autofill::FieldGlobalId global_field_id = {
      frame_token, autofill::FieldRendererId(field_id)};

  autofill::ContentAutofillDriver* autofill_driver = nullptr;
  std::optional<autofill::FormData> form;
  while (frame_rfh) {
    autofill_driver =
        autofill::ContentAutofillDriver::GetForRenderFrameHost(frame_rfh);

    // TODO(alexrudenko): This approach might lead to differences in behaviour
    // between the real Autofill flow triggered manually and Autofill triggered
    // over CDP. We should change how we find the form data and use the same
    // logic as used by AutofillDriverRouter.
    if (std::optional<autofill::FormData> rfh_form_data =
            FindFormDataWithField(autofill_driver, global_field_id)) {
      form = std::move(rfh_form_data);
    }

    frame_rfh = frame_rfh->GetParent();
  }

  if (!form.has_value()) {
    callback->sendFailure(Response::InvalidRequest("Field not found"));
    return;
  }

  if (!autofill_driver) {
    callback->sendFailure(
        Response::ServerError("RenderFrameHost is being destroyed"));
    return;
  }

  // Validate that card and address are mutually exclusive.
  if (card && address) {
    callback->sendFailure(
        Response::InvalidRequest("Card and address cannot both be provided"));
    return;
  }

  if (!card && !address) {
    callback->sendFailure(
        Response::InvalidRequest("Either card or address must be provided"));
    return;
  }

  if (card) {
    autofill::CreditCard tmp_autofill_card;
    tmp_autofill_card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                                 base::UTF8ToUTF16(card->GetNumber()));
    tmp_autofill_card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                                 base::UTF8ToUTF16(card->GetName()));
    tmp_autofill_card.SetRawInfo(autofill::CREDIT_CARD_EXP_MONTH,
                                 base::UTF8ToUTF16(card->GetExpiryMonth()));
    tmp_autofill_card.SetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
                                 base::UTF8ToUTF16(card->GetExpiryYear()));
    tmp_autofill_card.SetRawInfo(autofill::CREDIT_CARD_VERIFICATION_CODE,
                                 base::UTF8ToUTF16(card->GetCvc()));
    static_cast<autofill::BrowserAutofillManager&>(
        autofill_driver->GetAutofillManager())
        .FillOrPreviewForm(autofill::mojom::ActionPersistence::kFill, *form,
                           global_field_id, &tmp_autofill_card,
                           autofill::AutofillTriggerSource::kDevtools);
  }
  if (address) {
    std::string country_code =
        autofill::i18n_model_definition::kLegacyHierarchyCountryCodeString;
    for (const auto& field : *address->GetFields()) {
      if (field->GetName() == "ADDRESS_HOME_COUNTRY") {
        country_code = field->GetValue();
        break;
      }
    }
    autofill::AddressCountryCode address_country_code(country_code);
    autofill::AutofillProfile tmp_autofill_profile(address_country_code);
    for (const auto& field : *address->GetFields()) {
      std::string field_name = field->GetName();
      std::string field_value = field->GetValue();

      // Map field names to FieldType and set them.
      autofill::FieldType field_type =
          autofill::TypeNameToFieldType(field_name);
      if (!IsAddressType(field_type)) {
        callback->sendFailure(
            Response::InvalidRequest("Unsupported field type: " + field_name));
        return;
      }
      tmp_autofill_profile.SetRawInfo(field_type,
                                      base::UTF8ToUTF16(field_value));
    }
    static_cast<autofill::BrowserAutofillManager&>(
        autofill_driver->GetAutofillManager())
        .FillOrPreviewForm(autofill::mojom::ActionPersistence::kFill, *form,
                           global_field_id, &tmp_autofill_profile,
                           autofill::AutofillTriggerSource::kDevtools);
  }

  callback->sendSuccess();
}

void AutofillHandler::SetAddresses(
    std::unique_ptr<protocol::Array<protocol::Autofill::Address>> addresses,
    std::unique_ptr<SetAddressesCallback> callback) {
  if (!content::DevToolsAgentHost::GetForId(target_id_)) {
    std::move(callback)->sendFailure(Response::ServerError("Target not found"));
    return;
  }

  std::vector<autofill::AutofillProfile> test_address_for_countries;
  base::Value::List profiles;

  for (const auto& address : *addresses) {
    base::Value::Dict address_fields;
    for (const auto& field : *address->GetFields()) {
      address_fields.Set(field->GetName(), field->GetValue());
    }
    profiles.Append(std::move(address_fields));
  }

  std::optional<std::vector<autofill::AutofillProfile>> autofill_profiles =
      autofill::AutofillProfilesFromJSON(&profiles);
  if (autofill_profiles) {
    const std::string locale = "en-US";
    for (autofill::AutofillProfile& profile : *autofill_profiles) {
      const std::u16string test_address_country =
          profile.GetInfo(autofill::FieldType::ADDRESS_HOME_COUNTRY, locale);
      // The current test address for Germany is based on the old model. If the
      // new model is enabled we should not offer it in the list of
      // available addresses.
      // TODO(b/40270486): Offer a test address version for when the new model
      // is enabled.
      if (test_address_country == u"Germany") {
        continue;
      }
      profile.set_is_devtools_testing_profile(true);
      test_address_for_countries.push_back(profile);
    }
  }

  autofill::ContentAutofillDriver* autofill_driver = GetAutofillDriver();
  if (!autofill_driver) {
    std::move(callback)->sendFailure(
        Response::ServerError("RenderFrameHost is being destroyed"));
    return;
  }

  static_cast<autofill::BrowserAutofillManager&>(
      autofill_driver->GetAutofillManager())
      .client()
      .set_test_addresses(test_address_for_countries);
  std::move(callback)->sendSuccess();
}

void AutofillHandler::OnFillOrPreviewForm(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id,
    autofill::mojom::ActionPersistence action_persistence,
    const base::flat_set<autofill::FieldGlobalId>& filled_field_ids,
    const autofill::FillingPayload& filling_payload) {
  // We only care about address forms that were filled.
  if (action_persistence != autofill::mojom::ActionPersistence::kFill ||
      !std::holds_alternative<const autofill::AutofillProfile*>(
          filling_payload)) {
    return;
  }

  autofill::FormStructure& form =
      CHECK_DEREF(manager.FindCachedFormById(form_id));
  const autofill::AutofillProfile* profile_used_to_fill_form =
      std::get<const autofill::AutofillProfile*>(filling_payload);

  auto filled_form_ids = [&] {
    base::flat_set<autofill::FormGlobalId> ids;
    for (const std::unique_ptr<autofill::AutofillField>& field : form) {
      if (filled_field_ids.contains(field->global_id())) {
        ids.insert(field->renderer_form_id());
      }
    }
    return ids;
  }();

  // Devtools is already in English, so we can default the locale to en-US.
  const std::string locale = "en-US";

  auto filled_fields_to_be_sent_to_devtools =
      std::make_unique<protocol::Array<protocol::Autofill::FilledField>>();
  filled_fields_to_be_sent_to_devtools->reserve(filled_field_ids.size());
  for (const std::unique_ptr<autofill::AutofillField>& field : form) {
    // `form_structure` may contains fields from multiple forms, filter out
    // fields from forms that have no autofilled fields as irrelevant.
    if (!filled_form_ids.contains(field->renderer_form_id())) {
      continue;
    }
    autofill::FieldTypeSet field_types = field->Type().GetTypes();
    // Whether the field was classified from the autocomplete attribute or
    // predictions. If no autocomplete attribute exists OR the actual ServerType
    // differs from what it would have been with only autocomplete, autofill
    // inferred the type.
    bool autofill_inferred =
        field->html_type() == autofill::HtmlFieldType::kUnspecified ||
        field->html_type() == autofill::HtmlFieldType::kUnrecognized ||
        !field_types.contains(
            autofill::HtmlFieldTypeToBestCorrespondingFieldType(
                field->html_type()));

    std::vector<std::string_view> field_type_strings = base::ToVector(
        field_types, &autofill::FieldTypeToDeveloperRepresentationString);
    std::erase(field_type_strings, "");
    // TODO(crbug.com/436489442): Reading the filled value from the profile is
    // brittle because it could diverge from the values that are actually sent
    // to the renderer. A more robust solution should be found.
    std::u16string filled_value = u"";
    if (filled_field_ids.contains(field->global_id())) {
      std::string failure_to_fill;
      filled_value =
          autofill::GetFillingValueAndTypeForProfile(
              *profile_used_to_fill_form, locale, field->Type(), *field,
              manager.client().GetAddressNormalizer(), &failure_to_fill)
              .first;
    }
    filled_fields_to_be_sent_to_devtools->push_back(
        protocol::Autofill::FilledField::Create()
            .SetId(base::UTF16ToUTF8(field->id_attribute()))
            .SetName(base::UTF16ToUTF8(field->name_attribute()))
            .SetValue(base::UTF16ToUTF8(filled_value))
            .SetHtmlType(std::string(
                autofill::FormControlTypeToString(field->form_control_type())))
            .SetAutofillType(base::JoinString(field_type_strings, ", "))
            .SetFillingStrategy(
                autofill_inferred
                    ? protocol::Autofill::FillingStrategyEnum::AutofillInferred
                    : protocol::Autofill::FillingStrategyEnum::
                          AutocompleteAttribute)
            .SetFrameId(
                GetRenderFrameDevtoolsToken(
                    target_id_, field->global_id().frame_token->ToString())
                    .value_or(""))
            .SetFieldId(field->renderer_id().value())
            .Build());
  }

  // Send profile information to devtools so that it can build the UI.
  // We use the same format we see in the settings page.
  std::vector<std::vector<autofill::AutofillAddressUIComponent>> components;
  autofill::GetAddressComponents(
      base::UTF16ToUTF8(profile_used_to_fill_form->GetInfo(
          autofill::FieldType::ADDRESS_HOME_COUNTRY, locale)),
      locale,
      /*include_literals=*/false, &components, nullptr);

  // `profile_address_fields` is used to represent a profile as seen in the
  // settings page. It consists of a 2D array where each inner array is used
  // build a "profile line". The following `profile_address_fields` for
  // instance:
  // [[{name: "GIVE_NAME", value: "Jon"}, {name: "FAMILY_NAME", value: "Doe"}],
  // [{name: "CITY", value: "Munich"}, {name: "ZIP", value: "81456"}]] should
  // allow the receiver to render:
  // Jon Doe
  // Munich 81456
  auto profile_address_fields =
      std::make_unique<protocol::Array<protocol::Autofill::AddressFields>>();
  for (const std::vector<autofill::AutofillAddressUIComponent>& line :
       components) {
    auto profile_values =
        std::make_unique<protocol::Array<protocol::Autofill::AddressField>>();
    profile_values->reserve(line.size());
    for (const autofill::AutofillAddressUIComponent& component : line) {
      profile_values->push_back(
          protocol::Autofill::AddressField::Create()
              .SetName(FieldTypeToString(component.field))
              .SetValue(base::UTF16ToUTF8(
                  profile_used_to_fill_form->GetInfo(component.field, locale)))
              .Build());
    }
    profile_address_fields->push_back(
        protocol::Autofill::AddressFields::Create()
            .SetFields(std::move(profile_values))
            .Build());
  }
  // Insert the country at the end. This is required because it is not part of
  // `AutofillAddressUIComponent`.
  auto country_line =
      std::make_unique<protocol::Array<protocol::Autofill::AddressField>>();
  country_line->push_back(
      protocol::Autofill::AddressField::Create()
          .SetName(FieldTypeToString(autofill::FieldType::ADDRESS_HOME_COUNTRY))
          .SetValue(base::UTF16ToUTF8(profile_used_to_fill_form->GetInfo(
              autofill::FieldType::ADDRESS_HOME_COUNTRY, locale)))
          .Build());
  profile_address_fields->push_back(protocol::Autofill::AddressFields::Create()
                                        .SetFields(std::move(country_line))
                                        .Build());
  // Similarly to the `ADDRESS_HOME_COUNTRY`, also include
  // `PHONE_HOME_WHOLE_NUMBER` and `EMAIL_ADDRESS`. However in a single line.
  auto phone_and_email_line =
      std::make_unique<protocol::Array<protocol::Autofill::AddressField>>();
  phone_and_email_line->push_back(
      protocol::Autofill::AddressField::Create()
          .SetName(
              FieldTypeToString(autofill::FieldType::PHONE_HOME_WHOLE_NUMBER))
          .SetValue(base::UTF16ToUTF8(profile_used_to_fill_form->GetInfo(
              autofill::FieldType::PHONE_HOME_WHOLE_NUMBER, locale)))
          .Build());
  phone_and_email_line->push_back(
      protocol::Autofill::AddressField::Create()
          .SetName(FieldTypeToString(autofill::FieldType::EMAIL_ADDRESS))
          .SetValue(base::UTF16ToUTF8(profile_used_to_fill_form->GetInfo(
              autofill::FieldType::EMAIL_ADDRESS, locale)))
          .Build());

  profile_address_fields->push_back(
      protocol::Autofill::AddressFields::Create()
          .SetFields(std::move(phone_and_email_line))
          .Build());

  frontend_->AddressFormFilled(
      std::move(filled_fields_to_be_sent_to_devtools),
      protocol::Autofill::AddressUI::Create()
          .SetAddressFields(std::move(profile_address_fields))
          .Build());
}

void AutofillHandler::OnAutofillManagerStateChanged(
    autofill::AutofillManager& manager,
    autofill::AutofillManager::LifecycleState old_state,
    autofill::AutofillManager::LifecycleState new_state) {
  using enum autofill::AutofillManager::LifecycleState;
  switch (new_state) {
    case kInactive:
    case kActive:
    case kPendingReset:
      break;
    case kPendingDeletion:
      autofill_manager_observation_.Reset();
      break;
  }
}

void AutofillHandler::OnContentAutofillDriverFactoryDestroyed(
    autofill::ContentAutofillDriverFactory& factory) {
  autofill_manager_observation_.Reset();
}

void AutofillHandler::OnContentAutofillDriverCreated(
    autofill::ContentAutofillDriverFactory&,
    autofill::ContentAutofillDriver& new_driver) {
  // If the outermost frame driver (returned by `GetAutofillDriver()`) was
  // recreated (e.g. happens after certain navigations) we need to resubscribe
  // to the autofill manager, which is also recreated, to keep getting observer
  // callbacks like `OnFillOrPreviewForm`.
  if (enabled_ && &new_driver == GetAutofillDriver()) {
    autofill_manager_observation_.Reset();
    autofill_manager_observation_.Observe(&new_driver.GetAutofillManager());
  }
}

autofill::ContentAutofillDriver* AutofillHandler::GetAutofillDriver() {
  auto host = content::DevToolsAgentHost::GetForId(target_id_);
  CHECK(host);

  if (!host->GetWebContents()) {
    return nullptr;
  }

  content::RenderFrameHost* outermost_primary_rfh =
      host->GetWebContents()->GetOutermostWebContents()->GetPrimaryMainFrame();

  return autofill::ContentAutofillDriver::GetForRenderFrameHost(
      outermost_primary_rfh);
}

autofill::AutofillClient* AutofillHandler::GetAutofillClient() {
  auto host = content::DevToolsAgentHost::GetForId(target_id_);
  CHECK(host);

  if (!host->GetWebContents()) {
    return nullptr;
  }

  return autofill::ContentAutofillClient::FromWebContents(
      host->GetWebContents());
}

Response AutofillHandler::Enable() {
  if (enabled_) {
    return Response::Success();
  }

  enabled_ = true;

  auto host = content::DevToolsAgentHost::GetForId(target_id_);
  CHECK(host);

  autofill::ContentAutofillDriver* driver = GetAutofillDriver();
  if (driver && host->GetType() == content::DevToolsAgentHost::kTypePage) {
    factory_observation_.Observe(
        autofill::ContentAutofillDriverFactory::FromWebContents(
            host->GetWebContents()));
    autofill_manager_observation_.Observe(&driver->GetAutofillManager());
  }

  return Response::Success();
}

Response AutofillHandler::Disable() {
  enabled_ = false;
  autofill_manager_observation_.Reset();
  autofill::AutofillClient* autofill_client = GetAutofillClient();
  if (autofill_client) {
    autofill_client->set_test_addresses({});
  }
  return Response::Success();
}

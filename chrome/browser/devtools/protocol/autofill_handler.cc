// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/autofill_handler.h"

#include "base/check_deref.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/protocol/autofill.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/manual_testing_import.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using autofill::AutofillField;
using autofill::AutofillTriggerSource;
using autofill::CreditCard;
using autofill::FieldGlobalId;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::HtmlFieldTypeToBestCorrespondingServerFieldType;
using autofill::mojom::HtmlFieldType;
using protocol::Maybe;
using protocol::Response;

namespace {

absl::optional<std::pair<FormData, FormFieldData>> FindFieldWithFormData(
    autofill::ContentAutofillDriver* driver,
    autofill::FieldGlobalId id) {
  if (!driver) {
    return absl::nullopt;
  }
  for (const auto& [key, form] :
       driver->GetAutofillManager().form_structures()) {
    for (const auto& field : form->fields()) {
      if (field->global_id() == id) {
        return std::make_pair(form->ToFormData(), FormFieldData(*field));
      }
    }
  }
  return absl::nullopt;
}

}  // namespace

AutofillHandler::AutofillHandler(protocol::UberDispatcher* dispatcher,
                                 const std::string& target_id)
    : target_id_(target_id) {
  protocol::Autofill::Dispatcher::wire(dispatcher, this);

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillTestFormWithDevtools)) {
    frontend_ =
        std::make_unique<protocol::Autofill::Frontend>(dispatcher->channel());
  }
}

AutofillHandler::~AutofillHandler() = default;

void AutofillHandler::Trigger(
    int field_id,
    Maybe<String> frame_id,
    std::unique_ptr<protocol::Autofill::CreditCard> card,
    std::unique_ptr<TriggerCallback> callback) {
  auto host = content::DevToolsAgentHost::GetForId(target_id_);
  if (!host) {
    std::move(callback)->sendFailure(Response::ServerError("Target not found"));
    return;
  }
  host->GetUniqueFormControlId(
      field_id,
      base::BindOnce(&AutofillHandler::FinishTrigger,
                     weak_ptr_factory_.GetWeakPtr(), std::move(frame_id),
                     std::move(card), std::move(callback)));
}

void AutofillHandler::FinishTrigger(
    Maybe<String> frame_id,
    std::unique_ptr<protocol::Autofill::CreditCard> card,
    std::unique_ptr<TriggerCallback> callback,
    uint64_t field_id) {
  auto host = content::DevToolsAgentHost::GetForId(target_id_);
  if (!host) {
    std::move(callback)->sendFailure(Response::ServerError("Target not found"));
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
      std::move(callback)->sendFailure(
          Response::ServerError("Frame not found"));
      return;
    }
  } else {
    frame_rfh = outermost_primary_rfh;
  }

  autofill::LocalFrameToken frame_token(frame_rfh->GetFrameToken().value());
  autofill::FieldGlobalId global_field_id = {
      frame_token, autofill::FieldRendererId(field_id)};

  autofill::ContentAutofillDriver* autofill_driver = nullptr;
  absl::optional<std::pair<FormData, FormFieldData>> field_data;
  while (frame_rfh) {
    autofill_driver =
        autofill::ContentAutofillDriver::GetForRenderFrameHost(frame_rfh);

    // TODO(alexrudenko): This approach might lead to differences in behaviour
    // between the real Autofill flow triggered manually and Autofill triggered
    // over CDP. We should change how we find the form data and use the same
    // logic as used by AutofillDriverRouter.
    if (absl::optional<std::pair<FormData, FormFieldData>> rfh_field_data =
            FindFieldWithFormData(autofill_driver, global_field_id)) {
      field_data = std::move(rfh_field_data);
    }

    frame_rfh = frame_rfh->GetParent();
  }

  if (!field_data.has_value()) {
    std::move(callback)->sendFailure(
        Response::InvalidRequest("Field not found"));
    return;
  }

  if (!autofill_driver) {
    std::move(callback)->sendFailure(
        Response::ServerError("RenderFrameHost is being destroyed"));
    return;
  }

  CreditCard tmp_autofill_card;
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
      .FillCreditCardForm(field_data->first, field_data->second,
                          tmp_autofill_card, base::UTF8ToUTF16(card->GetCvc()),
                          {.trigger_source = AutofillTriggerSource::kPopup});

  std::move(callback)->sendSuccess();
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

  absl::optional<std::vector<autofill::AutofillProfile>> autofill_profiles =
      autofill::AutofillProfilesFromJSON(&profiles);
  if (autofill_profiles) {
    for (const autofill::AutofillProfile& profile : *autofill_profiles) {
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
      .GetPersonalDataManager()
      ->set_test_addresses(test_address_for_countries);
  std::move(callback)->sendSuccess();
}

void AutofillHandler::OnFillOrPreviewDataModelForm(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form,
    autofill::mojom::ActionPersistence action_persistence,
    base::span<const FormFieldData* const> filled_fields,
    absl::variant<const autofill::AutofillProfile*, const autofill::CreditCard*>
        profile_or_credit_card) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillTestFormWithDevtools)) {
    return;
  }

  // We only care about address forms that were filled.
  if (action_persistence != autofill::mojom::ActionPersistence::kFill ||
      !absl::holds_alternative<const autofill::AutofillProfile*>(
          profile_or_credit_card)) {
    return;
  }

  autofill::FormStructure& form_structure =
      CHECK_DEREF(manager.FindCachedFormById(form));
  const autofill::AutofillProfile* profile_used_to_fill_form =
      absl::get<const autofill::AutofillProfile*>(profile_or_credit_card);

  auto filled_fields_to_be_sent_to_devtools =
      std::make_unique<protocol::Array<protocol::Autofill::FilledField>>();
  filled_fields_to_be_sent_to_devtools->reserve(filled_fields.size());
  for (const FormFieldData* field : filled_fields) {
    AutofillField* autofill_field =
        form_structure.GetFieldById(field->global_id());
    if (!autofill_field) {
      continue;
    }
    // Whether the field was classified from the autocomplete attribute or
    // predictions. If no autocomplete attribute exists OR the actual ServerType
    // differs from what it would have been with only autocomplete, autofill
    // inferred the type.
    bool autofill_inferred =
        autofill_field->html_type() == HtmlFieldType::kUnspecified ||
        autofill_field->html_type() == HtmlFieldType::kUnrecognized ||
        HtmlFieldTypeToBestCorrespondingServerFieldType(
            autofill_field->html_type()) !=
            autofill_field->Type().GetStorableType();
    filled_fields_to_be_sent_to_devtools->push_back(
        protocol::Autofill::FilledField::Create()
            .SetId(base::UTF16ToASCII(autofill_field->id_attribute))
            .SetName(base::UTF16ToASCII(autofill_field->name_attribute))
            .SetValue(base::UTF16ToASCII(field->value))
            .SetHtmlType(std::string(
                autofill::FormControlTypeToString(field->form_control_type)))
            .SetAutofillType(
                std::string(FieldTypeToDeveloperRepresentationString(
                    autofill_field->Type().GetStorableType())))
            .SetFillingStrategy(
                autofill_inferred
                    ? protocol::Autofill::FillingStrategyEnum::AutofillInferred
                    : protocol::Autofill::FillingStrategyEnum::
                          AutocompleteAttribute)
            .Build());
  }

  // Send profile information to devtools so that it can build the UI.
  // We use the same format we see in the settings page.
  std::vector<std::vector<autofill::AutofillAddressUIComponent>> components;
  // Devtools is already in english, so we can default the local to en-US.
  const std::string locale = "en-US";
  autofill::GetAddressComponents(
      base::UTF16ToASCII(profile_used_to_fill_form->GetInfo(
          autofill::ServerFieldType::ADDRESS_HOME_COUNTRY, locale)),
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
              .SetValue(base::UTF16ToASCII(
                  profile_used_to_fill_form->GetInfo(component.field, locale)))
              .Build());
    }
    profile_address_fields->push_back(
        protocol::Autofill::AddressFields::Create()
            .SetFields(std::move(profile_values))
            .Build());
  }
  frontend_->AddressFormFilled(
      std::move(filled_fields_to_be_sent_to_devtools),
      protocol::Autofill::AddressUI::Create()
          .SetAddressFields(std::move(profile_address_fields))
          .Build());
}

autofill::ContentAutofillDriver* AutofillHandler::GetAutofillDriver() {
  auto host = content::DevToolsAgentHost::GetForId(target_id_);
  CHECK(host);

  content::RenderFrameHost* outermost_primary_rfh =
      host->GetWebContents()->GetOutermostWebContents()->GetPrimaryMainFrame();

  return autofill::ContentAutofillDriver::GetForRenderFrameHost(
      outermost_primary_rfh);
}

Response AutofillHandler::Enable() {
  if (enabled_) {
    return Response::Success();
  }

  enabled_ = true;
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillTestFormWithDevtools)) {
    auto host = content::DevToolsAgentHost::GetForId(target_id_);
    CHECK(host);
    autofill_managers_observation_.Observe(
        host->GetWebContents(),
        autofill::ScopedAutofillManagersObservation::InitializationPolicy::
            kObservePreexistingManagers);
  }
  return Response::Success();
}

Response AutofillHandler::Disable() {
  enabled_ = false;
  autofill_managers_observation_.Reset();
  return Response::Success();
}

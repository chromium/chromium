// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/autofill_handler.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/manual_testing_import.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using autofill::AutofillTriggerSource;
using autofill::CreditCard;
using autofill::FieldGlobalId;
using autofill::FormData;
using autofill::FormFieldData;
using protocol::Maybe;
using protocol::Response;

namespace {

absl::optional<std::pair<FormData, FormFieldData>> FindFieldWithFormData(
    autofill::ContentAutofillDriver* driver,
    autofill::FieldGlobalId id) {
  for (const auto& [key, form] :
       driver->autofill_manager()->form_structures()) {
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

  autofill::LocalFrameToken frame_token(
      outermost_primary_rfh->GetFrameToken().value());
  if (frame_id.isJust()) {
    bool found = false;
    outermost_primary_rfh->ForEachRenderFrameHost(
        [&frame_token, &frame_id, &found](content::RenderFrameHost* rfh) {
          if (rfh->GetDevToolsFrameToken().ToString() == frame_id.fromJust()) {
            frame_token =
                autofill::LocalFrameToken(rfh->GetFrameToken().value());
            found = true;
          }
        });

    if (!found) {
      std::move(callback)->sendFailure(
          Response::ServerError("Frame not found"));
      return;
    }
  }

  autofill::ContentAutofillDriver* autofill_driver = GetAutofillDriver();
  if (!autofill_driver) {
    std::move(callback)->sendFailure(
        Response::ServerError("RenderFrameHost is being destroyed"));
    return;
  }

  autofill::FieldGlobalId global_field_id = {
      frame_token, autofill::FieldRendererId(field_id)};

  const auto& field_data =
      FindFieldWithFormData(autofill_driver, global_field_id);

  if (!field_data.has_value()) {
    std::move(callback)->sendFailure(
        Response::InvalidRequest("field not found."));
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

  autofill_driver->autofill_manager()->FillCreditCardForm(
      field_data->first, field_data->second, tmp_autofill_card,
      base::UTF8ToUTF16(card->GetCvc()), AutofillTriggerSource::kPopup);

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

  static_cast<autofill::BrowserAutofillManager*>(
      autofill_driver->autofill_manager())
      ->set_test_addresses(test_address_for_countries);
  std::move(callback)->sendSuccess();
}

autofill::ContentAutofillDriver* AutofillHandler::GetAutofillDriver() {
  auto host = content::DevToolsAgentHost::GetForId(target_id_);
  DCHECK(host);

  content::RenderFrameHost* outermost_primary_rfh =
      host->GetWebContents()->GetOutermostWebContents()->GetPrimaryMainFrame();

  return autofill::ContentAutofillDriver::GetForRenderFrameHost(
      outermost_primary_rfh);
}

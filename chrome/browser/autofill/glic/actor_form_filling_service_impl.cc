// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/glic/actor_form_filling_service_impl.h"

#include <functional>
#include <utility>
#include <variant>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/types/zip.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/filling/form_filler.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/autofill/core/browser/suggestions/addresses/address_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/autofill_external_delegate.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

namespace {

struct ActorSuggestionWithFillData {
  ActorSuggestion suggestion;
  ActorFormFillingServiceImpl::FillData filling_payload;
};

// Attempts to generate an `ActorSuggestion` and the data needed for filling
// a suggestion. Returns `std::nullopt` if the suggestion does not contain an
// address payload.
std::optional<ActorSuggestionWithFillData> GetActorAddressSuggestion(
    const AddressDataManager& adm,
    base::span<const FieldGlobalId> fields,
    const Suggestion& suggestion) {
  const Suggestion::AutofillProfilePayload* const profile_payload =
      std::get_if<Suggestion::AutofillProfilePayload>(&suggestion.payload);
  if (!profile_payload) {
    return std::nullopt;
  }

  ActorSuggestion actor_suggestion;
  // TODO(crbug.com/455788947): Consider making `ActorSuggestion` use UTF16
  // strings.
  actor_suggestion.title = base::UTF16ToUTF8(suggestion.main_text.value);
  actor_suggestion.details =
      (!suggestion.labels.empty() && !suggestion.labels[0].empty())
          ? base::UTF16ToUTF8(suggestion.labels[0][0].value)
          : "";
  std::optional<AutofillProfile> profile =
      GetProfileFromPayload(adm, *profile_payload);
  if (!profile) {
    return std::nullopt;
  }
  ActorFormFillingServiceImpl::FillData fill_data = {base::ToVector(fields),
                                                     std::move(*profile)};
  return ActorSuggestionWithFillData{std::move(actor_suggestion),
                                     std::move(fill_data)};
}

// Generates address suggestions and the accompanying data that is needed for
// filling.
// Note that this is a preliminary implementation that is deficient in a number
// of ways:
// - The first entry in `fields` is used as the trigger field. This means that
//   we only return suggestions that have a non-empty value for this field.
// - Because only the first entry of `fields` is passed in, we may "deduplicate"
//   suggestions that would fill the same values in the form section represented
//   by the first field but that would fill different values in other sections.
//
// TODO(crbug.com/455788947): Improve suggestion generation.
// TODO(crbug.com/455788947): Check that address Autofill is not turned off.
std::vector<ActorSuggestionWithFillData> GetAddressSuggestions(
    base::span<const FieldGlobalId> fields,
    const AutofillManager& autofill_manager) {
  if (fields.empty()) {
    return {};
  }

  // For now, we simply take the first field.
  const FormStructure* const form_structure =
      autofill_manager.FindCachedFormById(fields[0]);
  if (!form_structure) {
    return {};
  }
  const FormData& form = form_structure->ToFormData();
  const AutofillField* const autofill_field =
      form_structure->GetFieldById(fields[0]);
  CHECK(autofill_field);

  std::vector<ActorSuggestionWithFillData> result;
  const AddressDataManager& adm =
      autofill_manager.client().GetPersonalDataManager().address_data_manager();

  auto convert_and_save_in_result =
      [&](std::pair<FillingProduct, std::vector<Suggestion>> response) {
        result.reserve(response.second.size());
        for (const Suggestion& s : response.second) {
          if (std::optional<ActorSuggestionWithFillData> actor_suggestion =
                  GetActorAddressSuggestion(adm, fields, s)) {
            result.emplace_back(*std::move(actor_suggestion));
          }
        }
      };

  AddressSuggestionGenerator generator(
      /*plus_address_email_override=*/std::nullopt,
      /*log_manager=*/nullptr);
  auto generate_suggestions =
      [&](std::pair<SuggestionGenerator::SuggestionDataSource,
                    std::vector<SuggestionGenerator::SuggestionData>> data) {
        generator.GenerateSuggestions(form, *autofill_field, form_structure,
                                      autofill_field, autofill_manager.client(),
                                      {std::move(data)},
                                      convert_and_save_in_result);
      };
  generator.FetchSuggestionData(form, *autofill_field, form_structure,
                                autofill_field, autofill_manager.client(),
                                generate_suggestions);
  return result;
}

// Retrieves the `AutofillManager` of the `tab`'s primary main frame.
[[nodiscard]] base::expected<std::reference_wrapper<AutofillManager>,
                             ActorFormFillingError>
GetAutofillManager(const tabs::TabInterface& tab) {
  using enum ActorFormFillingError;
  if (!tab.GetContents()) {
    return base::unexpected(kAutofillNotAvailable);
  }
  ContentAutofillClient* const client =
      ContentAutofillClient::FromWebContents(tab.GetContents());
  if (!client) {
    return base::unexpected(kAutofillNotAvailable);
  }
  if (AutofillManager* autofill_manager =
          client->GetAutofillManagerForPrimaryMainFrame()) {
    return *autofill_manager;
  }
  return base::unexpected(kAutofillNotAvailable);
}

// Converts the `FillData::Payload` into a payload that can be used by
// `BrowserAutofillManager`. Note that it does so by taking the addresses of
// `payload`'s contents, so `payload` must outlive the return value.
// Returns `std::nullopt` is `payload` contains `std::monostate`.
std::optional<FillingPayload> GetAutofillFillingPayload(
    const ActorFormFillingServiceImpl::FillData::Payload& payload
        LIFETIME_BOUND) {
  return std::visit(
      absl::Overload(
          [](std::monostate) { return std::optional<FillingPayload>(); },
          [](auto& payload) {
            return std::optional<FillingPayload>(&payload);
          }),
      payload);
}

}  // namespace

ActorFormFillingServiceImpl::FillData::FillData() = default;

ActorFormFillingServiceImpl::FillData::FillData(
    std::vector<FieldGlobalId> field_ids,
    std::variant<std::monostate, AutofillProfile> filling_payload)
    : field_ids(std::move(field_ids)),
      filling_payload(std::move(filling_payload)) {}

ActorFormFillingServiceImpl::FillData::FillData(const FillData&) = default;

ActorFormFillingServiceImpl::FillData&
ActorFormFillingServiceImpl::FillData::operator=(const FillData&) = default;

ActorFormFillingServiceImpl::FillData::FillData(FillData&&) = default;

ActorFormFillingServiceImpl::FillData&
ActorFormFillingServiceImpl::FillData::operator=(FillData&&) = default;

ActorFormFillingServiceImpl::FillData::~FillData() = default;

ActorFormFillingServiceImpl::ActorFormFillingServiceImpl() = default;

ActorFormFillingServiceImpl::~ActorFormFillingServiceImpl() = default;

void ActorFormFillingServiceImpl::GetSuggestions(
    const tabs::TabInterface& tab,
    base::span<const FillRequest> fill_requests,
    base::OnceCallback<void(base::expected<std::vector<ActorFormFillingRequest>,
                                           ActorFormFillingError>)> callback) {
  using enum ActorFormFillingError;
  base::expected<std::reference_wrapper<AutofillManager>, ActorFormFillingError>
      maybe_client = GetAutofillManager(tab);
  if (!maybe_client.has_value()) {
    std::move(callback).Run(base::unexpected(maybe_client.error()));
    return;
  }
  const AutofillManager& autofill_manager = maybe_client.value();

  // Fill requests should not be empty.
  if (fill_requests.empty()) {
    std::move(callback).Run(base::unexpected(kOther));
    return;
  }

  std::vector<ActorFormFillingRequest> requests;
  requests.reserve(fill_requests.size());
  for (const auto& [requested_data, representative_fields] : fill_requests) {
    std::vector<ActorSuggestionWithFillData> data;
    using enum ActorFormFillingRequest::RequestedData;
    switch (requested_data) {
      case FormFillingRequest_RequestedData_ADDRESS:
      case FormFillingRequest_RequestedData_SHIPPING_ADDRESS:
      case FormFillingRequest_RequestedData_BILLING_ADDRESS:
      case FormFillingRequest_RequestedData_HOME_ADDRESS:
      case FormFillingRequest_RequestedData_WORK_ADDRESS:
        data = GetAddressSuggestions(representative_fields, autofill_manager);
        break;
      case FormFillingRequest_RequestedData_CREDIT_CARD:
        // TODO(crbug.com/455788947): Add credit card suggestions.
        break;
      default:
        // Invalid request type.
        std::move(callback).Run(base::unexpected(kOther));
        return;
    }

    // For now, we require that every form is fillable.
    // TODO(crbug.com/455788947): Consider weakening this condition.
    if (data.empty()) {
      std::move(callback).Run(base::unexpected(kNoSuggestions));
      return;
    }

    requests.emplace_back();
    requests.back().requested_data = requested_data;
    requests.back().suggestions.reserve(data.size());
    for (ActorSuggestionWithFillData& entry : data) {
      entry.suggestion.id =
          ActorSuggestionId(suggestion_id_generator_.GenerateNextId());
      fill_data_[entry.suggestion.id] = std::move(entry.filling_payload);
      requests.back().suggestions.emplace_back(std::move(entry.suggestion));
    }
  }
  std::move(callback).Run(std::move(requests));
}

void ActorFormFillingServiceImpl::FillSuggestions(
    const tabs::TabInterface& tab,
    base::span<const ActorFormFillingSelection> chosen_suggestions,
    base::OnceCallback<void(base::expected<void, ActorFormFillingError>)>
        callback) {
  using enum ActorFormFillingError;
  base::expected<std::reference_wrapper<AutofillManager>, ActorFormFillingError>
      maybe_client = GetAutofillManager(tab);
  if (!maybe_client.has_value()) {
    std::move(callback).Run(base::unexpected(maybe_client.error()));
    return;
  }
  // TODO(crbug.com/455788947): Check that we are not using platform Autofill.
  BrowserAutofillManager& autofill_manager =
      static_cast<BrowserAutofillManager&>(maybe_client.value().get());

  // All suggestion ids must have been generated by this service.
  if (!std::ranges::all_of(
          chosen_suggestions,
          [&](ActorSuggestionId id) { return fill_data_.contains(id); },
          &ActorFormFillingSelection::selected_suggestion_id)) {
    std::move(callback).Run(base::unexpected(kOther));
    return;
  }

  // Re-determine the forms because in case they changed since the suggestions
  // were generated.
  std::vector<std::vector<FormData>> form_datas;
  form_datas.reserve(chosen_suggestions.size());
  for (const ActorFormFillingSelection& selection : chosen_suggestions) {
    const FillData& fill_data_for_form =
        fill_data_[selection.selected_suggestion_id];
    form_datas.emplace_back().reserve(fill_data_for_form.field_ids.size());
    for (FieldGlobalId field_id : fill_data_for_form.field_ids) {
      if (const FormStructure* form_structure =
              autofill_manager.FindCachedFormById(field_id)) {
        form_datas.back().emplace_back(form_structure->ToFormData());
      } else {
        // TODO(crbug.com/455788947): Consider being more lenient and complying
        // with partial form fills.
        std::move(callback).Run(base::unexpected(kNoForm));
        return;
      }
    }
  }

  // TODO(crbug.com/455788947): Set up fill observations.

  // Fill.
  for (const auto [selection, form_datas_for_suggestion] :
       base::zip(chosen_suggestions, form_datas)) {
    const FillData& fill_data_for_form =
        fill_data_[selection.selected_suggestion_id];
    for (const auto [field_id, form_data] :
         base::zip(fill_data_for_form.field_ids, form_datas_for_suggestion)) {
      std::optional<FillingPayload> filling_payload =
          GetAutofillFillingPayload(fill_data_for_form.filling_payload);
      if (!filling_payload) {
        continue;
      }
      autofill_manager.FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                         form_data, field_id, *filling_payload,
                                         AutofillTriggerSource::kGlic);
    }
  }

  // TODO(crbug.com/455788947): Only call after filling is complete.
  std::move(callback).Run(base::ok());
}

}  // namespace autofill

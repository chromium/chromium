// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/glic/actor_form_filling_service_impl.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <variant>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/types/zip.h"
#include "chrome/browser/autofill/glic/actor_filling_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/filling/form_filler.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/suggestions/addresses/address_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/autofill_external_delegate.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/origin.h"

namespace autofill {

namespace {

// Only emit directly to UMA - do not pass anywhere else, or you might hit
// NOTREACHED() in enum switches.
constexpr ActorFormFillingError kActorFormFillingSuccessForMetrics =
    static_cast<ActorFormFillingError>(0);

void RecordMetrics(std::string_view histogram_prefix,
                   base::TimeTicks start_time,
                   ActorFormFillingError outcome) {
  base::UmaHistogramTimes(base::StrCat({histogram_prefix, ".Latency"}),
                          base::TimeTicks::Now() - start_time);
  base::UmaHistogramEnumeration(base::StrCat({histogram_prefix, ".Outcome"}),
                                outcome);
}

// Records the latency and result of filling suggestions. `is_payments_fill`
// indicates whether any of the accepted suggestions was a payments suggestion.
// It returns the unmodified `result` to enable usage in chained callbacks.
base::expected<void, ActorFormFillingError> RecordFillSuggestionsMetrics(
    base::TimeTicks start_time,
    bool is_payments_fill,
    base::expected<void, ActorFormFillingError> result) {
  const ActorFormFillingError outcome =
      result.error_or(kActorFormFillingSuccessForMetrics);
  RecordMetrics("Autofill.Actor.FillSuggestions.Any", start_time, outcome);
  RecordMetrics(
      is_payments_fill
          ? "Autofill.Actor.FillSuggestions.WithPaymentInformation"
          : "Autofill.Actor.FillSuggestions.WithoutPaymentInformation",
      start_time, outcome);
  return result;
}

// Records the latency and result of getting suggestions.
// It returns the unmodified `result` to enable usage in chained callbacks.
base::expected<std::vector<ActorFormFillingRequest>, ActorFormFillingError>
RecordGetSuggestionsMetrics(base::TimeTicks start_time,
                            base::expected<std::vector<ActorFormFillingRequest>,
                                           ActorFormFillingError> result) {
  RecordMetrics("Autofill.Actor.GetSuggestions", start_time,
                result.error_or(kActorFormFillingSuccessForMetrics));
  return result;
}

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

// Generates an `ActorSuggestion` to fill a credit card or returns
// `std::nullopt` if it cannot generate a filling payload.
std::optional<ActorSuggestionWithFillData> GetActorCreditCardSuggestion(
    const PaymentsDataManager& paydm,
    base::span<const FieldGlobalId> fields,
    const Suggestion& suggestion) {
  const Suggestion::Guid* const card_guid =
      std::get_if<Suggestion::Guid>(&suggestion.payload);
  if (!card_guid) {
    return std::nullopt;
  }
  const CreditCard* const credit_card =
      paydm.GetCreditCardByGUID(card_guid->value());
  if (!credit_card) {
    return std::nullopt;
  }

  // TODO(crbug.com/455788947): Add the network/card art icon to the
  // ActorSuggestion.
  ActorSuggestion actor_suggestion;
  std::vector<std::u16string> title_components = {suggestion.main_text.value};
  base::Extend(title_components, suggestion.minor_texts,
               &Suggestion::Text::value);
  actor_suggestion.title =
      base::UTF16ToUTF8(base::JoinString(title_components, u" "));
  actor_suggestion.details =
      (!suggestion.labels.empty() && !suggestion.labels[0].empty())
          ? base::UTF16ToUTF8(suggestion.labels[0][0].value)
          : "";
  ActorFormFillingServiceImpl::FillData fill_data = {base::ToVector(fields),
                                                     *credit_card};
  return ActorSuggestionWithFillData{std::move(actor_suggestion),
                                     std::move(fill_data)};
}

// Generates address suggestions and the accompanying data that is needed for
// filling.
// Note that this is a preliminary implementation that is deficient in a
// number of ways:
// - The first entry in `fields` is used as the trigger field. This means that
//   we only return suggestions that have a non-empty value for this field.
// - Because only the first entry of `fields` is passed in, we may
// "deduplicate"
//   suggestions that would fill the same values in the form section
//   represented by the first field but that would fill different values in
//   other sections.
//
// TODO(crbug.com/455788947): Improve suggestion generation.
// TODO(crbug.com/455788947): Check that address Autofill is not turned off.
[[nodiscard]] std::vector<ActorSuggestionWithFillData> GetAddressSuggestions(
    base::span<const FieldGlobalId> fields,
    const AutofillManager& autofill_manager,
    LogManager* log_manager) {
  if (fields.empty()) {
    LOG_AF(log_manager) << LoggingScope::kAutofillActor
                        << "No fields were provided to GetAddressSuggestions.";
    return {};
  }

  // For now, we simply take the first field.
  const FormStructure* const form_structure =
      autofill_manager.FindCachedFormById(fields[0]);
  if (!form_structure) {
    LOG_AF(log_manager)
        << LoggingScope::kAutofillActor
        << "Could not find form structure for first trigger field.";
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

// Returns the first credit card number field in the same section as
// `initial_trigger_field` that has a safe origin. An origin is considered safe
// if it has the same origin as the initial trigger field or the origin is
// explicitly allowlisted.
std::optional<FieldGlobalId> GetSafeCreditCardNumberField(
    const AutofillOptimizationGuideDecider* decider,
    const FormStructure& form,
    const FieldGlobalId& trigger_field_id) {
  const AutofillField* const trigger_field =
      form.GetFieldById(trigger_field_id);
  if (!trigger_field) {
    return std::nullopt;
  }
  const Section& section = trigger_field->section();
  auto it = std::ranges::find_if(
      form.fields(), [&](const std::unique_ptr<AutofillField>& field) {
        if (field->section() != section) {
          return false;
        }
        if (field->Type().GetCreditCardType() != CREDIT_CARD_NUMBER) {
          return false;
        }
        const url::Origin& origin = field->origin();
        return origin.IsSameOriginWith(trigger_field->origin()) ||
               (decider &&
                decider->IsIframeUrlAllowlistedForActor(origin.GetURL()));
      });

  return it == form.fields().end() ? std::optional<FieldGlobalId>()
                                   : (*it)->global_id();
}

// Generates credit card suggestions and the data needed for filling them.
//
// Note that this is a preliminary version with the following traits:
// - VCN and BNPL suggestions are not supported.
// - No optimizations for CVCs (e.g. searching the last 4 digits in the DOM)
//   exist.
//
// TODO(crbug.com/455788947): Improve suggestion generation.
[[nodiscard]] std::vector<ActorSuggestionWithFillData> GetCreditCardSuggestions(
    base::span<const FieldGlobalId> fields,
    const AutofillManager& autofill_manager,
    LogManager* log_manager) {
  if (fields.empty()) {
    LOG_AF(log_manager)
        << LoggingScope::kAutofillActor
        << "No fields were provided to GetCreditCardSuggestions.";
    return {};
  }

  std::vector<FieldGlobalId> updated_fields;
  updated_fields.reserve(fields.size());
  // The field that we use to generate suggestion labels. We apply the following
  // logic:
  // - If `kAutofillActorRewriteCreditCardTriggerField` is enabled, we choose
  //   the first (safe) credit card number field that is in one of the sections
  //   represented by `fields`.
  // - Otherwise, we fall back to the first field that was passed in.
  const AutofillField* autofill_field_for_labels = nullptr;
  for (const FieldGlobalId& field : fields) {
    const FormStructure* const form_structure =
        autofill_manager.FindCachedFormById(field);
    if (!form_structure) {
      LOG_AF(log_manager) << LoggingScope::kAutofillActor
                          << "Could not find form structure for field: "
                          << field;
      return {};
    }

    if (base::FeatureList::IsEnabled(
            features::kAutofillActorRewriteCreditCardTriggerField)) {
      std::optional<FieldGlobalId> safe_credit_card_number_field =
          GetSafeCreditCardNumberField(
              autofill_manager.client().GetAutofillOptimizationGuideDecider(),
              *form_structure, field);
      updated_fields.push_back(safe_credit_card_number_field.value_or(field));
      if (safe_credit_card_number_field && !autofill_field_for_labels) {
        autofill_field_for_labels =
            form_structure->GetFieldById(*safe_credit_card_number_field);
      }
    } else {
      updated_fields.push_back(field);
    }
  }
  if (!autofill_field_for_labels) {
    const FormStructure* const form_structure =
        autofill_manager.FindCachedFormById(updated_fields[0]);
    autofill_field_for_labels = form_structure->GetFieldById(updated_fields[0]);
    if (!autofill_field_for_labels) {
      LOG_AF(log_manager) << LoggingScope::kAutofillActor
                          << "Could not find field for field: "
                          << updated_fields[0];
      return {};
    }
  }

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      autofill_manager.client(), *autofill_field_for_labels,
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/{},
      autofill_field_for_labels->Type().GetCreditCardType(),
      /*should_show_scan_credit_card=*/false, summary,
      /*is_card_number_field_empty=*/true);
  std::erase_if(suggestions, [](const Suggestion& s) {
    return s.type != SuggestionType::kCreditCardEntry;
  });

  std::vector<ActorSuggestionWithFillData> result;
  result.reserve(suggestions.size());
  const PaymentsDataManager& paydm = autofill_manager.client()
                                         .GetPersonalDataManager()
                                         .payments_data_manager();
  for (const Suggestion& s : suggestions) {
    if (std::optional<ActorSuggestionWithFillData> actor_suggestion =
            GetActorCreditCardSuggestion(paydm, updated_fields, s)) {
      result.emplace_back(*std::move(actor_suggestion));
    }
  }
  return result;
}

// Retrieves the `AutofillManager` of the `tab`'s primary main frame.
[[nodiscard]] base::expected<std::reference_wrapper<BrowserAutofillManager>,
                             ActorFormFillingError>
GetAutofillManager(const tabs::TabInterface& tab) {
  using enum ActorFormFillingError;
  if (!tab.GetContents()) {
    return base::unexpected(kAutofillNotAvailable);
  }

  Profile* const profile =
      Profile::FromBrowserContext(tab.GetContents()->GetBrowserContext());
  if (!profile) {
    return base::unexpected(kAutofillNotAvailable);
  }
  if (AutofillClientProviderFactory::GetForProfile(profile)
          .uses_platform_autofill()) {
    // This is currently only possible on Android platforms, but this check
    // guards against this becoming applicable for Desktop platforms as well.
    // It is a requirement for the cast to `BrowserAutofillManager` to be
    // safe.
    return base::unexpected(kAutofillNotAvailable);
  }

  ContentAutofillClient* const client =
      ContentAutofillClient::FromWebContents(tab.GetContents());
  if (!client) {
    return base::unexpected(kAutofillNotAvailable);
  }
  if (AutofillManager* autofill_manager =
          client->GetAutofillManagerForPrimaryMainFrame()) {
    return static_cast<BrowserAutofillManager&>(*autofill_manager);
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
    Payload filling_payload)
    : field_ids(std::move(field_ids)),
      filling_payload(std::move(filling_payload)) {}

ActorFormFillingServiceImpl::FillData::FillData(const FillData&) = default;

ActorFormFillingServiceImpl::FillData&
ActorFormFillingServiceImpl::FillData::operator=(const FillData&) = default;

ActorFormFillingServiceImpl::FillData::FillData(FillData&&) = default;

ActorFormFillingServiceImpl::FillData&
ActorFormFillingServiceImpl::FillData::operator=(FillData&&) = default;

ActorFormFillingServiceImpl::FillData::~FillData() = default;

bool ActorFormFillingServiceImpl::FillData::HasPaymentsPayload() const {
  return std::holds_alternative<CreditCard>(filling_payload);
}

ActorFormFillingServiceImpl::ActorFormFillingServiceImpl() = default;

ActorFormFillingServiceImpl::~ActorFormFillingServiceImpl() = default;

void ActorFormFillingServiceImpl::GetSuggestions(
    const tabs::TabInterface& tab,
    base::span<const FillRequest> fill_requests,
    base::OnceCallback<void(base::expected<std::vector<ActorFormFillingRequest>,
                                           ActorFormFillingError>)> callback) {
  auto callback_with_metrics =
      base::BindOnce(&RecordGetSuggestionsMetrics, base::TimeTicks::Now())
          .Then(std::move(callback));

  using enum ActorFormFillingError;
  base::expected<std::reference_wrapper<BrowserAutofillManager>,
                 ActorFormFillingError>
      maybe_manager = GetAutofillManager(tab);
  if (!maybe_manager.has_value()) {
    std::move(callback_with_metrics)
        .Run(base::unexpected(maybe_manager.error()));
    return;
  }
  AutofillManager& autofill_manager = maybe_manager.value();
  LogManager* const log_manager =
      autofill_manager.client().GetCurrentLogManager();

  // Fill requests should not be empty.
  if (fill_requests.empty()) {
    LOG_AF(log_manager) << LoggingScope::kAutofillActor
                        << "Fill requests are empty.";
    std::move(callback_with_metrics).Run(base::unexpected(kOther));
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
      case FormFillingRequest_RequestedData_CONTACT_INFORMATION: {
        if (!base::FeatureList::IsEnabled(
                ::features::kActorFormFillingServiceEnableAddress)) {
          LOG_AF(log_manager) << LoggingScope::kAutofillActor
                              << "Actor is disabled for address autofill.";
          std::move(callback_with_metrics)
              .Run(base::unexpected(kAutofillNotAvailable));
          return;
        }
        data = GetAddressSuggestions(representative_fields, autofill_manager,
                                     log_manager);
        break;
      }
      case FormFillingRequest_RequestedData_CREDIT_CARD: {
        if (!base::FeatureList::IsEnabled(
                ::features::kActorFormFillingServiceEnableCreditCard)) {
          LOG_AF(log_manager) << LoggingScope::kAutofillActor
                              << "Actor is disabled for credit card autofill.";
          std::move(callback_with_metrics)
              .Run(base::unexpected(kAutofillNotAvailable));
          return;
        }
        data = GetCreditCardSuggestions(representative_fields, autofill_manager,
                                        log_manager);
        break;
      }
      default: {
        // Invalid request type.
        LOG_AF(log_manager)
            << LoggingScope::kAutofillActor << "The request type is invalid.";
        std::move(callback_with_metrics).Run(base::unexpected(kOther));
        return;
      }
    }

    // For now, we require that every form is fillable.
    // TODO(crbug.com/455788947): Consider weakening this condition.
    if (data.empty()) {
      LOG_AF(log_manager) << LoggingScope::kAutofillActor
                          << "No suggestions were generated.";
      std::move(callback_with_metrics).Run(base::unexpected(kNoSuggestions));
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
  std::move(callback_with_metrics).Run(std::move(requests));
}

void ActorFormFillingServiceImpl::FillSuggestions(
    const tabs::TabInterface& tab,
    base::span<const ActorFormFillingSelection> chosen_suggestions,
    base::OnceCallback<void(base::expected<void, ActorFormFillingError>)>
        callback) {
  const bool is_payments_fill = std::ranges::any_of(
      chosen_suggestions, [&](const ActorFormFillingSelection& selection) {
        const FillData* fill_data =
            base::FindOrNull(fill_data_, selection.selected_suggestion_id);
        return fill_data && fill_data->HasPaymentsPayload();
      });
  auto callback_with_metrics =
      base::BindOnce(&RecordFillSuggestionsMetrics, base::TimeTicks::Now(),
                     is_payments_fill)
          .Then(std::move(callback));

  // Helper to make the early returns less verbose.
  auto post_error = [&callback_with_metrics](const base::Location& location,
                                             ActorFormFillingError error) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        location, base::BindOnce(std::move(callback_with_metrics),
                                 base::unexpected(error)));
  };

  using enum ActorFormFillingError;
  base::expected<std::reference_wrapper<BrowserAutofillManager>,
                 ActorFormFillingError>
      maybe_manager = GetAutofillManager(tab);
  if (!maybe_manager.has_value()) {
    post_error(FROM_HERE, maybe_manager.error());
    return;
  }
  BrowserAutofillManager& autofill_manager = maybe_manager.value();
  LogManager* const log_manager =
      autofill_manager.client().GetCurrentLogManager();

  // All suggestion ids must have been generated by this service.
  if (!std::ranges::all_of(
          chosen_suggestions,
          [&](ActorSuggestionId id) { return fill_data_.contains(id); },
          &ActorFormFillingSelection::selected_suggestion_id)) {
    LOG_AF(log_manager) << LoggingScope::kAutofillActor
                        << "A suggestion id is invalid.";
    post_error(FROM_HERE, kOther);
    return;
  }

  // Re-determine the forms because in case they changed since the suggestions
  // were generated.
  std::vector<std::vector<FormData>> form_datas;
  form_datas.reserve(chosen_suggestions.size());
  std::vector<FieldGlobalId> all_field_ids;
  for (const ActorFormFillingSelection& selection : chosen_suggestions) {
    const FillData& fill_data_for_form =
        fill_data_[selection.selected_suggestion_id];
    form_datas.emplace_back().reserve(fill_data_for_form.field_ids.size());
    for (FieldGlobalId field_id : fill_data_for_form.field_ids) {
      all_field_ids.push_back(field_id);
      if (const FormStructure* form_structure =
              autofill_manager.FindCachedFormById(field_id)) {
        form_datas.back().emplace_back(form_structure->ToFormData());
      } else {
        // TODO(crbug.com/455788947): Consider being more lenient and complying
        // with partial form fills.
        LOG_AF(log_manager)
            << LoggingScope::kAutofillActor
            << "Could not find form structure for field: " << field_id;
        post_error(FROM_HERE, kNoForm);
        return;
      }
    }
  }

  // Create a filling observer and keep it around until the maximum timeout is
  // reached.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](auto) {}, std::make_unique<ActorFillingObserver>(
                                      autofill_manager.client(), all_field_ids,
                                      std::move(callback_with_metrics))),
      ActorFillingObserver::GetMaximumTimeout());

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
}

}  // namespace autofill

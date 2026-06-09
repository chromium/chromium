// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/actor_form_filling_service_impl.h"

#include <algorithm>
#include <functional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/zip.h"
#include "chrome/browser/autofill/actor/actor_filling_observer.h"
#include "chrome/browser/autofill/actor/actor_key_metrics_recorder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/filling/form_filler.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/suggestions/addresses/address_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/credit_card_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/autofill_external_delegate.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/resource/resource_bundle.h"
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
void RecordFillSuggestionsMetrics(
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

struct ActorSuggestions {
  FieldGlobalId trigger_field_id;
  std::vector<ActorSuggestionWithFillData> suggestions_with_fill_data;
};

// Attempts to generate an `ActorSuggestion` and the data needed for filling
// a suggestion. Returns `std::nullopt` if the suggestion does not contain an
// address payload.
std::optional<ActorSuggestionWithFillData> GetActorAddressSuggestion(
    const AddressDataManager& adm,
    base::span<const FieldGlobalId> fields,
    const Suggestion& suggestion,
    actor::SectionSplitPart split_part) {
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
  fill_data.split_part = split_part;
  return ActorSuggestionWithFillData{std::move(actor_suggestion),
                                     std::move(fill_data)};
}

// Retrieves an icon for a payment suggestion
std::optional<gfx::Image> GetCreditCardSuggestionIcon(
    const Suggestion& suggestion) {
  // TODO(crbug.com/463396455): Credit cards will contain either a gfx::Image in
  // the `custom_icon` or a generic resource icon id in the `icon` field.
  // None-the-less, all types of icons should be converted.
  if (std::holds_alternative<gfx::Image>(suggestion.custom_icon)) {
    gfx::Image image = std::get<gfx::Image>(suggestion.custom_icon);
    if (!image.IsEmpty()) {
      return image;
    }
  }
  if (int icon_resource_id = GetIconResourceID(suggestion.icon)) {
    return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        icon_resource_id);
  } else {
    return std::nullopt;
  }
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
  // TODO(crbug.com/475192853): Move icon fetching to a higher layer.
  actor_suggestion.icon = GetCreditCardSuggestionIcon(suggestion);
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
[[nodiscard]] ActorSuggestions GetAddressSuggestions(
    base::span<const FieldGlobalId> fields,
    AutofillManager& autofill_manager,
    LogManager* log_manager,
    actor::SectionSplitPart split_part) {
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
      actor::RetargetTriggerFieldForSplittingIfNeeded(
          form_structure, form_structure->GetFieldById(fields[0]), split_part,
          log_manager);
  CHECK(autofill_field);

  std::vector<Suggestion> suggestions;
  const AddressDataManager& adm =
      autofill_manager.client().GetPersonalDataManager().address_data_manager();

  auto save_suggestions = base::BindOnce(
      [](std::vector<Suggestion>& suggestions,
         SuggestionGenerator::ReturnedSuggestions response) {
        suggestions = std::move(response.second);
      },
      std::ref(suggestions));
  AddressSuggestionGenerator generator(
      mojom::AutofillSuggestionTriggerSource::kGlic);
  generator.GenerateSuggestions(form, *autofill_field, form_structure,
                                autofill_field, autofill_manager.client(),
                                std::move(save_suggestions));

  std::vector<ActorSuggestionWithFillData> result;
  result.reserve(suggestions.size());
  for (const Suggestion& s : suggestions) {
    if (std::optional<ActorSuggestionWithFillData> actor_suggestion =
            GetActorAddressSuggestion(adm, fields, s, split_part)) {
      result.emplace_back(*std::move(actor_suggestion));
    }
  }
  return {autofill_field->global_id(), result};
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
[[nodiscard]] ActorSuggestions GetCreditCardSuggestions(
    base::span<const FieldGlobalId> fields,
    AutofillManager& autofill_manager,
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

  const FormStructure* const form_structure =
      autofill_manager.FindCachedFormById(
          autofill_field_for_labels->global_id());
  const FormData& form = form_structure->ToFormData();

  CreditCardSuggestionGenerator generator(
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      /*credit_card_form_event_logger=*/nullptr,
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/true);

  std::vector<Suggestion> suggestions;
  const PaymentsDataManager& paydm = autofill_manager.client()
                                         .GetPersonalDataManager()
                                         .payments_data_manager();

  auto save_suggestions = base::BindOnce(
      [](std::vector<Suggestion>& suggestions,
         SuggestionGenerator::ReturnedSuggestions response) {
        suggestions = std::move(response.second);
      },
      std::ref(suggestions));
  generator.GenerateSuggestions(form, *autofill_field_for_labels,
                                form_structure, autofill_field_for_labels,
                                autofill_manager.client(),
                                std::move(save_suggestions));

  std::vector<ActorSuggestionWithFillData> result;
  result.reserve(suggestions.size());
  for (const Suggestion& s : suggestions) {
    if (s.type != SuggestionType::kCreditCardEntry) {
      continue;
    }
    if (std::optional<ActorSuggestionWithFillData> actor_suggestion =
            GetActorCreditCardSuggestion(paydm, fields, s)) {
      result.emplace_back(*std::move(actor_suggestion));
    }
  }
  return {autofill_field_for_labels->global_id(), result};
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

ActorFormFillingService::FillRequest::FillRequest() = default;
ActorFormFillingService::FillRequest::FillRequest(
    ActorFormFillingRequest::RequestedData requested_data,
    std::vector<FieldGlobalId> trigger_fields,
    std::string section_label)
    : requested_data(requested_data),
      trigger_fields(std::move(trigger_fields)),
      section_label(std::move(section_label)) {}
ActorFormFillingService::FillRequest::FillRequest(const FillRequest&) = default;
ActorFormFillingService::FillRequest&
ActorFormFillingService::FillRequest::operator=(const FillRequest&) = default;
ActorFormFillingService::FillRequest::FillRequest(FillRequest&&) = default;
ActorFormFillingService::FillRequest&
ActorFormFillingService::FillRequest::operator=(FillRequest&&) = default;
ActorFormFillingService::FillRequest::~FillRequest() = default;

bool ActorFormFillingServiceImpl::FillData::HasPaymentsPayload() const {
  return std::holds_alternative<CreditCard>(filling_payload);
}

ActorFormFillingServiceImpl::ActorFormFillingServiceImpl(
    base::SafeRef<::actor::AggregatedJournal> journal,
    ::actor::TaskId task_id)
    : journal_(journal), task_id_(task_id) {}

ActorFormFillingServiceImpl::~ActorFormFillingServiceImpl() = default;

void ActorFormFillingServiceImpl::GetSuggestions(
    const tabs::TabInterface& tab,
    base::span<const FillRequest> fill_requests,
    GetSuggestionsCallback callback) {
  auto callback_with_metrics =
      base::BindOnce(&RecordGetSuggestionsMetrics, base::TimeTicks::Now())
          .Then(std::move(callback));

  auto log_actor_error = [&](std::string_view error_message) {
    journal_->Log(
        tab.GetContents()->GetLastCommittedURL(), task_id_,
        "ActorFormFillingServiceImpl::GetSuggestions",
        ::actor::JournalDetailsBuilder().AddError(error_message).Build());
  };

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
    log_actor_error("Fill requests are empty.");
    std::move(callback_with_metrics).Run(base::unexpected(kOther));
    return;
  }

  suggestion_trigger_field_id_.clear();
  std::vector<ActorFormFillingRequest> requests;
  requests.reserve(fill_requests.size());

  // Collect products per form to record metrics.
  base::flat_map<FormGlobalId, base::flat_set<FillingProduct>> products_by_form;

  for (const auto& fill_request : fill_requests) {
    using enum ActorFormFillingRequest::RequestedData;

    // A single FillRequest can result in multiple ActorFormFillingRequests
    // if we decide to split contact information from address information.
    struct SubRequest {
      ActorFormFillingRequest::RequestedData requested_data;
      actor::SectionSplitPart split_part;
    };
    std::vector<SubRequest> sub_requests;

    switch (fill_request.requested_data) {
      case kAddress:
      case kShippingAddress:
      case kBillingAddress:
      case kHomeAddress:
      case kWorkAddress:
      case kContactInformation: {
        if (!base::FeatureList::IsEnabled(
                ::features::kActorFormFillingServiceEnableAddress)) {
          LOG_AF(log_manager) << LoggingScope::kAutofillActor
                              << "Actor is disabled for address autofill.";
          log_actor_error("Actor is disabled for address autofill.");
          std::move(callback_with_metrics)
              .Run(base::unexpected(kAutofillNotAvailable));
          return;
        }

        if (actor::ShouldSplitOutContactInfo(fill_request.trigger_fields,
                                             autofill_manager, log_manager)) {
          sub_requests.push_back(
              {kContactInformation, actor::SectionSplitPart::kContactInfo});
          // For the address split part, use the original requested_data type
          // unless it was CONTACT_INFORMATION as that would create a misleading
          // UX (two contact info cards back to back).
          ActorFormFillingRequest::RequestedData address_requested_data =
              (fill_request.requested_data == kContactInformation)
                  ? kAddress
                  : fill_request.requested_data;
          sub_requests.push_back(
              {address_requested_data, actor::SectionSplitPart::kAddress});
        } else {
          sub_requests.push_back(
              {fill_request.requested_data, actor::SectionSplitPart::kNoSplit});
        }
        break;
      }
      case kCreditCard: {
        if (!base::FeatureList::IsEnabled(
                ::features::kActorFormFillingServiceEnableCreditCard)) {
          LOG_AF(log_manager) << LoggingScope::kAutofillActor
                              << "Actor is disabled for credit card autofill.";
          log_actor_error("Actor is disabled for credit card autofill.");
          std::move(callback_with_metrics)
              .Run(base::unexpected(kAutofillNotAvailable));
          return;
        }
        sub_requests.push_back(
            {kCreditCard, actor::SectionSplitPart::kNoSplit});
        break;
      }
      default: {
        LOG_AF(log_manager)
            << LoggingScope::kAutofillActor << "The request type is invalid.";
        log_actor_error("The request type is invalid.");
        std::move(callback_with_metrics).Run(base::unexpected(kOther));
        return;
      }
    }
    url::Origin origin =
        tab.GetContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();

    for (const SubRequest& sub_request : sub_requests) {
      ActorSuggestions suggestion_data;
      switch (sub_request.requested_data) {
        case kAddress:
        case kShippingAddress:
        case kBillingAddress:
        case kHomeAddress:
        case kWorkAddress:
        case kContactInformation:
          suggestion_data = GetAddressSuggestions(fill_request.trigger_fields,
                                                  autofill_manager, log_manager,
                                                  sub_request.split_part);
          break;
        case kCreditCard:
          suggestion_data = GetCreditCardSuggestions(
              fill_request.trigger_fields, autofill_manager, log_manager);
          break;
        default:
          LOG_AF(log_manager)
              << LoggingScope::kAutofillActor << "The request type is invalid.";
          log_actor_error("The request type is invalid.");
          std::move(callback_with_metrics).Run(base::unexpected(kOther));
          return;
      }

      // For now, we require that every form is fillable.
      // TODO(crbug.com/455788947): Consider weakening this condition.
      if (suggestion_data.suggestions_with_fill_data.empty()) {
        LOG_AF(log_manager)
            << LoggingScope::kAutofillActor << "No suggestions were generated.";
        log_actor_error("No suggestions were generated.");
        std::move(callback_with_metrics).Run(base::unexpected(kNoSuggestions));
        return;
      }

      if (const FormStructure* form_structure =
              autofill_manager.FindCachedFormById(
                  fill_request.trigger_fields[0])) {
        products_by_form[form_structure->global_id()].insert(
            sub_request.requested_data == kCreditCard
                ? FillingProduct::kCreditCard
                : FillingProduct::kAddress);
      }

      requests.emplace_back();
      requests.back().requested_data = sub_request.requested_data;
      requests.back().request_origin = origin;
      // TODO(crbug.com/502158215): Integrate form-splitting with section label.
      requests.back().section_label = fill_request.section_label;
      requests.back().suggestions.reserve(
          suggestion_data.suggestions_with_fill_data.size());
      suggestion_trigger_field_id_.emplace_back(
          suggestion_data.trigger_field_id);
      for (ActorSuggestionWithFillData& entry :
           suggestion_data.suggestions_with_fill_data) {
        entry.suggestion.id =
            ActorSuggestionId(suggestion_id_generator_.GenerateNextId());
        fill_data_[entry.suggestion.id] = std::move(entry.filling_payload);
        requests.back().suggestions.emplace_back(std::move(entry.suggestion));
      }
    }
  }

  if (ActorKeyMetricsRecorder* recorder =
          autofill_manager.client().GetActorKeyMetricsRecorder()) {
    for (const auto& [form_id, products] : products_by_form) {
      recorder->OnSuggestionsGenerated(form_id, products);
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
  auto callback_with_metrics = base::BindOnce(
      [](base::WeakPtr<ActorFormFillingServiceImpl> service,
         bool is_payments_fill,
         base::OnceCallback<void(base::expected<void, ActorFormFillingError>)>
             callback,
         base::expected<void, ActorFormFillingError> result) {
        if (!service) {
          return;
        }
        RecordFillSuggestionsMetrics(base::TimeTicks::Now(), is_payments_fill,
                                     result);
        std::move(callback).Run(
            service->errors_per_session_.empty()
                ? result
                : base::unexpected(service->errors_per_session_.front()));

        service->filling_observer_.reset();
        service->errors_per_session_.clear();
      },
      weak_ptr_factory_.GetWeakPtr(), is_payments_fill, std::move(callback));

  CHECK_DEREF(filling_observer_).Activate(std::move(callback_with_metrics));
}

void ActorFormFillingServiceImpl::ScrollToForm(const tabs::TabInterface& tab,
                                               int form_index) {
  base::expected<std::reference_wrapper<BrowserAutofillManager>,
                 ActorFormFillingError>
      maybe_manager = GetAutofillManager(tab);
  if (!maybe_manager.has_value()) {
    return;
  }
  AutofillManager& autofill_manager = maybe_manager.value();

  // TODO(crbug.com/448398227): Consider making `form_index` a `size_t`
  // everywhere instead of `int`.
  if (static_cast<size_t>(form_index) >= suggestion_trigger_field_id_.size() ||
      !suggestion_trigger_field_id_[form_index]) {
    LogManager* const log_manager =
        autofill_manager.client().GetCurrentLogManager();
    LOG_AF(log_manager) << LoggingScope::kAutofillActor
                        << "Fill/Preview aborted: `form_index` does not "
                           "correspond to any trigger field";
    return;
  }
  autofill_manager.driver().ScrollFieldIntoView(
      suggestion_trigger_field_id_[form_index]);
}

void ActorFormFillingServiceImpl::PreviewForm(const tabs::TabInterface& tab,
                                              int form_index,
                                              ActorSuggestionId suggestion_id) {
  FillOrPreviewFormImpl(tab, suggestion_id, mojom::ActionPersistence::kPreview);
}

void ActorFormFillingServiceImpl::ClearFormPreview(
    const tabs::TabInterface& tab,
    int form_index) {
  base::expected<std::reference_wrapper<BrowserAutofillManager>,
                 ActorFormFillingError>
      maybe_manager = GetAutofillManager(tab);
  if (!maybe_manager.has_value()) {
    return;
  }
  AutofillManager& autofill_manager = maybe_manager.value();
  autofill_manager.driver().RendererShouldClearPreviewedForm();
}

void ActorFormFillingServiceImpl::FillForm(
    const tabs::TabInterface& tab,
    int form_index,
    ActorFormFillingSelection selection) {
  std::optional<ActorFormFillingError> potential_error = FillOrPreviewFormImpl(
      tab, selection.selected_suggestion_id, mojom::ActionPersistence::kFill);
  if (potential_error) {
    errors_per_session_.push_back(*potential_error);
  }
}

std::optional<ActorFormFillingError>
ActorFormFillingServiceImpl::FillOrPreviewFormImpl(
    const tabs::TabInterface& tab,
    ActorSuggestionId suggestion_id,
    mojom::ActionPersistence action_persistence) {
  // TODO(crbug.com/448398227): Consider changing some of these early returns
  // into CHECKs.

  // Local helper for journal logging to reduce repetition
  auto log_actor_error = [&](std::string_view error_message) {
    std::string_view action_str =
        (action_persistence == mojom::ActionPersistence::kFill) ? "Fill"
                                                                : "Preview";
    journal_->Log(
        tab.GetContents()->GetLastCommittedURL(), task_id_,
        "ActorFormFillingServiceImpl::FillOrPreviewFormImpl",
        ::actor::JournalDetailsBuilder()
            .AddError(base::StrCat({action_str, " failed: ", error_message}))
            .Build());
  };

  base::expected<std::reference_wrapper<BrowserAutofillManager>,
                 ActorFormFillingError>
      maybe_manager = GetAutofillManager(tab);
  if (!maybe_manager.has_value()) {
    log_actor_error("Autofill manager not available.");
    return ActorFormFillingError::kAutofillNotAvailable;
  }
  BrowserAutofillManager& autofill_manager = maybe_manager.value();
  LogManager* const log_manager =
      autofill_manager.client().GetCurrentLogManager();

  const FillData* fill_data = base::FindOrNull(fill_data_, suggestion_id);
  if (!fill_data) {
    LOG_AF(log_manager) << LoggingScope::kAutofillActor
                        << "Fill/Preview aborted: Could not find the "
                           "`FillData` with the given `ActorSuggestionId`.";
    log_actor_error(
        "Could not find the FillData with the given ActorSuggestionId.");
    return ActorFormFillingError::kOther;
  }

  if (fill_data->field_ids.empty()) {
    LOG_AF(log_manager) << LoggingScope::kAutofillActor
                        << "Fill/Preview aborted: The corresponding `FillData` "
                           "had no associated fields.";
    log_actor_error("The corresponding FillData had no associated fields.");
    return ActorFormFillingError::kOther;
  }

  if (action_persistence == mojom::ActionPersistence::kFill) {
    if (!filling_observer_) {
      filling_observer_ =
          std::make_unique<ActorFillingObserver>(autofill_manager.client());
    }
    filling_observer_->ObserveNewFilling(fill_data->field_ids);
  }

  for (FieldGlobalId trigger_field_id : fill_data->field_ids) {
    if (const FormStructure* const form_structure =
            autofill_manager.FindCachedFormById(trigger_field_id)) {
      if (ActorKeyMetricsRecorder* recorder =
              autofill_manager.client().GetActorKeyMetricsRecorder()) {
        if (action_persistence == mojom::ActionPersistence::kFill) {
          recorder->RecordFormToFill(form_structure->global_id());
        }
      }
      std::visit(absl::Overload{
                     [&](const AutofillProfile& autofill_profile) {
                       base::flat_set<FieldGlobalId> blocked_fields =
                           actor::GetBlockedFieldsForSplit(
                               *form_structure, trigger_field_id,
                               fill_data->split_part, action_persistence);
                       autofill_manager.FillOrPreviewForm(
                           action_persistence, form_structure->global_id(),
                           trigger_field_id, &autofill_profile,
                           AutofillTriggerSource::kGlic, blocked_fields);
                     },
                     [&](const CreditCard& credit_card) {
                       autofill_manager.FillOrPreviewForm(
                           action_persistence, form_structure->global_id(),
                           trigger_field_id, &credit_card,
                           AutofillTriggerSource::kGlic,
                           /*blocked_fields=*/{});
                     },
                     [&](const std::monostate&) {
                       LOG_AF(log_manager)
                           << LoggingScope::kAutofillActor
                           << "Fill/Preview aborted: Could not fill/preview "
                              "because the suggestion had empty payload.";
                       log_actor_error("The suggestion had empty payload.");
                       return;
                     }},
                 fill_data->filling_payload);
    } else {
      log_actor_error("Form not found in cache.");
    }
  }
  return std::nullopt;
}

}  // namespace autofill

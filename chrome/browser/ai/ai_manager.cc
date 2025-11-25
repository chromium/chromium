// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_manager.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_crx_component.h"
#include "chrome/browser/ai/ai_language_model.h"
#include "chrome/browser/ai/ai_never_load_component.h"
#include "chrome/browser/ai/ai_proofreader.h"
#include "chrome/browser/ai/ai_rewriter.h"
#include "chrome/browser/ai/ai_summarizer.h"
#include "chrome/browser/ai/ai_utils.h"
#include "chrome/browser/ai/ai_writer.h"
#include "chrome/browser/ai/features.h"
#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language/core/common/locale_util.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/common/page_visibility_state.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_proofreader.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_writer.mojom.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"

namespace {

constexpr float kDefaultMaxTemperature = 2.0f;
constexpr uint32_t kMinTopK = 1;
constexpr float kMinTemperature = 0.0f;

// Base set of supported language codes when features don't have their own
// set of supported languages.
constexpr auto kDefaultSupportedBaseLanguages =
    base::MakeFixedFlatSet<std::string_view>({"en"});

const char kUnsupportedLanguageError[] =
    "Unsupported %s API languages were specified, and the request was aborted. "
    "API calls must only specify supported languages to ensure successful "
    "processing and guarantee output characteristics. Please only specify "
    "supported language codes: [%s]";
const char kEmptyOutputLanguageWarning[] =
    "No output language was specified in a %s API request. An output language "
    "should be specified to ensure optimal output quality and properly attest "
    "to output safety. Please specify a supported output language code: [%s]";

// Enables eagerly initializing other AI APIs when any session type is created.
BASE_FEATURE(kBuiltInAIEagerInit, base::FEATURE_DISABLED_BY_DEFAULT);

blink::mojom::ModelAvailabilityCheckResult
ConvertOnDeviceModelEligibilityReasonToModelAvailabilityCheckResult(
    optimization_guide::OnDeviceModelEligibilityReason
        on_device_model_eligibility_reason) {
  auto availability = optimization_guide::AvailabilityFromEligibilityReason(
      on_device_model_eligibility_reason);
  if (availability ==
      optimization_guide::mojom::ModelUnavailableReason::kPendingAssets) {
    return blink::mojom::ModelAvailabilityCheckResult::kDownloading;
  }
  if (availability ==
      optimization_guide::mojom::ModelUnavailableReason::kPendingUsage) {
    return blink::mojom::ModelAvailabilityCheckResult::kDownloadable;
  }

  switch (on_device_model_eligibility_reason) {
    case optimization_guide::OnDeviceModelEligibilityReason::kUnknown:
      return blink::mojom::ModelAvailabilityCheckResult::kUnavailableUnknown;
    case optimization_guide::OnDeviceModelEligibilityReason::kFeatureNotEnabled:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableFeatureNotEnabled;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kConfigNotAvailableForFeature:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableConfigNotAvailableForFeature;
    case optimization_guide::OnDeviceModelEligibilityReason::kGpuBlocked:
      return blink::mojom::ModelAvailabilityCheckResult::kUnavailableGpuBlocked;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kTooManyRecentCrashes:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableTooManyRecentCrashes;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kSafetyModelNotAvailable:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableSafetyModelNotAvailable;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kSafetyConfigNotAvailableForFeature:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableSafetyConfigNotAvailableForFeature;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kLanguageDetectionModelNotAvailable:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableLanguageDetectionModelNotAvailable;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kFeatureExecutionNotEnabled:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableFeatureExecutionNotEnabled;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kModelAdaptationNotAvailable:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableModelAdaptationNotAvailable;
    case optimization_guide::OnDeviceModelEligibilityReason::kModelNotEligible:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableModelNotEligible;
    case optimization_guide::OnDeviceModelEligibilityReason::kValidationPending:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableValidationPending;
    case optimization_guide::OnDeviceModelEligibilityReason::kValidationFailed:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableValidationFailed;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kInsufficientDiskSpace:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableInsufficientDiskSpace;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kModelToBeInstalled:
    case optimization_guide::OnDeviceModelEligibilityReason::
        kNoOnDeviceFeatureUsed:
      // Shouldn't reach here since it's handled by checking `availability`.
      NOTREACHED();
    case optimization_guide::OnDeviceModelEligibilityReason::
        kDeprecatedModelNotAvailable:
    case optimization_guide::OnDeviceModelEligibilityReason::kSuccess:
      NOTREACHED();
  }
  NOTREACHED();
}

// Get the capabilities specified from the expected input or output types.
on_device_model::Capabilities GetExpectedCapabilities(
    const std::optional<std::vector<blink::mojom::AILanguageModelExpectedPtr>>&
        expected_vector) {
  on_device_model::Capabilities capabilities;
  if (expected_vector) {
    for (const auto& expected_entry : expected_vector.value()) {
      switch (expected_entry->type) {
        case blink::mojom::AILanguageModelPromptType::kText:
          // Text capabilities are included by default.
          break;
        case blink::mojom::AILanguageModelPromptType::kImage:
          capabilities.Put(on_device_model::CapabilityFlags::kImageInput);
          break;
        case blink::mojom::AILanguageModelPromptType::kAudio:
          capabilities.Put(on_device_model::CapabilityFlags::kAudioInput);
          break;
        case blink::mojom::AILanguageModelPromptType::kToolCall:
        case blink::mojom::AILanguageModelPromptType::kToolResponse:
          // TODO(crbug.com/422803232): Implement tool capabilities.
          break;
      }
    }
  }
  return capabilities;
}

on_device_model::mojom::Priority GetPriorityFromVisibility(
    content::RenderFrameHost* rfh) {
  if (!rfh) {
    // If there is no rfh, this is a service worker. Treat as foreground always.
    return on_device_model::mojom::Priority::kForeground;
  }
  return rfh->GetVisibilityState() == content::PageVisibilityState::kVisible
             ? on_device_model::mojom::Priority::kForeground
             : on_device_model::mojom::Priority::kBackground;
}

// A struct for hashing and comparison of language base codes.
struct BaseCode {
  size_t operator()(const AILanguageCodePtr& l) const {
    return absl::DefaultHashContainerHash<std::string_view>()(
        language::ExtractBaseLanguage(l->code));
  }
  bool operator()(const AILanguageCodePtr& a,
                  const AILanguageCodePtr& b) const {
    return language::ExtractBaseLanguage(a->code) ==
           language::ExtractBaseLanguage(b->code);
  }
};
using LanguageSet = absl::flat_hash_set<AILanguageCodePtr, BaseCode, BaseCode>;

void Insert(LanguageSet& set, const AILanguageCodePtr& language) {
  if (language && !language->code.empty()) {
    set.insert(language.Clone());
  }
}

void Insert(LanguageSet& set, const std::vector<AILanguageCodePtr>& languages) {
  for (const AILanguageCodePtr& language : languages) {
    Insert(set, language);
  }
}

template <typename OptionsPtr>
LanguageSet GetLanguages(const OptionsPtr& options) {
  LanguageSet languages;
  if (options) {
    Insert(languages, options->expected_input_languages);
    Insert(languages, options->expected_context_languages);
    Insert(languages, options->output_language);
  }
  return languages;
}

template <>
LanguageSet GetLanguages(
    const blink::mojom::AIProofreaderCreateOptionsPtr& options) {
  LanguageSet languages;
  if (options) {
    Insert(languages, options->expected_input_languages);
    Insert(languages, options->correction_explanation_language);
  }
  return languages;
}

template <>
LanguageSet GetLanguages(
    const blink::mojom::AILanguageModelCreateOptionsPtr& options) {
  LanguageSet languages;
  if (options && options->expected_inputs.has_value()) {
    for (const auto& expected_entry : options->expected_inputs.value()) {
      if (expected_entry->languages.has_value()) {
        for (const auto& language : expected_entry->languages.value()) {
          Insert(languages, language);
        }
      }
    }
  }
  if (options && options->expected_outputs.has_value()) {
    for (const auto& expected_entry : options->expected_outputs.value()) {
      if (expected_entry->languages.has_value()) {
        for (const auto& language : expected_entry->languages.value()) {
          Insert(languages, language);
        }
      }
    }
  }
  return languages;
}

bool AreLanguagesSupported(const LanguageSet& requested,
                           const base::flat_set<std::string_view>& supported) {
  return std::ranges::all_of(requested, [&](const AILanguageCodePtr& lang) {
    return supported.contains(language::ExtractBaseLanguage(lang->code));
  });
}

// Returns whether an output language was specified or initialized here using an
// inferred language, i.e. when all input languages use the same base language.
template <typename OptionsWithOutputLanguagePtrType>
bool CheckAndFixOutputLanguage(OptionsWithOutputLanguagePtrType& options,
                               const LanguageSet& languages) {
  if ((!options->output_language || options->output_language->code.empty()) &&
      languages.size() == 1) {
    options->output_language = languages.begin()->Clone();
  }
  return options->output_language && !options->output_language->code.empty();
}

template <>
bool CheckAndFixOutputLanguage(
    blink::mojom::AIProofreaderCreateOptionsPtr& options,
    const LanguageSet& languages) {
  if ((!options->correction_explanation_language ||
       options->correction_explanation_language->code.empty()) &&
      languages.size() == 1) {
    options->correction_explanation_language = languages.begin()->Clone();
  }
  return options->correction_explanation_language &&
         !options->correction_explanation_language->code.empty();
}

// Returns whether an output language was specified; does not initialize.
template <>
bool CheckAndFixOutputLanguage(
    blink::mojom::AILanguageModelCreateOptionsPtr& options,
    const LanguageSet& languages) {
  if (options->expected_outputs.has_value()) {
    for (const auto& expected_entry : options->expected_outputs.value()) {
      if (expected_entry->languages.has_value()) {
        if (std::ranges::any_of(expected_entry->languages.value(),
                                [](const AILanguageCodePtr& lang) {
                                  return !lang->code.empty();
                                })) {
          return true;
        }
      }
    }
  }
  return false;
}

}  // namespace

AIManager::AIManager(
    content::BrowserContext* browser_context,
    component_updater::ComponentUpdateService* component_update_service,
    content::RenderFrameHost* rfh)
    : component_update_service_(*component_update_service),
      context_bound_object_set_(GetPriorityFromVisibility(rfh)),
      browser_context_(browser_context),
      rfh_(rfh ? rfh->GetWeakDocumentPtr() : content::WeakDocumentPtr()) {
  if (rfh && rfh->GetRenderWidgetHost()) {
    widget_observer_.Observe(rfh->GetRenderWidgetHost());
  }
  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  if (service) {
    model_broker_client_ = service->CreateModelBrokerClient();
  }
}

AIManager::~AIManager() = default;

bool AIManager::IsBuiltInAIAPIsEnabledByPolicy() {
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context_)->GetPrefs();
  return !prefs->HasPrefPath(policy::policy_prefs::kBuiltInAIAPIsEnabled) ||
         prefs->GetBoolean(policy::policy_prefs::kBuiltInAIAPIsEnabled);
}

void AIManager::AddReceiver(
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AIManager::CanCreateLanguageModel(
    blink::mojom::AILanguageModelCreateOptionsPtr options,
    CanCreateLanguageModelCallback callback) {
  if (!IsBuiltInAIAPIsEnabledByPolicy()) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableEnterprisePolicyDisabled);
    return;
  }

  on_device_model::Capabilities input_capabilities;
  if (options) {
    input_capabilities = GetExpectedCapabilities(options->expected_inputs);
    if (!GetExpectedCapabilities(options->expected_outputs).empty() ||
        (!input_capabilities.empty() &&
         !base::FeatureList::IsEnabled(
             blink::features::kAIPromptAPIMultimodalInput))) {
      std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                  kUnavailableModelAdaptationNotAvailable);
      return;
    }
  }

  if (!CheckAndFixLanguages(options, "LanguageModel",
                            AILanguageModel::GetSupportedLanguageBaseCodes())) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }

  CanCreateSession(optimization_guide::mojom::OnDeviceFeature::kPromptApi,
                   input_capabilities, std::move(callback));
}

void AIManager::CreateLanguageModel(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client,
    blink::mojom::AILanguageModelCreateOptionsPtr options) {
  CHECK(options);
  if (!CheckAndFixLanguages(options, "LanguageModel",
                            AILanguageModel::GetSupportedLanguageBaseCodes())) {
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote(std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }

  if (!model_broker_client_) {
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote(std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }
  model_broker_client_
      ->GetSubscriber(optimization_guide::mojom::OnDeviceFeature::kPromptApi)
      .WaitForClient(base::BindOnce(&AIManager::CreateLanguageModelInternal,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(client), std::move(options)));
}

void AIManager::CreateLanguageModelInternal(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client,
    blink::mojom::AILanguageModelCreateOptionsPtr options,
    base::WeakPtr<optimization_guide::ModelClient> model_client) {
  if (!model_client) {
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote(std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  blink::mojom::AILanguageModelParamsPtr language_model_params =
      GetLanguageModelParams();
  blink::mojom::AILanguageModelSamplingParamsPtr sampling_params =
      std::move(options->sampling_params);
  auto params = on_device_model::mojom::SessionParams::New();
  if (sampling_params) {
    params->top_k = std::min(std::max(kMinTopK, sampling_params->top_k),
                             language_model_params->max_sampling_params->top_k);
    params->temperature =
        std::min(std::max(kMinTemperature, sampling_params->temperature),
                 language_model_params->max_sampling_params->temperature);
  } else {
    params->top_k = language_model_params->default_sampling_params->top_k;
    params->temperature =
        language_model_params->default_sampling_params->temperature;
  }

  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  params->capabilities = GetExpectedCapabilities(options->expected_inputs);
  on_device_model::Capabilities output_capabilities =
      GetExpectedCapabilities(options->expected_outputs);
  if (!params->capabilities.empty() || !output_capabilities.empty()) {
    if (!output_capabilities.empty() ||
        !base::FeatureList::IsEnabled(
            blink::features::kAIPromptAPIMultimodalInput) ||
        !service->GetOnDeviceCapabilities().HasAll(params->capabilities)) {
      mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
          client_remote(std::move(client));
      AIUtils::SendClientRemoteError(
          client_remote,
          blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
      return;
    }
  }

  mojo::PendingRemote<on_device_model::mojom::Session> session;
  model_client->solution().CreateSession(
      session.InitWithNewPipeAndPassReceiver(), params.Clone());

  auto model = std::make_unique<AILanguageModel>(
      context_bound_object_set_, std::move(params), std::move(model_client),
      std::move(session),
      service->GetOptimizationGuideLogger()
          ? service->GetOptimizationGuideLogger()->GetWeakPtr()
          : nullptr);
  model->Initialize(std::move(options->initial_prompts), std::move(client));

  context_bound_object_set_.AddContextBoundObject(std::move(model));

  tried_init_.insert(optimization_guide::mojom::OnDeviceFeature::kPromptApi);
  // Eagerly initialize other features, now that one successfully initialized.
  MaybeTryEagerInit();
}

void AIManager::CanCreateSummarizer(
    blink::mojom::AISummarizerCreateOptionsPtr options,
    CanCreateSummarizerCallback callback) {
  if (!IsBuiltInAIAPIsEnabledByPolicy()) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableEnterprisePolicyDisabled);
    return;
  }
  if (!CheckAndFixLanguages(options, "Summarizer",
                            AISummarizer::GetSupportedLanguageBaseCodes())) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  CanCreateSession(optimization_guide::mojom::OnDeviceFeature::kSummarize,
                   on_device_model::Capabilities(), std::move(callback));
}

void AIManager::CreateSummarizer(
    mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
    blink::mojom::AISummarizerCreateOptionsPtr options) {
  if (!CheckAndFixLanguages(options, "Summarizer",
                            AISummarizer::GetSupportedLanguageBaseCodes())) {
    mojo::Remote<blink::mojom::AIManagerCreateSummarizerClient> client_remote(
        std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }

  if (!model_broker_client_) {
    mojo::Remote<blink::mojom::AIManagerCreateSummarizerClient> client_remote(
        std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  std::optional<optimization_guide::MultimodalMessage> initial_request;
  if (options->shared_context.has_value() &&
      !options->shared_context.value().empty()) {
    optimization_guide::proto::SummarizeRequest request;
    request.set_context(options->shared_context.value());
    initial_request = optimization_guide::MultimodalMessage(request);
  }
  auto callback = base::BindOnce(
      &AIManager::OnSessionCreated<
          AISummarizer, blink::mojom::AISummarizer,
          blink::mojom::AIManagerCreateSummarizerClient,
          blink::mojom::AISummarizerCreateOptionsPtr>,
      weak_factory_.GetWeakPtr(), std::ref(context_bound_object_set_),
      std::move(options), std::move(initial_request), std::move(client));
  tried_init_.insert(optimization_guide::mojom::OnDeviceFeature::kSummarize);
  model_broker_client_->CreateSession(
      optimization_guide::mojom::OnDeviceFeature::kSummarize,
      ::optimization_guide::SessionConfigParams{}, std::move(callback));
}

void AIManager::CanCreateProofreader(
    blink::mojom::AIProofreaderCreateOptionsPtr options,
    CanCreateProofreaderCallback callback) {
  // TODO(crbug.com/424673180): Add a warning message when options
  // `includeCorrectionTypes` and `includeCorrectionExplanations` are set to
  // true as those features are not yet supported by the API.
  auto supported =
      base::MakeFlatSet<std::string_view>(kDefaultSupportedBaseLanguages);
  if (!CheckAndFixLanguages(options, "Proofreader", supported)) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  CanCreateSession(optimization_guide::mojom::OnDeviceFeature::kProofreaderApi,
                   on_device_model::Capabilities(), std::move(callback));
}

void AIManager::CreateProofreader(
    mojo::PendingRemote<blink::mojom::AIManagerCreateProofreaderClient> client,
    blink::mojom::AIProofreaderCreateOptionsPtr options) {
  auto supported =
      base::MakeFlatSet<std::string_view>(kDefaultSupportedBaseLanguages);
  if (!CheckAndFixLanguages(options, "Proofreader", supported)) {
    mojo::Remote<blink::mojom::AIManagerCreateProofreaderClient> client_remote(
        std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }

  if (!model_broker_client_) {
    mojo::Remote<blink::mojom::AIManagerCreateProofreaderClient> client_remote(
        std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  auto callback =
      base::BindOnce(&AIManager::OnSessionCreated<
                         AIProofreader, blink::mojom::AIProofreader,
                         blink::mojom::AIManagerCreateProofreaderClient,
                         blink::mojom::AIProofreaderCreateOptionsPtr>,
                     weak_factory_.GetWeakPtr(),
                     std::ref(context_bound_object_set_), std::move(options),
                     /*initial_request=*/std::nullopt, std::move(client));
  tried_init_.insert(
      optimization_guide::mojom::OnDeviceFeature::kProofreaderApi);
  model_broker_client_->CreateSession(
      optimization_guide::mojom::OnDeviceFeature::kProofreaderApi,
      ::optimization_guide::SessionConfigParams{}, std::move(callback));
}

blink::mojom::AILanguageModelParamsPtr AIManager::GetLanguageModelParams() {
  auto model_info = blink::mojom::AILanguageModelParams::New(
      blink::mojom::AILanguageModelSamplingParams::New(),
      blink::mojom::AILanguageModelSamplingParams::New());
  model_info->max_sampling_params->top_k =
      optimization_guide::features::GetOnDeviceModelMaxTopK();
  model_info->max_sampling_params->temperature = kDefaultMaxTemperature;

  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  if (!service) {
    return model_info;
  }
  auto sampling_params_config = service->GetSamplingParamsConfig(
      optimization_guide::mojom::OnDeviceFeature::kPromptApi);

  if (!sampling_params_config.has_value()) {
    return model_info;
  }

  model_info->default_sampling_params->top_k =
      sampling_params_config->default_top_k;
  model_info->default_sampling_params->temperature =
      sampling_params_config->default_temperature;

  auto metadata = service->GetFeatureMetadata(
      optimization_guide::mojom::OnDeviceFeature::kPromptApi);
  if (metadata.has_value()) {
    auto parsed_metadata = AILanguageModel::ParseMetadata(metadata.value());
    if (parsed_metadata.has_max_sampling_params()) {
      auto max_sampling_params = parsed_metadata.max_sampling_params();
      if (max_sampling_params.has_top_k()) {
        model_info->max_sampling_params->top_k = max_sampling_params.top_k();
      }
      if (max_sampling_params.has_temperature()) {
        model_info->max_sampling_params->temperature =
            max_sampling_params.temperature();
      }
    }
  }

  return model_info;
}

// This is the method to get the info for AILanguageModel.
void AIManager::GetLanguageModelParams(
    GetLanguageModelParamsCallback callback) {
  std::move(callback).Run(GetLanguageModelParams());
}

void AIManager::CanCreateWriter(blink::mojom::AIWriterCreateOptionsPtr options,
                                CanCreateWriterCallback callback) {
  if (!IsBuiltInAIAPIsEnabledByPolicy()) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableEnterprisePolicyDisabled);
    return;
  }
  if (!CheckAndFixLanguages(options, "Writer",
                            AIWriter::GetSupportedLanguageBaseCodes())) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  CanCreateSession(
      optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi,
      on_device_model::Capabilities(), std::move(callback));
}

void AIManager::CreateWriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
    blink::mojom::AIWriterCreateOptionsPtr options) {
  if (!CheckAndFixLanguages(options, "Writer",
                            AIWriter::GetSupportedLanguageBaseCodes())) {
    mojo::Remote<blink::mojom::AIManagerCreateWriterClient> client_remote(
        std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }

  if (!model_broker_client_) {
    mojo::Remote<blink::mojom::AIManagerCreateWriterClient> client_remote(
        std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  std::optional<optimization_guide::MultimodalMessage> initial_request;
  if (options->shared_context.has_value() &&
      !options->shared_context.value().empty()) {
    optimization_guide::proto::WritingAssistanceApiRequest request;
    request.set_shared_context(options->shared_context.value());
    initial_request = optimization_guide::MultimodalMessage(request);
  }
  auto callback = base::BindOnce(
      &AIManager::OnSessionCreated<AIWriter, blink::mojom::AIWriter,
                                   blink::mojom::AIManagerCreateWriterClient,
                                   blink::mojom::AIWriterCreateOptionsPtr>,
      weak_factory_.GetWeakPtr(), std::ref(context_bound_object_set_),
      std::move(options), std::move(initial_request), std::move(client));
  tried_init_.insert(
      optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi);
  model_broker_client_->CreateSession(
      optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi,
      ::optimization_guide::SessionConfigParams{}, std::move(callback));
}

void AIManager::CanCreateRewriter(
    blink::mojom::AIRewriterCreateOptionsPtr options,
    CanCreateRewriterCallback callback) {
  if (!IsBuiltInAIAPIsEnabledByPolicy()) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableEnterprisePolicyDisabled);
    return;
  }
  if (!CheckAndFixLanguages(options, "Rewriter",
                            AIRewriter::GetSupportedLanguageBaseCodes())) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  CanCreateSession(
      optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi,
      on_device_model::Capabilities(), std::move(callback));
}

void AIManager::CreateRewriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
    blink::mojom::AIRewriterCreateOptionsPtr options) {
  if (!CheckAndFixLanguages(options, "Rewriter",
                            AIRewriter::GetSupportedLanguageBaseCodes())) {
    mojo::Remote<blink::mojom::AIManagerCreateRewriterClient> client_remote(
        std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }

  if (!model_broker_client_) {
    mojo::Remote<blink::mojom::AIManagerCreateRewriterClient> client_remote(
        std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  std::optional<optimization_guide::MultimodalMessage> initial_request;
  if (options->shared_context.has_value() &&
      !options->shared_context.value().empty()) {
    optimization_guide::proto::WritingAssistanceApiRequest request;
    request.set_shared_context(options->shared_context.value());
    initial_request = optimization_guide::MultimodalMessage(request);
  }
  auto callback = base::BindOnce(
      &AIManager::OnSessionCreated<AIRewriter, blink::mojom::AIRewriter,
                                   blink::mojom::AIManagerCreateRewriterClient,
                                   blink::mojom::AIRewriterCreateOptionsPtr>,
      weak_factory_.GetWeakPtr(), std::ref(context_bound_object_set_),
      std::move(options), std::move(initial_request), std::move(client));
  tried_init_.insert(
      optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi);
  model_broker_client_->CreateSession(
      optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi,
      ::optimization_guide::SessionConfigParams{}, std::move(callback));
}

void AIManager::CanCreateSession(
    optimization_guide::mojom::OnDeviceFeature capability,
    on_device_model::Capabilities capabilities,
    CanCreateLanguageModelCallback callback) {
  auto model_path =
      optimization_guide::switches::GetOnDeviceModelExecutionOverride();
  if (model_path.has_value()) {
    // If the model path is provided, we do this additional check and post a
    // warning message to dev tools if it does not exist.
    // This needs to be done in a task runner with `MayBlock` trait.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(base::PathExists, model_path.value()),
        base::BindOnce(&AIManager::OnModelPathValidationComplete,
                       weak_factory_.GetWeakPtr(), model_path.value()));
  }

  // Check if the optimization guide service can create session.
  OptimizationGuideKeyedService* service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_));

  // If the `OptimizationGuideKeyedService` cannot be retrieved, return false.
  if (!service) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableServiceNotRunning);
    return;
  }

  service->GetOnDeviceModelEligibilityAsync(
      capability, capabilities,
      base::BindOnce(&AIManager::FinishCanCreateSession,
                     weak_factory_.GetWeakPtr(), capability, capabilities,
                     std::move(callback)));
}

void AIManager::FinishCanCreateSession(
    optimization_guide::mojom::OnDeviceFeature capability,
    on_device_model::Capabilities capabilities,
    CanCreateLanguageModelCallback callback,
    optimization_guide::OnDeviceModelEligibilityReason eligibility) {
  OptimizationGuideKeyedService* service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_));

  // If the `OptimizationGuideKeyedService` cannot create new session, return
  // the reason.
  if (eligibility !=
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess) {
    std::move(callback).Run(
        ConvertOnDeviceModelEligibilityReasonToModelAvailabilityCheckResult(
            eligibility));
    return;
  }

  if (!capabilities.empty() &&
      !service->GetOnDeviceCapabilities().HasAll(capabilities)) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableModelAdaptationNotAvailable);
    return;
  }

  std::move(callback).Run(
      blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

template <typename ContextBoundObjectType,
          typename ContextBoundObjectReceiverInterface,
          typename ClientRemoteInterface,
          typename CreateOptionsPtrType>
void AIManager::OnSessionCreated(
    AIContextBoundObjectSet& context_bound_object_set,
    CreateOptionsPtrType options,
    std::optional<optimization_guide::MultimodalMessage> initial_request,
    mojo::PendingRemote<ClientRemoteInterface> client,
    std::unique_ptr<optimization_guide::OnDeviceSession> session) {
  mojo::Remote<ClientRemoteInterface> client_remote(std::move(client));

  if (!session) {
    AIUtils::AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  // Eagerly initialize other features, now that one successfully initialized.
  MaybeTryEagerInit();

  if (initial_request.has_value()) {
    session->GetExecutionInputSizeInTokens(
        initial_request.value().read(),
        base::BindOnce(
            [](AIContextBoundObjectSet& context_bound_object_set,
               CreateOptionsPtrType options,
               mojo::Remote<ClientRemoteInterface> client_remote,
               std::unique_ptr<optimization_guide::OnDeviceSession> session,
               std::optional<uint32_t> result) {
              if (!result.has_value()) {
                AIUtils::SendClientRemoteError(
                    client_remote, blink::mojom::AIManagerCreateClientError::
                                       kUnableToCalculateTokenSize);
                return;
              }
              uint32_t quota =
                  blink::mojom::kWritingAssistanceMaxInputTokenSize;
              if (result.value() > quota) {
                AIUtils::SendClientRemoteError(
                    client_remote,
                    blink::mojom::AIManagerCreateClientError::
                        kInitialInputTooLarge,
                    blink::mojom::QuotaErrorInfo::New(result.value(), quota));
                return;
              }
              mojo::PendingRemote<ContextBoundObjectReceiverInterface>
                  pending_remote;
              context_bound_object_set.AddContextBoundObject(
                  std::make_unique<ContextBoundObjectType>(
                      context_bound_object_set, std::move(session),
                      std::move(options),
                      pending_remote.InitWithNewPipeAndPassReceiver()));
              client_remote->OnResult(std::move(pending_remote));
            },
            std::ref(context_bound_object_set), std::move(options),
            std::move(client_remote), std::move(session)));
    return;
  }

  mojo::PendingRemote<ContextBoundObjectReceiverInterface> pending_remote;
  context_bound_object_set.AddContextBoundObject(
      std::make_unique<ContextBoundObjectType>(
          context_bound_object_set, std::move(session), std::move(options),
          pending_remote.InitWithNewPipeAndPassReceiver()));
  client_remote->OnResult(std::move(pending_remote));
}

void AIManager::MaybeTryEagerInit() {
  if (!base::FeatureList::IsEnabled(kBuiltInAIEagerInit)) {
    return;
  }
  // Experimentally initialize other features when one is used. This presumes a
  // large foundational model download completed with the first feature usage,
  // and other features just need lightweight configuration downloads to become
  // readily available for usage on this device.
  AIContextBoundObjectSet empty(on_device_model::mojom::Priority::kBackground);
  for (optimization_guide::mojom::OnDeviceFeature feature :
       {optimization_guide::mojom::OnDeviceFeature::kPromptApi,
        optimization_guide::mojom::OnDeviceFeature::kSummarize,
        optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi,
        optimization_guide::mojom::OnDeviceFeature::kProofreaderApi}) {
    // TODO(crbug.com/442015822): Gate on availability state.
    // TODO(crbug.com/447192715): Gate on runtime determined component size.
    if (tried_init_.insert(feature).second) {
      // TODO(crbug.com/447174556): Init features without creating sessions.
      model_broker_client_->CreateSession(
          feature, ::optimization_guide::SessionConfigParams{},
          base::DoNothing());
    }
  }
}

template <typename OptionsPtrType>
bool AIManager::CheckAndFixLanguages(
    OptionsPtrType& options,
    std::string_view api_name,
    const base::flat_set<std::string_view>& supported) {
  LanguageSet languages = GetLanguages(options);
  if (!AreLanguagesSupported(languages, supported)) {
    MaybeLogUnsupportedLanguageError(api_name, supported);
    return false;
  }
  if (!options || !CheckAndFixOutputLanguage(options, languages)) {
    MaybeLogMissingOutputLanguageWarning(api_name, supported);
  }
  return true;
}

void AIManager::OnModelPathValidationComplete(const base::FilePath& model_path,
                                              bool is_valid_path) {
  // TODO(crbug.com/346491542): Remove this when the error page is implemented.
  if (!is_valid_path) {
    VLOG(1) << base::StringPrintf(
        "Unable to create a session because the model path ('%s') is invalid.",
        model_path.AsUTF8Unsafe());
  }
}

void AIManager::AddModelDownloadProgressObserver(
    mojo::PendingRemote<blink ::mojom::ModelDownloadProgressObserver>
        observer_remote) {
  auto components = on_device_ai::AICrxComponent::FromComponentIds(
      &component_update_service_.get(),
      {component_updater::OptimizationGuideOnDeviceModelInstallerPolicy::
           GetOnDeviceModelExtensionId()});

  // Have some portion of the loading bar occupied until the renderer sends the
  // 100% download progress on creation. This is to indicate that there is still
  // work going on between when the model is downloaded and the actual API
  // instance is created.
  if (base::FeatureList::IsEnabled(features::kAIModelUnloadableProgress)) {
    components.insert(std::make_unique<on_device_ai::AINeverLoadComponent>(
        features::kAIModelUnloadableProgressBytes.Get()));
  }

  model_download_progress_manager_.AddObserver(std::move(observer_remote),
                                               std::move(components));
}

void AIManager::RenderWidgetHostVisibilityChanged(
    content::RenderWidgetHost* widget_host,
    bool became_visible) {
  context_bound_object_set_.SetPriority(
      became_visible ? on_device_model::mojom::Priority::kForeground
                     : on_device_model::mojom::Priority::kBackground);
}

void AIManager::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  DCHECK(widget_observer_.IsObservingSource(widget_host));
  widget_observer_.Reset();
}

void AIManager::MaybeLogMissingOutputLanguageWarning(
    const std::string_view api_name,
    const base::flat_set<std::string_view>& supported_languages) {
  auto* rfh = rfh_.AsRenderFrameHostIfValid();
  if (!rfh || did_log_missing_output_language_warning_) {
    return;
  }
  did_log_missing_output_language_warning_ = true;
  auto list = base::JoinString(supported_languages, ", ");
  rfh->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning,
      base::StringPrintf(kEmptyOutputLanguageWarning, api_name, list));
}

void AIManager::MaybeLogUnsupportedLanguageError(
    const std::string_view api_name,
    const base::flat_set<std::string_view>& supported_languages) {
  auto* rfh = rfh_.AsRenderFrameHostIfValid();
  if (!rfh || did_log_unsupported_language_error_) {
    return;
  }
  did_log_unsupported_language_error_ = true;
  auto list = base::JoinString(supported_languages, ", ");
  rfh->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError,
      base::StringPrintf(kUnsupportedLanguageError, api_name, list));
}

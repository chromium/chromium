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
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ai/ai_classifier.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_language_model.h"
#include "chrome/browser/ai/ai_proofreader.h"
#include "chrome/browser/ai/ai_rewriter.h"
#include "chrome/browser/ai/ai_summarizer.h"
#include "chrome/browser/ai/ai_writer.h"
#include "chrome/browser/ai/features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language/core/common/locale_util.h"
#include "components/on_device_ai/ai_utils.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/feature_configs.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/common/page_visibility_state.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/mojom/download_observer.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_classifier.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_proofreader.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_writer.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"

namespace {

constexpr float kDefaultMaxTemperature = 2.0f;
constexpr uint32_t kMinTopK = 1;
constexpr float kMinTemperature = 0.0f;

constexpr float kMostPredictableTemperature = 0.0f;
constexpr uint32_t kMostPredictableTopK = 1;

constexpr float kPredictableTemperature = 0.2f;
constexpr uint32_t kPredictableTopK = 2;

constexpr float kBalancedTemperature = 1.0f;
constexpr uint32_t kBalancedTopK = 3;

constexpr float kCreativeTemperature = 1.1f;
constexpr uint32_t kCreativeTopK = 10;

constexpr float kMostCreativeTemperature = 1.2f;
constexpr uint32_t kMostCreativeTopK = 25;

const char kUnsupportedLanguageError[] =
    "Unsupported %s API languages were specified, and the request was aborted. "
    "API calls must only specify supported languages to ensure successful "
    "processing and guarantee output characteristics. Please only specify "
    "supported language codes: [%s]";
const char kEmptyOutputLanguageWarning[] =
    "No output language was specified in a %s API request. An output language "
    "should be specified to ensure optimal output quality and properly attest "
    "to output safety. Please specify a supported output language code: [%s]";
const char kExperimentalLanguageWarning[] =
    "The specified languages are experimental in %s API and output quality "
    "cannot be guaranteed. The supported language codes are: [%s]";
const char kSpeedPreferenceMarkdownWarning[] =
    "The 'speed' performance preference utilizes a model with limited support "
    "for 'markdown' format.";

// Eagerly initializes other downloadable APIs when any session type is created.
BASE_FEATURE(kBuiltInAIEagerInit, base::FEATURE_ENABLED_BY_DEFAULT);

blink::mojom::ModelAvailabilityCheckResult
ConvertModelNotSupportedReasonToModelAvailabilityCheckResult(
    std::optional<optimization_guide::mojom::ModelNotSupportedDetailedReason>
        reason) {
  if (!reason.has_value()) {
    return blink::mojom::ModelAvailabilityCheckResult::kUnavailableUnknown;
  }

  switch (reason.value()) {
    case optimization_guide::mojom::ModelNotSupportedDetailedReason::
        kFeatureNotEnabled:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableFeatureNotEnabled;
    case optimization_guide::mojom::ModelNotSupportedDetailedReason::
        kGpuBlocked:
      return blink::mojom::ModelAvailabilityCheckResult::kUnavailableGpuBlocked;
    case optimization_guide::mojom::ModelNotSupportedDetailedReason::
        kTooManyRecentCrashes:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableTooManyRecentCrashes;
    case optimization_guide::mojom::ModelNotSupportedDetailedReason::
        kSafetyConfigNotAvailableForFeature:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableSafetyConfigNotAvailableForFeature;
    case optimization_guide::mojom::ModelNotSupportedDetailedReason::
        kFeatureExecutionNotEnabled:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableFeatureExecutionNotEnabled;
    case optimization_guide::mojom::ModelNotSupportedDetailedReason::
        kValidationFailed:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableValidationFailed;
    case optimization_guide::mojom::ModelNotSupportedDetailedReason::
        kModelNotEligible:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableModelNotEligible;
    case optimization_guide::mojom::ModelNotSupportedDetailedReason::
        kInsufficientDiskSpace:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableInsufficientDiskSpace;
    case optimization_guide::mojom::ModelNotSupportedDetailedReason::
        kModelAdaptationNotAvailable:
      return blink::mojom::ModelAvailabilityCheckResult::
          kUnavailableModelAdaptationNotAvailable;
  }

  NOTREACHED();
}

blink::mojom::ModelAvailabilityCheckResult
ConvertModelEligibilityReasonToModelAvailabilityCheckResult(
    std::optional<optimization_guide::mojom::ModelUnavailableReason> reason,
    std::optional<optimization_guide::mojom::ModelNotSupportedDetailedReason>
        detailed_reason) {
  if (!reason.has_value()) {
    return blink::mojom::ModelAvailabilityCheckResult::kAvailable;
  }

  switch (reason.value()) {
    case optimization_guide::mojom::ModelUnavailableReason::kUnknown:
      return blink::mojom::ModelAvailabilityCheckResult::kUnavailableUnknown;
    case optimization_guide::mojom::ModelUnavailableReason::kPendingAssets:
      return blink::mojom::ModelAvailabilityCheckResult::kDownloading;
    case optimization_guide::mojom::ModelUnavailableReason::kPendingUsage:
      return blink::mojom::ModelAvailabilityCheckResult::kDownloadable;
    case optimization_guide::mojom::ModelUnavailableReason::kNotSupported:
      return ConvertModelNotSupportedReasonToModelAvailabilityCheckResult(
          detailed_reason);
  }

  NOTREACHED();
}

// Checks if capabilities contain image or audio input (multimodal
// capabilities).
bool HasMultimodalInputCapabilities(
    const on_device_model::Capabilities& capabilities) {
  return capabilities.Has(on_device_model::CapabilityFlags::kImageInput) ||
         capabilities.Has(on_device_model::CapabilityFlags::kAudioInput);
}

// Checks if the expected outputs contain any invalid types.
// Models can generate text and tool calls, but not images, audio, or tool
// responses.
bool HasInvalidOutputTypes(
    base::optional_ref<
        const std::vector<blink::mojom::AILanguageModelExpectedPtr>>
        expected_outputs) {
  if (!expected_outputs.has_value()) {
    return false;
  }
  for (const auto& expected_entry : expected_outputs.value()) {
    switch (expected_entry->type) {
      case blink::mojom::AILanguageModelPromptType::kText:
      case blink::mojom::AILanguageModelPromptType::kToolCall:
        // Valid output types.
        break;
      case blink::mojom::AILanguageModelPromptType::kImage:
      case blink::mojom::AILanguageModelPromptType::kAudio:
      case blink::mojom::AILanguageModelPromptType::kToolResponse:
        // Invalid output types - models don't generate these.
        return true;
    }
  }
  return false;
}

on_device_model::Capabilities GetExpectedInputCapabilities(
    base::optional_ref<
        const std::vector<blink::mojom::AILanguageModelExpectedPtr>>
        expected_inputs) {
  on_device_model::Capabilities capabilities;
  if (expected_inputs.has_value()) {
    for (const auto& expected_entry : expected_inputs.value()) {
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
          // Tool use capability is signaled by the presence of tool
          // declarations, not by expected input/output types.
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

// Returns the use case name based on the `model_version` param from the
// `experimental_use_cases` field of the config.
template <typename FeatureConfigProto>
std::optional<std::string> GetExperimentalUseCaseByModelVersion(
    const FeatureConfigProto& feature_config) {
  if (base::FeatureList::IsEnabled(kAIApiFoundationalModel)) {
    std::string model_version = base::GetFieldTrialParamValueByFeature(
        kAIApiFoundationalModel, kModelVersionParam);
    auto it = feature_config.experimental_use_cases().find(model_version);
    if (it != feature_config.experimental_use_cases().end()) {
      return it->second;
    }
  }
  return std::nullopt;
}

template <typename FeatureConfigProto>
std::optional<FeatureConfigProto> ParseFeatureConfig(
    const std::optional<mojo_base::ProtoWrapper>& wrapper) {
  if (!wrapper.has_value()) {
    return std::nullopt;
  }
  auto any_config = wrapper->As<optimization_guide::proto::Any>();
  if (!any_config.has_value()) {
    return std::nullopt;
  }

  FeatureConfigProto feature_config;
  if (!feature_config.ParseFromString(any_config->value())) {
    return std::nullopt;
  }

  return feature_config;
}

template <typename FeatureConfigProto>
std::optional<std::string> GetUseCaseFromFeatureConfig(
    const std::optional<mojo_base::ProtoWrapper>& wrapper) {
  auto feature_config = ParseFeatureConfig<FeatureConfigProto>(wrapper);
  if (!feature_config) {
    return std::nullopt;
  }

  if (std::optional<std::string> experimental_use_case =
          GetExperimentalUseCaseByModelVersion(*feature_config)) {
    return experimental_use_case;
  }

  return feature_config->default_use_case();
}

// Creates a session for a feature by determining the correct model use case
// (e.g. default, performance preference, or experimental use case) via the
// provided `UseCaseResolver` and the feature configuration.
void CreateSessionWithConfigAndResolver(
    optimization_guide::ModelBrokerClient* broker_client,
    base::OnceCallback<
        void(std::unique_ptr<optimization_guide::OnDeviceSession>)> callback,
    AIManager::UseCaseResolver resolver,
    std::optional<mojo_base::ProtoWrapper> wrapper) {
  std::optional<std::string> use_case = std::move(resolver).Run(wrapper);

  if (!use_case.has_value() || use_case->empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  broker_client->CreateSession(*use_case,
                               ::optimization_guide::SessionConfigParams{},
                               std::move(callback));
}

// Convenience template that creates a session using the default use case
// resolution logic derived from `FeatureConfigProto`.
template <typename FeatureConfigProto>
void CreateSessionWithConfig(
    optimization_guide::ModelBrokerClient* broker_client,
    base::OnceCallback<
        void(std::unique_ptr<optimization_guide::OnDeviceSession>)> callback,
    std::optional<mojo_base::ProtoWrapper> wrapper) {
  CreateSessionWithConfigAndResolver(
      broker_client, std::move(callback),
      base::BindOnce(&GetUseCaseFromFeatureConfig<FeatureConfigProto>),
      std::move(wrapper));
}

// Request assets and wait for the model broker client to become
// available by reading the use case from the use case config provided by the
// model broker.
template <typename FeatureConfigProto>
void RequestAssetsAndWaitForClientWithConfig(
    optimization_guide::ModelBrokerClient* broker_client,
    base::OnceCallback<void(base::WeakPtr<optimization_guide::ModelClient>)>
        callback,
    std::optional<mojo_base::ProtoWrapper> wrapper) {
  std::optional<std::string> use_case =
      GetUseCaseFromFeatureConfig<FeatureConfigProto>(wrapper);

  if (!use_case.has_value() || use_case->empty()) {
    std::move(callback).Run(nullptr);
    return;
  }
  broker_client->RequestAssetsFor(*use_case);
  broker_client->GetSubscriber(*use_case).WaitForClient(std::move(callback));
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

template <typename Range>
bool AreLanguagesEnabled(
    const Range& requested,
    const std::optional<base::flat_set<std::string>>& enabled) {
  // If `enabled` is nullopt, then all languages are considered available.
  if (!enabled) {
    return true;
  }
  return std::ranges::all_of(requested, [&](const auto& lang) {
    return enabled->contains(language::ExtractBaseLanguage(lang->code));
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

bool IsLanguageInSet(const blink::mojom::AILanguageCodePtr& language,
                     const base::flat_set<std::string>& set) {
  return language &&
         set.contains(language::ExtractBaseLanguage(language->code));
}

// Checks if the provided options satisfy the requirements for the 'speed'
// performance preference:
// 1. Languages must be supported for speed preference.
// 2. Type must be TLDR or KeyPoints. 3. Length must be short or medium.
// 4. `shared_context` must not be specified.
// TODO(crbug.com/508631503): In the long term, model configs should express
// the subset of supported options, and this matching code should be more
// generalized.
enum class SpeedPreferenceIncompatibilityReason {
  kOutputLanguageNotSupported,
  kInputLanguageNotSupported,
  kContextLanguageNotSupported,
  kSharedContextNotSupported,
  kTypeNotSupported,
  kLengthNotSupported,
  kManifestBrokerDisabled,
  kLiteRTBackendDisabled,
};

base::expected<void, SpeedPreferenceIncompatibilityReason>
IsSpeedPreferenceCompatible(
    const blink::mojom::AISummarizerCreateOptionsPtr& options) {
  if (!base::FeatureList::IsEnabled(
          optimization_guide::kOptimizationGuideManifestBroker)) {
    return base::unexpected(
        SpeedPreferenceIncompatibilityReason::kManifestBrokerDisabled);
  }
  if (!base::FeatureList::IsEnabled(
          on_device_model::features::kOnDeviceModelLitertLmBackend)) {
    return base::unexpected(
        SpeedPreferenceIncompatibilityReason::kLiteRTBackendDisabled);
  }

  auto supported_langs =
      AISummarizer::GetSupportedLanguagesForSpeedPreference();

  if (options->output_language &&
      !IsLanguageInSet(options->output_language, supported_langs)) {
    return base::unexpected(
        SpeedPreferenceIncompatibilityReason::kOutputLanguageNotSupported);
  }
  if (!AreLanguagesEnabled(options->expected_input_languages,
                           supported_langs)) {
    return base::unexpected(
        SpeedPreferenceIncompatibilityReason::kInputLanguageNotSupported);
  }
  if (!AreLanguagesEnabled(options->expected_context_languages,
                           supported_langs)) {
    return base::unexpected(
        SpeedPreferenceIncompatibilityReason::kContextLanguageNotSupported);
  }
  if (options->shared_context.has_value() &&
      !options->shared_context.value().empty()) {
    return base::unexpected(
        SpeedPreferenceIncompatibilityReason::kSharedContextNotSupported);
  }

  if (options->type != blink::mojom::AISummarizerType::kTLDR &&
      options->type != blink::mojom::AISummarizerType::kKeyPoints) {
    return base::unexpected(
        SpeedPreferenceIncompatibilityReason::kTypeNotSupported);
  }

  if (options->length != blink::mojom::AISummarizerLength::kShort &&
      options->length != blink::mojom::AISummarizerLength::kMedium) {
    return base::unexpected(
        SpeedPreferenceIncompatibilityReason::kLengthNotSupported);
  }

  return base::ok();
}

std::optional<std::string> ResolveSummarizerUseCaseName(
    const blink::mojom::AISummarizerCreateOptionsPtr& options,
    const std::optional<mojo_base::ProtoWrapper>& config_wrapper) {
  // Keys used in the preference_use_cases map in the manifest.
  constexpr char kPreferenceSpeed[] = "speed";

  auto metadata =
      ParseFeatureConfig<optimization_guide::proto::SummarizerFeatureConfig>(
          config_wrapper);
  if (!metadata) {
    return std::nullopt;
  }

  std::optional<std::string> use_case =
      GetExperimentalUseCaseByModelVersion(*metadata).value_or(
          metadata->default_use_case());
  if (!options) {
    return use_case;
  }

  const char* pref_str = nullptr;
  switch (options->preference) {
    case blink::mojom::PerformancePreference::kAuto:
    case blink::mojom::PerformancePreference::kCapability:
      return use_case;
    case blink::mojom::PerformancePreference::kSpeed:
      pref_str = kPreferenceSpeed;
      break;
  }

  auto it = metadata->preference_use_cases().find(pref_str);
  if (it != metadata->preference_use_cases().end()) {
    return it->second;
  }
  VLOG(1) << "Manifest missing preference use case mapping for: " << pref_str;
  return std::nullopt;
}

}  // namespace

// Feature flag for enabling foundational models in the AI API, requires the
// field param kModelVersionParam to specify the model version. Example:
// --enable-features=AIApiFoundationalModel:model_version=v4
BASE_FEATURE(kAIApiFoundationalModel, base::FEATURE_DISABLED_BY_DEFAULT);
const char kModelVersionParam[] = "model_version";

AIManager::AIManager(content::BrowserContext* browser_context,
                     content::RenderFrameHost* rfh)
    : context_bound_object_set_(GetPriorityFromVisibility(rfh)),
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

void AIManager::AddReceiver(
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AIManager::CanCreateLanguageModel(
    blink::mojom::AILanguageModelCreateOptionsPtr options,
    CanCreateLanguageModelCallback callback) {
  if (IsPermissionsPolicyBlocked(
          network::mojom::PermissionsPolicyFeature::kLanguageModel)) {
    receivers_.ReportBadMessage("Permissions policy disabled");
    return;
  }
  if (auto pref_blocked_result = GetPrefBlockedResult()) {
    std::move(callback).Run(*pref_blocked_result);
    return;
  }

  on_device_model::Capabilities input_capabilities;
  if (options) {
    input_capabilities = GetExpectedInputCapabilities(options->expected_inputs);
    // Check if outputs request invalid types (image/audio/tool response).
    // Models can generate text and tool calls, but not multimodal content or
    // tool responses.
    if (HasInvalidOutputTypes(options->expected_outputs)) {
      std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                  kUnavailableModelAdaptationNotAvailable);
      return;
    }
    // Check if multimodal input (image/audio) is used without the feature flag.
    if (HasMultimodalInputCapabilities(input_capabilities) &&
        !base::FeatureList::IsEnabled(
            blink::features::kAIPromptAPIMultimodalInput)) {
      std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                  kUnavailableModelAdaptationNotAvailable);
      return;
    }
    // Note: Tool use capabilities are gated by RuntimeEnabledFeatures in Blink.
    // Tool use capability is signaled by the presence of tool declarations.
    if (options->tools.has_value() && !options->tools->empty()) {
      input_capabilities.Put(on_device_model::CapabilityFlags::kToolUse);
    }
  }

  if (!CheckAndFixLanguages(
          options, "LanguageModel",
          AILanguageModel::GetEnabledLanguageBaseCodes(),
          AILanguageModel::GetDefaultSupportedLanguageBaseCodes())) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }

  if (base::FeatureList::IsEnabled(
          optimization_guide::kOptimizationGuideManifestBroker)) {
    CanCreateSessionWithConfig<
        optimization_guide::proto::PromptApiFeatureConfig>(
        optimization_guide::mojom::OnDeviceFeature::kPromptApi,
        input_capabilities, std::move(callback));
  } else {
    CanCreateSession(optimization_guide::mojom::OnDeviceFeature::kPromptApi,
                     input_capabilities, std::move(callback));
  }
}

void AIManager::CreateLanguageModel(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client,
    blink::mojom::AILanguageModelCreateOptionsPtr options) {
  CHECK(options);
  if (IsBlocked(network::mojom::PermissionsPolicyFeature::kLanguageModel)) {
    receivers_.ReportBadMessage("Policy or user setting disabled");
    return;
  }
  if (!CheckAndFixLanguages(
          options, "LanguageModel",
          AILanguageModel::GetEnabledLanguageBaseCodes(),
          AILanguageModel::GetDefaultSupportedLanguageBaseCodes())) {
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote(std::move(client));
    on_device_ai::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }

  if (!model_broker_client_) {
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote(std::move(client));
    on_device_ai::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  if (base::FeatureList::IsEnabled(
          optimization_guide::kOptimizationGuideManifestBroker)) {
    model_broker_client_->GetConfig(
        optimization_guide::mojom::OnDeviceFeature::kPromptApi,
        base::BindOnce(&RequestAssetsAndWaitForClientWithConfig<
                           optimization_guide::proto::PromptApiFeatureConfig>,
                       model_broker_client_.get(),
                       base::BindOnce(&AIManager::CreateLanguageModelInternal,
                                      weak_factory_.GetWeakPtr(),
                                      std::move(client), std::move(options))));
  } else {
    model_broker_client_->RequestAssetsFor(
        optimization_guide::mojom::OnDeviceFeature::kPromptApi);
    model_broker_client_
        ->GetSubscriber(optimization_guide::mojom::OnDeviceFeature::kPromptApi)
        .WaitForClient(base::BindOnce(&AIManager::CreateLanguageModelInternal,
                                      weak_factory_.GetWeakPtr(),
                                      std::move(client), std::move(options)));
  }
}

void AIManager::CreateLanguageModelInternal(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client,
    blink::mojom::AILanguageModelCreateOptionsPtr options,
    base::WeakPtr<optimization_guide::ModelClient> model_client) {
  if (!model_client) {
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote(std::move(client));
    on_device_ai::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  blink::mojom::AILanguageModelParamsPtr language_model_params =
      GetLanguageModelParams(model_client.get());
  blink::mojom::AILanguageModelSamplingParamsPtr sampling_params =
      std::move(options->sampling_params);
  auto params = on_device_model::mojom::SessionParams::New();

  // TODO(crbug.com/502214118): Get values from model-specific configs.
  if (options->sampling_mode.has_value()) {
    switch (options->sampling_mode.value()) {
      case blink::mojom::AILanguageModelSamplingMode::kMostPredictable:
        params->temperature = kMostPredictableTemperature;
        params->top_k = kMostPredictableTopK;
        break;
      case blink::mojom::AILanguageModelSamplingMode::kPredictable:
        params->temperature = kPredictableTemperature;
        params->top_k = kPredictableTopK;
        break;
      case blink::mojom::AILanguageModelSamplingMode::kBalanced:
        params->temperature = kBalancedTemperature;
        params->top_k = kBalancedTopK;
        break;
      case blink::mojom::AILanguageModelSamplingMode::kCreative:
        params->temperature = kCreativeTemperature;
        params->top_k = kCreativeTopK;
        break;
      case blink::mojom::AILanguageModelSamplingMode::kMostCreative:
        params->temperature = kMostCreativeTemperature;
        params->top_k = kMostCreativeTopK;
        break;
    }
  } else if (sampling_params) {
    params->temperature = sampling_params->temperature;
    params->top_k = sampling_params->top_k;
  } else {
    params->temperature =
        language_model_params->default_sampling_params->temperature;
    params->top_k = language_model_params->default_sampling_params->top_k;
  }

  // Clamp the values against the model's actual capabilities
  params->top_k = std::min(std::max(kMinTopK, params->top_k),
                           language_model_params->max_sampling_params->top_k);
  params->temperature =
      std::min(std::max(kMinTemperature, params->temperature),
               language_model_params->max_sampling_params->temperature);

  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  params->capabilities = GetExpectedInputCapabilities(options->expected_inputs);
  // Tool use capability is signaled by the presence of tool declarations.
  if (options->tools.has_value() && !options->tools->empty()) {
    params->capabilities.Put(on_device_model::CapabilityFlags::kToolUse);
  }
  // Check if outputs request invalid types (image/audio/tool response).
  // Models can generate text and tool calls, but not multimodal content or
  // tool responses.
  if (HasInvalidOutputTypes(options->expected_outputs)) {
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote(std::move(client));
    on_device_ai::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }
  if (!params->capabilities.empty()) {
    // Check if multimodal input (image/audio) is used without the feature flag
    // or if the model doesn't support the requested capabilities.
    if ((HasMultimodalInputCapabilities(params->capabilities) &&
         !base::FeatureList::IsEnabled(
             blink::features::kAIPromptAPIMultimodalInput)) ||
        !model_client->capabilities().HasAll(params->capabilities)) {
      mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
          client_remote(std::move(client));
      on_device_ai::SendClientRemoteError(
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
  std::vector<blink::mojom::AILanguageModelToolDeclarationPtr> tools;
  if (options->tools.has_value()) {
    tools = std::move(options->tools.value());
  }
  model->Initialize(std::move(options->initial_prompts), std::move(tools),
                    std::move(client));

  context_bound_object_set_.AddContextBoundObject(std::move(model));

  tried_init_.insert(optimization_guide::mojom::OnDeviceFeature::kPromptApi);
  // Eagerly initialize other features, now that one successfully initialized.
  MaybeTryEagerInit();
}

void AIManager::CanCreateSummarizer(
    blink::mojom::AISummarizerCreateOptionsPtr options,
    CanCreateSummarizerCallback callback) {
  if (IsPermissionsPolicyBlocked(
          network::mojom::PermissionsPolicyFeature::kSummarizer)) {
    receivers_.ReportBadMessage("Permissions policy disabled");
    return;
  }
  if (auto pref_blocked_result = GetPrefBlockedResult()) {
    std::move(callback).Run(*pref_blocked_result);
    return;
  }
  if (!CheckAndFixLanguages(
          options, "Summarizer", AISummarizer::GetEnabledLanguageBaseCodes(),
          AISummarizer::GetDefaultSupportedLanguageBaseCodes())) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }

  if (options &&
      options->preference == blink::mojom::PerformancePreference::kSpeed) {
    if (!base::FeatureList::IsEnabled(
            blink::features::kAISummarizationPerformancePreference)) {
      receivers_.ReportBadMessage(
          "Speed preference requested but feature disabled");
      return;
    }
    auto result = IsSpeedPreferenceCompatible(options);
    if (!result.has_value()) {
      std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                  kUnavailableIncompatiblePreferenceOptions);
      return;
    }
    if (options->format == blink::mojom::AISummarizerFormat::kMarkDown) {
      MaybeLogSpeedPreferenceMarkdownWarning();
    }
  }

  if (base::FeatureList::IsEnabled(
          optimization_guide::kOptimizationGuideManifestBroker)) {
    CanCreateSessionWithConfig<
        optimization_guide::proto::SummarizerFeatureConfig>(
        optimization_guide::mojom::OnDeviceFeature::kSummarize,
        on_device_model::Capabilities(), std::move(callback),
        base::BindOnce(&ResolveSummarizerUseCaseName, std::move(options)));
  } else {
    CanCreateSession(optimization_guide::mojom::OnDeviceFeature::kSummarize,
                     on_device_model::Capabilities(), std::move(callback));
  }
}

void AIManager::CreateSummarizer(
    mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
    blink::mojom::AISummarizerCreateOptionsPtr options) {
  if (IsBlocked(network::mojom::PermissionsPolicyFeature::kSummarizer)) {
    receivers_.ReportBadMessage("Policy or user setting disabled");
    return;
  }
  if (!CheckAndFixLanguages(
          options, "Summarizer", AISummarizer::GetEnabledLanguageBaseCodes(),
          AISummarizer::GetDefaultSupportedLanguageBaseCodes())) {
    mojo::Remote<blink::mojom::AIManagerCreateSummarizerClient> client_remote(
        std::move(client));
    on_device_ai::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }

  if (options &&
      options->preference == blink::mojom::PerformancePreference::kSpeed) {
    if (!base::FeatureList::IsEnabled(
            blink::features::kAISummarizationPerformancePreference)) {
      receivers_.ReportBadMessage(
          "Speed preference requested but feature disabled");
      return;
    }
    auto result = IsSpeedPreferenceCompatible(options);
    if (!result.has_value()) {
      mojo::Remote<blink::mojom::AIManagerCreateSummarizerClient> client_remote(
          std::move(client));
      on_device_ai::SendClientRemoteError(
          client_remote, blink::mojom::AIManagerCreateClientError::
                             kIncompatiblePreferenceOptions);
      return;
    }
    if (options->format == blink::mojom::AISummarizerFormat::kMarkDown) {
      MaybeLogSpeedPreferenceMarkdownWarning();
    }
  }

  if (!model_broker_client_) {
    mojo::Remote<blink::mojom::AIManagerCreateSummarizerClient> client_remote(
        std::move(client));
    on_device_ai::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  if (options &&
      options->preference == blink::mojom::PerformancePreference::kSpeed) {
    auto* rfh = rfh_.AsRenderFrameHostIfValid();
    if (rfh) {
      rfh->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "We're rapidly iterating on the training set for the smaller expert "
          "model used with the 'speed' preference, and welcome your feedback "
          "here: https://issues.chromium.org/issues/new?component=1617227");
    }
  }

  // Clone because `options` is move-only but needed by both
  // `CreateSummarizerSessionCallback` and the use case resolver.
  auto options_clone = options ? options.Clone() : nullptr;
  auto callback =
      CreateSummarizerSessionCallback(std::move(options), std::move(client));

  if (base::FeatureList::IsEnabled(
          optimization_guide::kOptimizationGuideManifestBroker)) {
    model_broker_client_->GetConfig(
        optimization_guide::mojom::OnDeviceFeature::kSummarize,
        base::BindOnce(&CreateSessionWithConfigAndResolver,
                       model_broker_client_.get(), std::move(callback),
                       base::BindOnce(&ResolveSummarizerUseCaseName,
                                      std::move(options_clone))));
  } else {
    model_broker_client_->CreateSession(
        optimization_guide::mojom::OnDeviceFeature::kSummarize,
        ::optimization_guide::SessionConfigParams{}, std::move(callback));
  }
}

// Returns a callback to handle session creation for the summarizer.
base::OnceCallback<void(std::unique_ptr<optimization_guide::OnDeviceSession>)>
AIManager::CreateSummarizerSessionCallback(
    blink::mojom::AISummarizerCreateOptionsPtr options,
    mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client) {
  std::optional<optimization_guide::MultimodalMessage> initial_request;
  if (options->shared_context.has_value() &&
      !options->shared_context.value().empty()) {
    optimization_guide::proto::SummarizeRequest request;
    request.set_context(options->shared_context.value());
    initial_request = optimization_guide::MultimodalMessage(request);
  }
  tried_init_.insert(optimization_guide::mojom::OnDeviceFeature::kSummarize);

  return base::BindOnce(&AIManager::OnSessionCreated<
                            AISummarizer, blink::mojom::AISummarizer,
                            blink::mojom::AIManagerCreateSummarizerClient,
                            blink::mojom::AISummarizerCreateOptionsPtr>,
                        weak_factory_.GetWeakPtr(), std::move(options),
                        std::move(initial_request), std::move(client));
}

void AIManager::CanCreateProofreader(
    blink::mojom::AIProofreaderCreateOptionsPtr options,
    CanCreateProofreaderCallback callback) {
  // TODO(crbug.com/466425250): Enforce permissions policy.
  // TODO(crbug.com/424673180): Add a warning message when options
  // `includeCorrectionTypes` and `includeCorrectionExplanations` are set to
  // true as those features are not yet supported by the API.
  if (auto pref_blocked_result = GetPrefBlockedResult()) {
    std::move(callback).Run(*pref_blocked_result);
    return;
  }
  if (!CheckAndFixLanguages(
          options, "Proofreader", AIProofreader::GetEnabledLanguageBaseCodes(),
          AIProofreader::GetDefaultSupportedLanguageBaseCodes())) {
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
  // TODO(crbug.com/466425250): Enforce permissions policy.
  if (IsBlocked()) {
    receivers_.ReportBadMessage("Policy or user setting disabled");
    return;
  }
  if (!CheckAndFixLanguages(
          options, "Proofreader", AIProofreader::GetEnabledLanguageBaseCodes(),
          AIProofreader::GetDefaultSupportedLanguageBaseCodes())) {
    mojo::Remote<blink::mojom::AIManagerCreateProofreaderClient> client_remote(
        std::move(client));
    on_device_ai::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }

  if (!model_broker_client_) {
    mojo::Remote<blink::mojom::AIManagerCreateProofreaderClient> client_remote(
        std::move(client));
    on_device_ai::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  auto callback =
      base::BindOnce(&AIManager::OnSessionCreated<
                         AIProofreader, blink::mojom::AIProofreader,
                         blink::mojom::AIManagerCreateProofreaderClient,
                         blink::mojom::AIProofreaderCreateOptionsPtr>,
                     weak_factory_.GetWeakPtr(), std::move(options),
                     /*initial_request=*/std::nullopt, std::move(client));
  tried_init_.insert(
      optimization_guide::mojom::OnDeviceFeature::kProofreaderApi);
  model_broker_client_->CreateSession(
      optimization_guide::mojom::OnDeviceFeature::kProofreaderApi,
      ::optimization_guide::SessionConfigParams{}, std::move(callback));
}

blink::mojom::AILanguageModelParamsPtr AIManager::GetLanguageModelParams(
    optimization_guide::ModelClient* model_client) {
  if (!model_client) {
    return nullptr;
  }
  auto sampling_params_config = model_client->GetSamplingParamsConfig();
  if (!sampling_params_config.has_value()) {
    return nullptr;
  }

  auto model_info = blink::mojom::AILanguageModelParams::New(
      blink::mojom::AILanguageModelSamplingParams::New(),
      blink::mojom::AILanguageModelSamplingParams::New());

  model_info->default_sampling_params->top_k =
      sampling_params_config->default_top_k;
  model_info->default_sampling_params->temperature =
      sampling_params_config->default_temperature;

  model_info->max_sampling_params->top_k =
      optimization_guide::features::GetOnDeviceModelMaxTopK();
  model_info->max_sampling_params->temperature = kDefaultMaxTemperature;

  auto metadata = model_client->GetFeatureMetadata();
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
  if (!model_broker_client_) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto& subscriber = model_broker_client_->GetSubscriber(
      optimization_guide::mojom::OnDeviceFeature::kPromptApi);
  optimization_guide::ModelClient* client =
      subscriber.client().has_value() ? &subscriber.client().value() : nullptr;
  std::move(callback).Run(GetLanguageModelParams(client));
}

void AIManager::CanCreateWriter(blink::mojom::AIWriterCreateOptionsPtr options,
                                CanCreateWriterCallback callback) {
  if (IsPermissionsPolicyBlocked(
          network::mojom::PermissionsPolicyFeature::kWriter)) {
    receivers_.ReportBadMessage("Permissions policy disabled");
    return;
  }
  if (auto pref_blocked_result = GetPrefBlockedResult()) {
    std::move(callback).Run(*pref_blocked_result);
    return;
  }
  if (!CheckAndFixLanguages(options, "Writer",
                            AIWriter::GetEnabledLanguageBaseCodes(),
                            AIWriter::GetDefaultSupportedLanguageBaseCodes())) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }

  if (base::FeatureList::IsEnabled(
          optimization_guide::kOptimizationGuideManifestBroker)) {
    CanCreateSessionWithConfig<
        optimization_guide::proto::WritingAssistanceApiFeatureConfig>(
        optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi,
        on_device_model::Capabilities(), std::move(callback));
  } else {
    CanCreateSession(
        optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi,
        on_device_model::Capabilities(), std::move(callback));
  }
}

void AIManager::CreateWriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
    blink::mojom::AIWriterCreateOptionsPtr options) {
  if (IsBlocked(network::mojom::PermissionsPolicyFeature::kWriter)) {
    receivers_.ReportBadMessage("Policy or user setting disabled");
    return;
  }
  if (!CheckAndFixLanguages(options, "Writer",
                            AIWriter::GetEnabledLanguageBaseCodes(),
                            AIWriter::GetDefaultSupportedLanguageBaseCodes())) {
    mojo::Remote<blink::mojom::AIManagerCreateWriterClient> client_remote(
        std::move(client));
    on_device_ai::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }

  if (!model_broker_client_) {
    mojo::Remote<blink::mojom::AIManagerCreateWriterClient> client_remote(
        std::move(client));
    on_device_ai::SendClientRemoteError(
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
      weak_factory_.GetWeakPtr(), std::move(options),
      std::move(initial_request), std::move(client));
  tried_init_.insert(
      optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi);
  if (base::FeatureList::IsEnabled(
          optimization_guide::kOptimizationGuideManifestBroker)) {
    model_broker_client_->GetConfig(
        optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi,
        base::BindOnce(
            &CreateSessionWithConfig<
                optimization_guide::proto::WritingAssistanceApiFeatureConfig>,
            model_broker_client_.get(), std::move(callback)));
  } else {
    model_broker_client_->CreateSession(
        optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi,
        ::optimization_guide::SessionConfigParams{}, std::move(callback));
  }
}

void AIManager::CanCreateRewriter(
    blink::mojom::AIRewriterCreateOptionsPtr options,
    CanCreateRewriterCallback callback) {
  if (IsPermissionsPolicyBlocked(
          network::mojom::PermissionsPolicyFeature::kRewriter)) {
    receivers_.ReportBadMessage("Permissions policy disabled");
    return;
  }
  if (auto pref_blocked_result = GetPrefBlockedResult()) {
    std::move(callback).Run(*pref_blocked_result);
    return;
  }
  if (!CheckAndFixLanguages(
          options, "Rewriter", AIRewriter::GetEnabledLanguageBaseCodes(),
          AIRewriter::GetDefaultSupportedLanguageBaseCodes())) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  if (base::FeatureList::IsEnabled(
          optimization_guide::kOptimizationGuideManifestBroker)) {
    CanCreateSessionWithConfig<
        optimization_guide::proto::WritingAssistanceApiFeatureConfig>(
        optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi,
        on_device_model::Capabilities(), std::move(callback));
  } else {
    CanCreateSession(
        optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi,
        on_device_model::Capabilities(), std::move(callback));
  }
}

void AIManager::CreateRewriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
    blink::mojom::AIRewriterCreateOptionsPtr options) {
  if (IsBlocked(network::mojom::PermissionsPolicyFeature::kRewriter)) {
    receivers_.ReportBadMessage("Policy or user setting disabled");
    return;
  }
  if (!CheckAndFixLanguages(
          options, "Rewriter", AIRewriter::GetEnabledLanguageBaseCodes(),
          AIRewriter::GetDefaultSupportedLanguageBaseCodes())) {
    mojo::Remote<blink::mojom::AIManagerCreateRewriterClient> client_remote(
        std::move(client));
    on_device_ai::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }

  if (!model_broker_client_) {
    mojo::Remote<blink::mojom::AIManagerCreateRewriterClient> client_remote(
        std::move(client));
    on_device_ai::SendClientRemoteError(
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
      weak_factory_.GetWeakPtr(), std::move(options),
      std::move(initial_request), std::move(client));
  tried_init_.insert(
      optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi);
  if (base::FeatureList::IsEnabled(
          optimization_guide::kOptimizationGuideManifestBroker)) {
    model_broker_client_->GetConfig(
        optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi,
        base::BindOnce(
            &CreateSessionWithConfig<
                optimization_guide::proto::WritingAssistanceApiFeatureConfig>,
            model_broker_client_.get(), std::move(callback)));
  } else {
    model_broker_client_->CreateSession(
        optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi,
        ::optimization_guide::SessionConfigParams{}, std::move(callback));
  }
}

void AIManager::CanCreateClassifier(
    blink::mojom::AIClassifierCreateOptionsPtr options,
    CanCreateClassifierCallback callback) {
  // TODO(crbug.com/499365168): Enforce permissions policy and
  // CheckAndFixLanguages.
  if (auto pref_blocked_result = GetPrefBlockedResult()) {
    std::move(callback).Run(*pref_blocked_result);
    return;
  }
  CanCreateSession(optimization_guide::mojom::OnDeviceFeature::kClassifier,
                   on_device_model::Capabilities(), std::move(callback));
}

void AIManager::CreateClassifier(
    mojo::PendingRemote<blink::mojom::AIManagerCreateClassifierClient> client,
    blink::mojom::AIClassifierCreateOptionsPtr options) {
  // TODO(crbug.com/499365168): Enforce permissions policy and
  // CheckAndFixLanguages.
  if (IsBlocked()) {
    receivers_.ReportBadMessage("Policy or user setting disabled");
    return;
  }
  if (!model_broker_client_) {
    mojo::Remote<blink::mojom::AIManagerCreateClassifierClient> client_remote(
        std::move(client));
    on_device_ai::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  auto callback =
      base::BindOnce(&AIManager::OnSessionCreated<
                         AIClassifier, blink::mojom::AIClassifier,
                         blink::mojom::AIManagerCreateClassifierClient,
                         blink::mojom::AIClassifierCreateOptionsPtr>,
                     weak_factory_.GetWeakPtr(), std::move(options),
                     /*initial_request=*/std::nullopt, std::move(client));
  tried_init_.insert(optimization_guide::mojom::OnDeviceFeature::kClassifier);
  model_broker_client_->CreateSession(
      optimization_guide::mojom::OnDeviceFeature::kClassifier,
      ::optimization_guide::SessionConfigParams{}, std::move(callback));
}

void AIManager::CanCreateSession(
    optimization_guide::mojom::OnDeviceFeature capability,
    on_device_model::Capabilities capabilities,
    CanCreateLanguageModelCallback callback) {
  StartModelPathValidationIfOverrideSet();

  if (!model_broker_client_) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableServiceNotRunning);
    return;
  }

  model_broker_client_->GetSubscriber(capability)
      .CanCreateSession(
          capabilities,
          base::BindOnce(&AIManager::FinishCanCreateSession,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AIManager::CanCreateSession(const std::string& use_case_string,
                                 on_device_model::Capabilities capabilities,
                                 CanCreateLanguageModelCallback callback) {
  auto model_path =
      optimization_guide::switches::GetOnDeviceModelExecutionOverride();
  if (model_path.has_value()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(base::PathExists, model_path.value()),
        base::BindOnce(&AIManager::OnModelPathValidationComplete,
                       weak_factory_.GetWeakPtr(), model_path.value()));
  }

  if (!model_broker_client_) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableServiceNotRunning);
    return;
  }

  model_broker_client_->GetSubscriber(use_case_string)
      .CanCreateSession(
          capabilities,
          base::BindOnce(&AIManager::FinishCanCreateSession,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AIManager::FinishCanCreateSessionWithConfig(
    on_device_model::Capabilities capabilities,
    CanCreateLanguageModelCallback callback,
    UseCaseResolver resolver,
    std::optional<mojo_base::ProtoWrapper> wrapper) {
  std::optional<std::string> use_case = std::move(resolver).Run(wrapper);
  if (!use_case.has_value() || use_case->empty()) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableConfigNotAvailableForFeature);
    return;
  }
  model_broker_client_->GetSubscriber(*use_case).CanCreateSession(
      capabilities,
      base::BindOnce(&AIManager::FinishCanCreateSession,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

template <typename FeatureConfigProto>
void AIManager::CanCreateSessionWithConfig(
    optimization_guide::mojom::OnDeviceFeature capability,
    on_device_model::Capabilities capabilities,
    CanCreateLanguageModelCallback callback,
    UseCaseResolver resolver) {
  StartModelPathValidationIfOverrideSet();

  if (!model_broker_client_) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableServiceNotRunning);
    return;
  }

  model_broker_client_->GetConfig(
      capability, base::BindOnce(&AIManager::FinishCanCreateSessionWithConfig,
                                 weak_factory_.GetWeakPtr(), capabilities,
                                 std::move(callback), std::move(resolver)));
}

template <typename FeatureConfigProto>
void AIManager::CanCreateSessionWithConfig(
    optimization_guide::mojom::OnDeviceFeature capability,
    on_device_model::Capabilities capabilities,
    CanCreateLanguageModelCallback callback) {
  CanCreateSessionWithConfig<FeatureConfigProto>(
      capability, capabilities, std::move(callback),
      base::BindOnce(&GetUseCaseFromFeatureConfig<FeatureConfigProto>));
}

void AIManager::FinishCanCreateSession(
    CanCreateLanguageModelCallback callback,
    std::optional<optimization_guide::mojom::ModelUnavailableReason> reason,
    std::optional<optimization_guide::mojom::ModelNotSupportedDetailedReason>
        detailed_reason) {
  std::move(callback).Run(
      ConvertModelEligibilityReasonToModelAvailabilityCheckResult(
          reason, detailed_reason));
}

template <typename ContextBoundObjectType,
          typename ContextBoundObjectReceiverInterface,
          typename ClientRemoteInterface,
          typename CreateOptionsPtrType>
void AIManager::OnSessionCreated(
    CreateOptionsPtrType options,
    std::optional<optimization_guide::MultimodalMessage> initial_request,
    mojo::PendingRemote<ClientRemoteInterface> client,
    std::unique_ptr<optimization_guide::OnDeviceSession> session) {
  mojo::Remote<ClientRemoteInterface> client_remote(std::move(client));

  if (!session) {
    on_device_ai::SendClientRemoteError(
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
            &AIManager::OnGotExecutionInputSizeInTokens<
                ContextBoundObjectType, ContextBoundObjectReceiverInterface,
                ClientRemoteInterface, CreateOptionsPtrType>,
            weak_factory_.GetWeakPtr(), std::move(options),
            std::move(client_remote), std::move(session)));
    return;
  }

  mojo::PendingRemote<ContextBoundObjectReceiverInterface> pending_remote;
  context_bound_object_set_.AddContextBoundObject(
      std::make_unique<ContextBoundObjectType>(
          context_bound_object_set_, std::move(session), std::move(options),
          pending_remote.InitWithNewPipeAndPassReceiver()));
  client_remote->OnResult(std::move(pending_remote));
}

template <typename ContextBoundObjectType,
          typename ContextBoundObjectReceiverInterface,
          typename ClientRemoteInterface,
          typename CreateOptionsPtrType>
void AIManager::OnGotExecutionInputSizeInTokens(
    CreateOptionsPtrType options,
    mojo::Remote<ClientRemoteInterface> client_remote,
    std::unique_ptr<optimization_guide::OnDeviceSession> session,
    std::optional<uint32_t> result) {
  if (!result.has_value()) {
    on_device_ai::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCalculateTokenSize);
    return;
  }
  uint32_t quota = blink::mojom::kWritingAssistanceMaxInputTokenSize;
  if (result.value() > quota) {
    on_device_ai::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge,
        blink::mojom::QuotaErrorInfo::New(result.value(), quota));
    return;
  }
  mojo::PendingRemote<ContextBoundObjectReceiverInterface> pending_remote;
  context_bound_object_set_.AddContextBoundObject(
      std::make_unique<ContextBoundObjectType>(
          context_bound_object_set_, std::move(session), std::move(options),
          pending_remote.InitWithNewPipeAndPassReceiver()));
  client_remote->OnResult(std::move(pending_remote));
}

void AIManager::MaybeTryEagerInit() {
  if (!base::FeatureList::IsEnabled(kBuiltInAIEagerInit)) {
    return;
  }
  // Initialize other features when one is used. This presumes a large common
  // model download completed with the first feature usage, and other features
  // just need lightweight configuration downloads to become readily available
  // for usage on this device.
  AIContextBoundObjectSet empty(on_device_model::mojom::Priority::kBackground);
  for (optimization_guide::mojom::OnDeviceFeature feature :
       {optimization_guide::mojom::OnDeviceFeature::kPromptApi,
        optimization_guide::mojom::OnDeviceFeature::kSummarize,
        optimization_guide::mojom::OnDeviceFeature::kWritingAssistanceApi}) {
    // TODO(crbug.com/447192715): Gate on runtime determined component size.
    if (tried_init_.insert(feature).second && model_broker_client_) {
      model_broker_client_->RequestAssetsFor(feature);
    }
  }
}

template <typename OptionsPtrType>
bool AIManager::CheckAndFixLanguages(
    OptionsPtrType& options,
    std::string_view api_name,
    const std::optional<base::flat_set<std::string>>& enabled,
    const base::flat_set<std::string>& default_supported) {
  LanguageSet languages = GetLanguages(options);
  if (!AreLanguagesEnabled(languages, enabled)) {
    MaybeLogUnsupportedLanguageError(api_name, enabled);
    return false;
  }
  if (!AreLanguagesEnabled(languages, default_supported)) {
    MaybeLogExperimentalLanguageWarning(api_name, default_supported);
  }
  if (!options || !CheckAndFixOutputLanguage(options, languages)) {
    MaybeLogMissingOutputLanguageWarning(api_name, enabled);
  }
  return true;
}

std::optional<blink::mojom::ModelAvailabilityCheckResult>
AIManager::GetPrefBlockedResult() {
  PrefService* local_state = g_browser_process->local_state();
  // chromeenterprise.google/policies/#GenAILocalFoundationalModelSettings
  if (optimization_guide::
          GetGenAILocalFoundationalModelEnterprisePolicySettings(local_state) ==
      optimization_guide::model_execution::prefs::
          GenAILocalFoundationalModelEnterprisePolicySettings::kDisallowed) {
    return blink::mojom::ModelAvailabilityCheckResult::
        kUnavailableEnterprisePolicyDisabled;
  }

  PrefService* profile_prefs =
      Profile::FromBrowserContext(browser_context_)->GetPrefs();
  // chromeenterprise.google/policies/#BuiltInAIAPIsEnabled
  if (!profile_prefs->GetBoolean(policy::policy_prefs::kBuiltInAIAPIsEnabled)) {
    return blink::mojom::ModelAvailabilityCheckResult::
        kUnavailableEnterprisePolicyDisabled;
  }

  // chrome://settings/system "On-device AI" user toggle.
  if (!local_state->GetBoolean(
          optimization_guide::model_execution::prefs::localstate::
              kOnDeviceAiUserSettingsEnabled)) {
    return blink::mojom::ModelAvailabilityCheckResult::
        kUnavailableFeatureNotEnabled;
  }

  return std::nullopt;
}

bool AIManager::IsPermissionsPolicyBlocked(
    network::mojom::PermissionsPolicyFeature feature) {
  auto* rfh = rfh_.AsRenderFrameHostIfValid();
  return rfh && !rfh->IsFeatureEnabled(feature);
}

bool AIManager::IsBlocked(
    std::optional<network::mojom::PermissionsPolicyFeature> feature) {
  if (feature.has_value() && IsPermissionsPolicyBlocked(feature.value())) {
    return true;
  }
  return GetPrefBlockedResult().has_value();
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

void AIManager::StartModelPathValidationIfOverrideSet() {
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
}

void AIManager::AddModelDownloadProgressObserver(
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver>
        observer_remote) {
  if (model_broker_client_) {
    model_broker_client_->AddModelDownloadProgressObserver(
        std::move(observer_remote));
  }
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
    const std::optional<base::flat_set<std::string>>& enabled_languages) {
  auto* rfh = rfh_.AsRenderFrameHostIfValid();
  if (!rfh || did_log_missing_output_language_warning_) {
    return;
  }
  did_log_missing_output_language_warning_ = true;
  auto list =
      enabled_languages ? base::JoinString(*enabled_languages, ", ") : "*";
  rfh->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning,
      base::StringPrintf(kEmptyOutputLanguageWarning, api_name, list));
}

void AIManager::MaybeLogUnsupportedLanguageError(
    const std::string_view api_name,
    const std::optional<base::flat_set<std::string>>& enabled_languages) {
  auto* rfh = rfh_.AsRenderFrameHostIfValid();
  if (!rfh || did_log_unsupported_language_error_) {
    return;
  }
  did_log_unsupported_language_error_ = true;
  auto list =
      enabled_languages ? base::JoinString(*enabled_languages, ", ") : "*";
  rfh->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError,
      base::StringPrintf(kUnsupportedLanguageError, api_name, list));
}

void AIManager::MaybeLogExperimentalLanguageWarning(
    const std::string_view api_name,
    const base::flat_set<std::string>& default_supported_languages) {
  auto* rfh = rfh_.AsRenderFrameHostIfValid();
  if (!rfh || did_log_experimental_language_warning_) {
    return;
  }
  did_log_experimental_language_warning_ = true;
  auto list = base::JoinString(default_supported_languages, ", ");
  rfh->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning,
      base::StringPrintf(kExperimentalLanguageWarning, api_name, list));
}

void AIManager::MaybeLogSpeedPreferenceMarkdownWarning() {
  auto* rfh = rfh_.AsRenderFrameHostIfValid();
  if (!rfh || did_log_speed_preference_markdown_warning_) {
    return;
  }
  did_log_speed_preference_markdown_warning_ = true;
  rfh->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                           kSpeedPreferenceMarkdownWarning);
}

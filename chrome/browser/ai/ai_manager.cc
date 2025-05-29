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
#include "base/functional/callback_forward.h"
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
#include "chrome/browser/ai/ai_language_model.h"
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
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/common/page_visibility_state.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
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

// TODO(crbug.com/394841624): Consider using the model execution config instead
// of using the hardcoded list.
const char kUnsupportedLanguageError[] =
    "Cannot proceed with API call for %s. Expected language contains one or "
    "more unsupported languages. This API call requires expected language to "
    "specify only languages from our supported list to ensure successful "
    "processing and guarantee output characteristics. Please update it with "
    "valid language codes from this list: ['en']";
const char kEmptyExpectedOutputLanguageWarning[] =
    "The output language is not specified for %s API call. Without specifying "
    "the language, we cannot ensure optimal output quality or properly attest "
    "to output safety for potentially unsupported languages. Please specify it "
    "when possible for best and most reliable results using our supported "
    "list: ['en']";

// Checks if the model path configured via command line is valid.
bool IsModelPathValid(const std::string& model_path_str) {
  std::optional<base::FilePath> model_path =
      optimization_guide::StringToFilePath(model_path_str);
  if (!model_path) {
    return false;
  }
  return base::PathExists(*model_path);
}

blink::mojom::ModelAvailabilityCheckResult
ConvertOnDeviceModelEligibilityReasonToModelAvailabilityCheckResult(
    optimization_guide::OnDeviceModelEligibilityReason
        on_device_model_eligibility_reason,
    bool is_downloading) {
  auto availability = optimization_guide::AvailabilityFromEligibilityReason(
      on_device_model_eligibility_reason);
  if (availability ==
      optimization_guide::mojom::ModelUnavailableReason::kPendingAssets) {
    if (is_downloading) {
      return blink::mojom::ModelAvailabilityCheckResult::kDownloading;
    }
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
      if (is_downloading) {
        return blink::mojom::ModelAvailabilityCheckResult::kDownloading;
      }
      return blink::mojom::ModelAvailabilityCheckResult::kDownloadable;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kDeprecatedModelNotAvailable:
    case optimization_guide::OnDeviceModelEligibilityReason::kSuccess:
      NOTREACHED();
  }
  NOTREACHED();
}

// TODO(crbug.com/394841624): Consider using the model execution config instead
// of using the hardcoded list.
// Checks for supported language code options (currently just "en").
auto is_language_supported = [](const AILanguageCodePtr& language) {
  return language->code.empty() ||
         language::ExtractBaseLanguage(language->code) == "en";
};

// Returns whether optional LanguageModel expected_inputs or expected_outputs
// vectors contain only supported languages. Returns true for absent languages.
bool AreExpectedLanguagesSupported(
    const std::optional<std::vector<blink::mojom::AILanguageModelExpectedPtr>>&
        expected_vector) {
  if (!expected_vector) {
    return true;
  }
  for (const auto& expected_entry : expected_vector.value()) {
    if (expected_entry->languages.has_value() &&
        !AIManager::IsLanguagesSupported(expected_entry->languages.value())) {
      return false;
    }
  }
  return true;
}

template <typename ContextBoundObjectType,
          typename ContextBoundObjectReceiverInterface,
          typename ClientRemoteInterface,
          typename CreateOptionsPtrType>
void OnSessionCreated(
    AIContextBoundObjectSet& context_bound_object_set,
    CreateOptionsPtrType options,
    std::optional<optimization_guide::MultimodalMessage> initial_request,
    mojo::Remote<ClientRemoteInterface> client_remote,
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session) {
  if (!session) {
    AIUtils::AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  if (initial_request.has_value()) {
    session->GetExecutionInputSizeInTokens(
        initial_request.value().read(),
        base::BindOnce(
            [](AIContextBoundObjectSet& context_bound_object_set,
               CreateOptionsPtrType options,
               mojo::Remote<ClientRemoteInterface> client_remote,
               std::unique_ptr<
                   optimization_guide::OptimizationGuideModelExecutor::Session>
                   session,
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

// TODO(crbug.com/402442890): Move this to `ai_create_on_device_session_task.cc`
template <typename ClientRemoteInterface>
class CreateWritingAssistanceSessionTask : public CreateOnDeviceSessionTask {
 public:
  using WritingAssistanceSessionTaskCallback = base::OnceCallback<void(
      mojo::Remote<ClientRemoteInterface>,
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session>)>;

  static void CreateAndStart(
      content::BrowserContext* browser_context,
      optimization_guide::ModelBasedCapabilityKey feature,
      AIContextBoundObjectSet& context_bound_object_set,
      WritingAssistanceSessionTaskCallback callback,
      mojo::PendingRemote<ClientRemoteInterface> client) {
    auto task = std::make_unique<CreateWritingAssistanceSessionTask>(
        base::PassKey<CreateWritingAssistanceSessionTask>(), browser_context,
        feature, context_bound_object_set, std::move(callback),
        std::move(client));
    task->Start();
    if (task->IsPending()) {
      // Put `task` to AIContextBoundObjectSet to continue observing the model
      // availability.
      context_bound_object_set.AddContextBoundObject(std::move(task));
    }
  }

  CreateWritingAssistanceSessionTask(
      base::PassKey<CreateWritingAssistanceSessionTask>,
      content::BrowserContext* browser_context,
      optimization_guide::ModelBasedCapabilityKey feature,
      AIContextBoundObjectSet& context_bound_object_set,
      WritingAssistanceSessionTaskCallback callback,
      mojo::PendingRemote<ClientRemoteInterface> client)
      : CreateOnDeviceSessionTask(context_bound_object_set,
                                  browser_context,
                                  feature),
        callback_(std::move(callback)),
        client_remote_(std::move(client)) {
    client_remote_.set_disconnect_handler(base::BindOnce(
        &CreateWritingAssistanceSessionTask::Cancel, base::Unretained(this)));
  }
  ~CreateWritingAssistanceSessionTask() override = default;

 protected:
  void OnFinish(std::unique_ptr<
                optimization_guide::OptimizationGuideModelExecutor::Session>
                    session) override {
    std::move(callback_).Run(std::move(client_remote_), std::move(session));
  }

 private:
  WritingAssistanceSessionTaskCallback callback_;
  mojo::Remote<ClientRemoteInterface> client_remote_;
};

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

// static
bool AIManager::IsLanguagesSupported(
    const std::vector<AILanguageCodePtr>& languages) {
  return std::ranges::all_of(languages, is_language_supported);
}

// static
bool AIManager::IsLanguagesSupported(
    const std::vector<AILanguageCodePtr>& input,
    const std::vector<AILanguageCodePtr>& context,
    const AILanguageCodePtr& output) {
  return IsLanguagesSupported(input) && IsLanguagesSupported(context) &&
         is_language_supported(output);
}

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

void AIManager::AddMessageToConsoleForUnexpectedLanguage(
    blink::mojom::ConsoleMessageLevel level,
    std::string message) {
  bool* flag = nullptr;
  if (level == blink::mojom::ConsoleMessageLevel::kWarning) {
    flag = &did_add_warning_console_message_for_unexpected_language_;
  }
  if (level == blink::mojom::ConsoleMessageLevel::kError) {
    flag = &did_add_error_console_message_for_unexpected_language_;
  }
  if (flag != nullptr) {
    if (*flag) {
      return;
    }
    *flag = true;
  }

  if (auto* rfh = rfh_.AsRenderFrameHostIfValid()) {
    rfh->AddMessageToConsole(level, message);
  }
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
    if (!options || !options->expected_outputs) {
      AddMessageToConsoleForUnexpectedLanguage(
          blink::mojom::ConsoleMessageLevel::kWarning,
          base::StringPrintf(kEmptyExpectedOutputLanguageWarning,
                             "LanguageModel"));
    }
    if (!AreExpectedLanguagesSupported(options->expected_inputs) ||
        !AreExpectedLanguagesSupported(options->expected_outputs)) {
      AddMessageToConsoleForUnexpectedLanguage(
          blink::mojom::ConsoleMessageLevel::kError,
          base::StringPrintf(kUnsupportedLanguageError, "LanguageModel"));
      std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                  kUnavailableUnsupportedLanguage);
      return;
    }
  }

  CanCreateSession(optimization_guide::ModelBasedCapabilityKey::kPromptApi,
                   input_capabilities, std::move(callback));
}

void AIManager::CreateLanguageModel(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client,
    blink::mojom::AILanguageModelCreateOptionsPtr options) {
  CHECK(options);
  if (!AreExpectedLanguagesSupported(options->expected_inputs) ||
      !AreExpectedLanguagesSupported(options->expected_outputs)) {
    AddMessageToConsoleForUnexpectedLanguage(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf(kUnsupportedLanguageError, "LanguageModel"));
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
      ->GetSubscriber(
          optimization_guide::mojom::ModelBasedCapabilityKey::kPromptApi)
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
}

void AIManager::CanCreateSummarizer(
    blink::mojom::AISummarizerCreateOptionsPtr options,
    CanCreateSummarizerCallback callback) {
  if (!IsBuiltInAIAPIsEnabledByPolicy()) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableEnterprisePolicyDisabled);
    return;
  }
  if (!options || !options->output_language) {
    AddMessageToConsoleForUnexpectedLanguage(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(kEmptyExpectedOutputLanguageWarning, "Summarizer"));
  }
  if (options && !IsLanguagesSupported(options->expected_input_languages,
                                       options->expected_context_languages,
                                       options->output_language)) {
    AddMessageToConsoleForUnexpectedLanguage(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf(kUnsupportedLanguageError, "Summarizer"));
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  CanCreateSession(optimization_guide::ModelBasedCapabilityKey::kSummarize,
                   on_device_model::Capabilities(), std::move(callback));
}

void AIManager::CreateSummarizer(
    mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
    blink::mojom::AISummarizerCreateOptionsPtr options) {
  if (!options || !options->output_language) {
    AddMessageToConsoleForUnexpectedLanguage(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(kEmptyExpectedOutputLanguageWarning, "Summarizer"));
  }
  if (options && !IsLanguagesSupported(options->expected_input_languages,
                                       options->expected_context_languages,
                                       options->output_language)) {
    AddMessageToConsoleForUnexpectedLanguage(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf(kUnsupportedLanguageError, "Summarizer"));
    mojo::Remote<blink::mojom::AIManagerCreateSummarizerClient> client_remote(
        std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
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
      &OnSessionCreated<AISummarizer, blink::mojom::AISummarizer,
                        blink::mojom::AIManagerCreateSummarizerClient,
                        blink::mojom::AISummarizerCreateOptionsPtr>,
      std::ref(context_bound_object_set_), std::move(options),
      std::move(initial_request));
  CreateWritingAssistanceSessionTask<
      blink::mojom::AIManagerCreateSummarizerClient>::
      CreateAndStart(browser_context_,
                     optimization_guide::ModelBasedCapabilityKey::kSummarize,
                     context_bound_object_set_, std::move(callback),
                     std::move(client));
}

blink::mojom::AILanguageModelParamsPtr AIManager::GetLanguageModelParams() {
  auto model_info = blink::mojom::AILanguageModelParams::New(
      blink::mojom::AILanguageModelSamplingParams::New(),
      blink::mojom::AILanguageModelSamplingParams::New());
  model_info->max_sampling_params->top_k = GetLanguageModelMaxTopK();
  model_info->max_sampling_params->temperature =
      GetLanguageModelMaxTemperature();

  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  auto sampling_params_config = service->GetSamplingParamsConfig(
      optimization_guide::ModelBasedCapabilityKey::kPromptApi);

  if (!sampling_params_config.has_value()) {
    return model_info;
  }

  model_info->default_sampling_params->top_k =
      sampling_params_config->default_top_k;
  model_info->default_sampling_params->temperature =
      sampling_params_config->default_temperature;

  auto metadata = service->GetFeatureMetadata(
      optimization_guide::ModelBasedCapabilityKey::kPromptApi);
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
  if (!options || !options->output_language) {
    AddMessageToConsoleForUnexpectedLanguage(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(kEmptyExpectedOutputLanguageWarning, "Writer"));
  }
  if (options && !IsLanguagesSupported(options->expected_input_languages,
                                       options->expected_context_languages,
                                       options->output_language)) {
    AddMessageToConsoleForUnexpectedLanguage(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf(kUnsupportedLanguageError, "Writer"));
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  CanCreateSession(
      optimization_guide::ModelBasedCapabilityKey::kWritingAssistanceApi,
      on_device_model::Capabilities(), std::move(callback));
}

void AIManager::CreateWriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
    blink::mojom::AIWriterCreateOptionsPtr options) {
  if (!options || !options->output_language) {
    AddMessageToConsoleForUnexpectedLanguage(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(kEmptyExpectedOutputLanguageWarning, "Writer"));
  }
  if (options && !IsLanguagesSupported(options->expected_input_languages,
                                       options->expected_context_languages,
                                       options->output_language)) {
    AddMessageToConsoleForUnexpectedLanguage(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf(kUnsupportedLanguageError, "Writer"));
    mojo::Remote<blink::mojom::AIManagerCreateWriterClient> client_remote(
        std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
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
      &OnSessionCreated<AIWriter, blink::mojom::AIWriter,
                        blink::mojom::AIManagerCreateWriterClient,
                        blink::mojom::AIWriterCreateOptionsPtr>,
      std::ref(context_bound_object_set_), std::move(options),
      std::move(initial_request));
  CreateWritingAssistanceSessionTask<
      blink::mojom::AIManagerCreateWriterClient>::
      CreateAndStart(
          browser_context_,
          optimization_guide::ModelBasedCapabilityKey::kWritingAssistanceApi,
          context_bound_object_set_, std::move(callback), std::move(client));
}

void AIManager::CanCreateRewriter(
    blink::mojom::AIRewriterCreateOptionsPtr options,
    CanCreateRewriterCallback callback) {
  if (!IsBuiltInAIAPIsEnabledByPolicy()) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableEnterprisePolicyDisabled);
    return;
  }
  if (!options || !options->output_language) {
    AddMessageToConsoleForUnexpectedLanguage(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(kEmptyExpectedOutputLanguageWarning, "Rewriter"));
  }
  if (options && !IsLanguagesSupported(options->expected_input_languages,
                                       options->expected_context_languages,
                                       options->output_language)) {
    AddMessageToConsoleForUnexpectedLanguage(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf(kUnsupportedLanguageError, "Rewriter"));
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  CanCreateSession(
      optimization_guide::ModelBasedCapabilityKey::kWritingAssistanceApi,
      on_device_model::Capabilities(), std::move(callback));
}

void AIManager::CreateRewriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
    blink::mojom::AIRewriterCreateOptionsPtr options) {
  if (!options || !options->output_language) {
    AddMessageToConsoleForUnexpectedLanguage(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(kEmptyExpectedOutputLanguageWarning, "Rewriter"));
  }
  if (options && !IsLanguagesSupported(options->expected_input_languages,
                                       options->expected_context_languages,
                                       options->output_language)) {
    AddMessageToConsoleForUnexpectedLanguage(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf(kUnsupportedLanguageError, "Rewriter"));
    mojo::Remote<blink::mojom::AIManagerCreateRewriterClient> client_remote(
        std::move(client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
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
      &OnSessionCreated<AIRewriter, blink::mojom::AIRewriter,
                        blink::mojom::AIManagerCreateRewriterClient,
                        blink::mojom::AIRewriterCreateOptionsPtr>,
      std::ref(context_bound_object_set_), std::move(options),
      std::move(initial_request));
  CreateWritingAssistanceSessionTask<
      blink::mojom::AIManagerCreateRewriterClient>::
      CreateAndStart(
          browser_context_,
          optimization_guide::ModelBasedCapabilityKey::kWritingAssistanceApi,
          context_bound_object_set_, std::move(callback), std::move(client));
}

void AIManager::CanCreateSession(
    optimization_guide::ModelBasedCapabilityKey capability,
    on_device_model::Capabilities capabilities,
    CanCreateLanguageModelCallback callback) {
  auto model_path =
      optimization_guide::switches::GetOnDeviceModelExecutionOverride();
  if (model_path.has_value()) {
    // If the model path is provided, we do this additional check and post a
    // warning message to dev tools if it's invalid.
    // This needs to be done in a task runner with `MayBlock` trait.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(IsModelPathValid, model_path.value()),
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
    optimization_guide::ModelBasedCapabilityKey capability,
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
    bool is_downloading =
        model_download_progress_manager_.GetNumberOfReporters() >= 1;
    std::move(callback).Run(
        ConvertOnDeviceModelEligibilityReasonToModelAvailabilityCheckResult(
            eligibility, is_downloading));
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

void AIManager::OnModelPathValidationComplete(const std::string& model_path,
                                              bool is_valid_path) {
  // TODO(crbug.com/346491542): Remove this when the error page is implemented.
  if (!is_valid_path) {
    VLOG(1) << base::StringPrintf(
        "Unable to create a session because the model path ('%s') is invalid.",
        model_path.c_str());
  }
}

// TODO(crbug.com/367771112): remove these methods after we roll out the model
// execution config change.
uint32_t AIManager::GetLanguageModelMaxTopK() {
  if (base::FeatureList::IsEnabled(
          features::kAILanguageModelOverrideConfiguration)) {
    return std::min(
        optimization_guide::features::GetOnDeviceModelMaxTopK(),
        features::kAILanguageModelOverrideConfigurationMaxTopK.Get());
  }

  return optimization_guide::features::GetOnDeviceModelMaxTopK();
}

float AIManager::GetLanguageModelMaxTemperature() {
  if (base::FeatureList::IsEnabled(
          features::kAILanguageModelOverrideConfiguration)) {
    return std::min(
        kDefaultMaxTemperature,
        static_cast<float>(
            features::kAILanguageModelOverrideConfigurationMaxTemperature
                .Get()));
  }

  return kDefaultMaxTemperature;
}

void AIManager::AddModelDownloadProgressObserver(
    mojo::PendingRemote<blink ::mojom::ModelDownloadProgressObserver>
        observer_remote) {
  model_download_progress_manager_.AddObserver(
      &component_update_service_.get(), std::move(observer_remote),
      {component_updater::OptimizationGuideOnDeviceModelInstallerPolicy::
           GetOnDeviceModelExtensionId()});
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

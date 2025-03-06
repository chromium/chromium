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
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_writer.mojom.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"

namespace {

constexpr float kDefaultMaxTemperature = 2.0f;

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

template <typename ContextBoundObjectType,
          typename ContextBoundObjectReceiverInterface,
          typename ClientRemoteInterface,
          typename CreateOptionsPtrType>
class CreateContextBoundObjectTask : public CreateOnDeviceSessionTask {
 public:
  using CreateObjectCallback =
      base::OnceCallback<std::unique_ptr<ContextBoundObjectType>(
          std::unique_ptr<
              optimization_guide::OptimizationGuideModelExecutor::Session>,
          mojo::PendingReceiver<ContextBoundObjectReceiverInterface>)>;
  static void CreateAndStart(
      content::BrowserContext* browser_context,
      optimization_guide::ModelBasedCapabilityKey feature,
      AIContextBoundObjectSet& context_bound_object_set,
      CreateOptionsPtrType options,
      mojo::PendingRemote<ClientRemoteInterface> client) {
    auto task = std::make_unique<CreateContextBoundObjectTask>(
        base::PassKey<CreateContextBoundObjectTask>(), browser_context, feature,
        context_bound_object_set, std::move(options), std::move(client));
    task->Start();
    if (task->IsPending()) {
      // Put `task` to AIContextBoundObjectSet to continue observing the model
      // availability.
      context_bound_object_set.AddContextBoundObject(std::move(task));
    }
  }

  CreateContextBoundObjectTask(
      base::PassKey<CreateContextBoundObjectTask>,
      content::BrowserContext* browser_context,
      optimization_guide::ModelBasedCapabilityKey feature,
      AIContextBoundObjectSet& context_bound_object_set,
      CreateOptionsPtrType options,
      mojo::PendingRemote<ClientRemoteInterface> client)
      : CreateOnDeviceSessionTask(context_bound_object_set,
                                  browser_context,
                                  feature),
        context_bound_object_set_(context_bound_object_set),
        options_(std::move(options)),
        client_remote_(std::move(client)) {
    client_remote_.set_disconnect_handler(base::BindOnce(
        &CreateContextBoundObjectTask::Cancel, base::Unretained(this)));
  }
  ~CreateContextBoundObjectTask() override = default;

 protected:
  void OnFinish(std::unique_ptr<
                optimization_guide::OptimizationGuideModelExecutor::Session>
                    session) override {
    if (!session) {
      client_remote_->OnError(
          blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
      return;
    }
    mojo::PendingRemote<ContextBoundObjectReceiverInterface> pending_remote;
    context_bound_object_set_->AddContextBoundObject(
        std::make_unique<ContextBoundObjectType>(
            context_bound_object_set_.get(), std::move(session),
            std::move(options_),
            pending_remote.InitWithNewPipeAndPassReceiver()));
    client_remote_->OnResult(std::move(pending_remote));
  }

 private:
  // Both of `CreateContextBoundObjectTask` and `AIContextBoundObjectSet` are
  // owned by the `AIManager`.
  const raw_ref<AIContextBoundObjectSet> context_bound_object_set_;
  CreateOptionsPtrType options_;
  mojo::Remote<ClientRemoteInterface> client_remote_;
};

}  // namespace

AIManager::AIManager(content::BrowserContext* browser_context)
    : component_observer_(
          std::make_unique<AIOnDeviceModelComponentObserver>(this)),
      browser_context_(browser_context) {}

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

void AIManager::AddReceiver(
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AIManager::CanCreateLanguageModel(
    std::optional<std::vector<blink::mojom::AILanguageCodePtr>>
        expected_input_languages,
    CanCreateLanguageModelCallback callback) {
  if (expected_input_languages.has_value() &&
      !IsLanguagesSupported(expected_input_languages.value())) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }

  CanCreateSession(optimization_guide::ModelBasedCapabilityKey::kPromptApi,
                   std::move(callback));
}

std::unique_ptr<CreateLanguageModelOnDeviceSessionTask>
AIManager::CreateLanguageModelInternal(
    const blink::mojom::AILanguageModelSamplingParamsPtr& sampling_params,
    AIContextBoundObjectSet& context_bound_object_set,
    AIUtils::LanguageCodes expected_input_languages,
    base::OnceCallback<void(AILanguageModelOrCreationError)> callback,
    const std::optional<const AILanguageModel::Context>& context) {
  auto task = std::make_unique<CreateLanguageModelOnDeviceSessionTask>(
      *this, context_bound_object_set, browser_context_, sampling_params,
      base::BindOnce(
          [](base::WeakPtr<content::BrowserContext> browser_context,
             AIContextBoundObjectSet& context_bound_object_set,
             AIUtils::LanguageCodes expected_input_languages,
             const std::optional<const AILanguageModel::Context>& context,
             AIManager& ai_manager,
             base::OnceCallback<void(
                 base::expected<std::unique_ptr<AILanguageModel>,
                                blink::mojom::AIManagerCreateClientError>)>
                 callback,
             std::unique_ptr<
                 optimization_guide::OptimizationGuideModelExecutor::Session>
                 session) {
            if (!session) {
              std::move(callback).Run(
                  base::unexpected(blink::mojom::AIManagerCreateClientError::
                                       kUnableToCalculateTokenSize));
              return;
            }

            mojo::PendingRemote<blink::mojom::AILanguageModel> pending_remote;
            std::move(callback).Run(std::make_unique<AILanguageModel>(
                std::move(session), browser_context, std::move(pending_remote),
                context_bound_object_set, ai_manager,
                std::move(expected_input_languages), context));
          },
          browser_context_->GetWeakPtr(), std::ref(context_bound_object_set),
          std::move(expected_input_languages), context, std::ref(*this),
          std::move(callback)));
  task->Start();
  return task;
}

void AIManager::CreateLanguageModel(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client,
    blink::mojom::AILanguageModelCreateOptionsPtr options) {
  CHECK(options);

  if (options->expected_input_languages.has_value() &&
      !IsLanguagesSupported(options->expected_input_languages.value())) {
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote(std::move(client));
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }

  blink::mojom::AILanguageModelSamplingParamsPtr sampling_params =
      std::move(options->sampling_params);
  AIUtils::LanguageCodes expected_input_languages =
      std::move(options->expected_input_languages);

  auto create_language_model_callback = base::BindOnce(
      [](mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
             client,
         AIContextBoundObjectSet& context_bound_object_set,
         blink::mojom::AILanguageModelCreateOptionsPtr options,
         AILanguageModelOrCreationError creation_result) {
        mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
            client_remote(std::move(client));
        if (!creation_result.has_value()) {
          client_remote->OnError(creation_result.error());
          return;
        }
        std::unique_ptr<AILanguageModel> language_model =
            std::move(creation_result.value());
        CHECK(language_model);

        const std::optional<std::string>& system_prompt =
            options->system_prompt;
        std::vector<blink::mojom::AILanguageModelPromptPtr>& initial_prompts =
            options->initial_prompts;
        if (system_prompt.has_value() || !initial_prompts.empty()) {
          // If the initial prompt is provided, we need to set it and
          // invoke the callback after this, because the token counting
          // happens asynchronously.
          language_model->SetInitialPrompts(
              system_prompt, std::move(initial_prompts),
              base::BindOnce(
                  [](mojo::Remote<
                         blink::mojom::AIManagerCreateLanguageModelClient>
                         client_remote,
                     base::expected<
                         mojo::PendingRemote<blink::mojom::AILanguageModel>,
                         blink::mojom::AIManagerCreateClientError> remote,
                     blink::mojom::AILanguageModelInstanceInfoPtr info) {
                    if (remote.has_value()) {
                      client_remote->OnResult(std::move(remote.value()),
                                              std::move(info));
                    } else {
                      client_remote->OnError(remote.error());
                    }
                  },
                  std::move(client_remote)));
        } else {
          client_remote->OnResult(
              language_model->TakePendingRemote(),
              language_model->GetLanguageModelInstanceInfo());
        }

        context_bound_object_set.AddContextBoundObject(
            std::move(language_model));
      },
      std::move(client), std::ref(context_bound_object_set_),
      std::move(options));

  // When creating a new language model, the `context` will not be set since it
  // should start fresh.
  auto task =
      CreateLanguageModelInternal(sampling_params, context_bound_object_set_,
                                  std::move(expected_input_languages),
                                  std::move(create_language_model_callback));
  if (task->IsPending()) {
    // Put `task` to AIContextBoundObjectSet to continue observing the model
    // availability.
    context_bound_object_set_.AddContextBoundObject(std::move(task));
  }
}

void AIManager::CanCreateSummarizer(
    blink::mojom::AISummarizerCreateOptionsPtr options,
    CanCreateSummarizerCallback callback) {
  if (options && !IsLanguagesSupported(options->expected_input_languages,
                                       options->expected_context_languages,
                                       options->output_language)) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  CanCreateSession(optimization_guide::ModelBasedCapabilityKey::kSummarize,
                   std::move(callback));
}

void AIManager::CreateSummarizer(
    mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
    blink::mojom::AISummarizerCreateOptionsPtr options) {
  if (options && !IsLanguagesSupported(options->expected_input_languages,
                                       options->expected_context_languages,
                                       options->output_language)) {
    mojo::Remote<blink::mojom::AIManagerCreateSummarizerClient> client_remote(
        std::move(client));
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }
  CreateContextBoundObjectTask<AISummarizer, blink::mojom::AISummarizer,
                               blink::mojom::AIManagerCreateSummarizerClient,
                               blink::mojom::AISummarizerCreateOptionsPtr>::
      CreateAndStart(browser_context_,
                     optimization_guide::ModelBasedCapabilityKey::kSummarize,
                     context_bound_object_set_, std::move(options),
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
  if (options && !IsLanguagesSupported(options->expected_input_languages,
                                       options->expected_context_languages,
                                       options->output_language)) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  CanCreateSession(
      optimization_guide::ModelBasedCapabilityKey::kWritingAssistanceApi,
      std::move(callback));
}

void AIManager::CreateWriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
    blink::mojom::AIWriterCreateOptionsPtr options) {
  if (options && !IsLanguagesSupported(options->expected_input_languages,
                                       options->expected_context_languages,
                                       options->output_language)) {
    mojo::Remote<blink::mojom::AIManagerCreateWriterClient> client_remote(
        std::move(client));
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }
  CreateContextBoundObjectTask<AIWriter, blink::mojom::AIWriter,
                               blink::mojom::AIManagerCreateWriterClient,
                               blink::mojom::AIWriterCreateOptionsPtr>::
      CreateAndStart(
          browser_context_,
          optimization_guide::ModelBasedCapabilityKey::kWritingAssistanceApi,
          context_bound_object_set_, std::move(options), std::move(client));
}

void AIManager::CanCreateRewriter(
    blink::mojom::AIRewriterCreateOptionsPtr options,
    CanCreateRewriterCallback callback) {
  if (options && !IsLanguagesSupported(options->expected_input_languages,
                                       options->expected_context_languages,
                                       options->output_language)) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  CanCreateSession(
      optimization_guide::ModelBasedCapabilityKey::kWritingAssistanceApi,
      std::move(callback));
}

void AIManager::CreateRewriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
    blink::mojom::AIRewriterCreateOptionsPtr options) {
  if (options && !IsLanguagesSupported(options->expected_input_languages,
                                       options->expected_context_languages,
                                       options->output_language)) {
    mojo::Remote<blink::mojom::AIManagerCreateRewriterClient> client_remote(
        std::move(client));
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
    return;
  }
  CreateContextBoundObjectTask<AIRewriter, blink::mojom::AIRewriter,
                               blink::mojom::AIManagerCreateRewriterClient,
                               blink::mojom::AIRewriterCreateOptionsPtr>::
      CreateAndStart(
          browser_context_,
          optimization_guide::ModelBasedCapabilityKey::kWritingAssistanceApi,
          context_bound_object_set_, std::move(options), std::move(client));
}

void AIManager::CanCreateSession(
    optimization_guide::ModelBasedCapabilityKey capability,
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
    std::move(callback).Run(
        /*result=*/
        blink::mojom::ModelAvailabilityCheckResult::
            kUnavailableServiceNotRunning);
    return;
  }

  // If the `OptimizationGuideKeyedService` cannot create new session, return
  // the reason.
  auto eligibility = service->GetOnDeviceModelEligibility(capability);
  if (eligibility !=
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess) {
    std::move(callback).Run(
        ConvertOnDeviceModelEligibilityReasonToModelAvailabilityCheckResult(
            eligibility, component_observer_->is_downloading()));
    return;
  }

  std::move(callback).Run(
      /*result=*/blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

void AIManager::CreateLanguageModelForCloning(
    base::PassKey<AILanguageModel> pass_key,
    blink::mojom::AILanguageModelSamplingParamsPtr sampling_params,
    AIContextBoundObjectSet& context_bound_object_set,
    AIUtils::LanguageCodes expected_input_languages,
    const AILanguageModel::Context& context,
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote) {
  auto create_language_model_callback = base::BindOnce(
      [](AIContextBoundObjectSet& context_bound_object_set,
         mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
             client_remote,
         AILanguageModelOrCreationError creation_result) {
        if (!creation_result.has_value()) {
          client_remote->OnError(creation_result.error());
          return;
        }
        std::unique_ptr<AILanguageModel> language_model =
            std::move(creation_result.value());
        CHECK(language_model);

        client_remote->OnResult(language_model->TakePendingRemote(),
                                language_model->GetLanguageModelInstanceInfo());
        context_bound_object_set.AddContextBoundObject(
            std::move(language_model));
      },
      std::ref(context_bound_object_set), std::move(client_remote));
  // When cloning an existing language model, the `context` from the source of
  // clone should be provided.
  auto task = CreateLanguageModelInternal(
      sampling_params, context_bound_object_set,
      std::move(expected_input_languages),
      std::move(create_language_model_callback), context);
  // The on-device model must be available before the existing language model
  // was created, so the `CreateLanguageModelOnDeviceSessionTask` should
  // complete without waiting for the on-device model availability changes.
  CHECK(!task->IsPending());
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
        float(features::kAILanguageModelOverrideConfigurationMaxTemperature
                  .Get()));
  }

  return kDefaultMaxTemperature;
}

void AIManager::AddModelDownloadProgressObserver(
    mojo::PendingRemote<blink ::mojom::ModelDownloadProgressObserver>
        observer_remote) {
  download_progress_observers_.Add(std::move(observer_remote));
}

void AIManager::SendDownloadProgressUpdate(uint64_t downloaded_bytes,
                                           uint64_t total_bytes) {
  for (auto& observer : download_progress_observers_) {
    observer->OnDownloadProgressUpdate(downloaded_bytes, total_bytes);
  }
}

void AIManager::SendDownloadProgressUpdateForTesting(uint64_t downloaded_bytes,
                                                     uint64_t total_bytes) {
  SendDownloadProgressUpdate(downloaded_bytes, total_bytes);
}

void AIManager::OnTextModelDownloadProgressChange(
    base::PassKey<AIOnDeviceModelComponentObserver> observer_key,
    uint64_t downloaded_bytes,
    uint64_t total_bytes) {
  SendDownloadProgressUpdate(downloaded_bytes, total_bytes);
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_manager_keyed_service.h"

#include <memory>
#include <optional>

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
#include "base/supports_user_data.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ai/ai_assistant.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_rewriter.h"
#include "chrome/browser/ai/ai_summarizer.h"
#include "chrome/browser/ai/ai_writer.h"
#include "chrome/browser/ai/features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"

namespace {

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
        on_device_model_eligibility_reason) {
  switch (on_device_model_eligibility_reason) {
    case optimization_guide::OnDeviceModelEligibilityReason::kUnknown:
      return blink::mojom::ModelAvailabilityCheckResult::kNoUnknown;
    case optimization_guide::OnDeviceModelEligibilityReason::kFeatureNotEnabled:
      return blink::mojom::ModelAvailabilityCheckResult::kNoFeatureNotEnabled;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kConfigNotAvailableForFeature:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoConfigNotAvailableForFeature;
    case optimization_guide::OnDeviceModelEligibilityReason::kGpuBlocked:
      return blink::mojom::ModelAvailabilityCheckResult::kNoGpuBlocked;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kTooManyRecentCrashes:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoTooManyRecentCrashes;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kTooManyRecentTimeouts:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoTooManyRecentTimeouts;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kSafetyModelNotAvailable:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoSafetyModelNotAvailable;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kSafetyConfigNotAvailableForFeature:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoSafetyConfigNotAvailableForFeature;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kLanguageDetectionModelNotAvailable:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoLanguageDetectionModelNotAvailable;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kFeatureExecutionNotEnabled:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoFeatureExecutionNotEnabled;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kModelAdaptationNotAvailable:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoModelAdaptationNotAvailable;
    case optimization_guide::OnDeviceModelEligibilityReason::kModelNotEligible:
      return blink::mojom::ModelAvailabilityCheckResult::kModelNotEligible;
    case optimization_guide::OnDeviceModelEligibilityReason::kValidationPending:
      return blink::mojom::ModelAvailabilityCheckResult::kNoValidationPending;
    case optimization_guide::OnDeviceModelEligibilityReason::kValidationFailed:
      return blink::mojom::ModelAvailabilityCheckResult::kNoValidationFailed;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kInsufficientDiskSpace:
      return blink::mojom::ModelAvailabilityCheckResult::
          kNoInsufficientDiskSpace;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kModelToBeInstalled:
    case optimization_guide::OnDeviceModelEligibilityReason::
        kNoOnDeviceFeatureUsed:
      return blink::mojom::ModelAvailabilityCheckResult::kAfterDownload;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kDeprecatedModelNotAvailable:
    case optimization_guide::OnDeviceModelEligibilityReason::kSuccess:
      NOTREACHED();
  }
  NOTREACHED();
}

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
      base::SupportsUserData& context_user_data,
      CreateOptionsPtrType options,
      mojo::PendingRemote<ClientRemoteInterface> client) {
    auto task = std::make_unique<CreateContextBoundObjectTask>(
        base::PassKey<CreateContextBoundObjectTask>(), browser_context, feature,
        context_user_data, std::move(options), std::move(client));
    task->Start();
    if (task->IsPending()) {
      // Put `task` to AIContextBoundObjectSet to continue observing the model
      // availability.
      AIContextBoundObjectSet::GetFromContext(context_user_data)
          ->AddContextBoundObject(std::move(task));
    }
  }

  CreateContextBoundObjectTask(
      base::PassKey<CreateContextBoundObjectTask>,
      content::BrowserContext* browser_context,
      optimization_guide::ModelBasedCapabilityKey feature,
      base::SupportsUserData& context_user_data,
      CreateOptionsPtrType options,
      mojo::PendingRemote<ClientRemoteInterface> client)
      : CreateOnDeviceSessionTask(browser_context, feature),
        owning_user_data_(context_user_data),
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
      // TODO(crbug.com/357967382): Return an error enum and throw a clear
      // exception from the blink side.
      client_remote_->OnResult(
          mojo::PendingRemote<ContextBoundObjectReceiverInterface>());
      return;
    }
    mojo::PendingRemote<ContextBoundObjectReceiverInterface> pending_remote;
    AIContextBoundObjectSet::GetFromContext(owning_user_data_.get())
        ->AddContextBoundObject(std::make_unique<ContextBoundObjectType>(
            std::move(session), std::move(options_),
            pending_remote.InitWithNewPipeAndPassReceiver()));
    client_remote_->OnResult(std::move(pending_remote));
  }

 private:
  // If this came from RenderFrameHostImpl this will be the
  // document_associate_data. If it's a worker, it will be the worker itself.
  // When the RFHI's document changes or the worker is destroyed, it will cause
  // `this` to be destroyed also, so it's safe to rely on this reference.
  const raw_ref<base::SupportsUserData> owning_user_data_;
  CreateOptionsPtrType options_;
  mojo::Remote<ClientRemoteInterface> client_remote_;
};

// The class is responsible for removing the receivers from the
// `AIManagerKeyedService` when the corresponding receiver contexts are
// destroyed.
// TODO(crbug.com/367755363): To further improve this flow, we should implement
// the factory interface per context, and they talk to the keyed service for
// optimization guide integration. In this case, we don't have to maintain the
// `ReceiverContext` any more.
class AIManagerReceiverRemover : public AIContextBoundObject {
 public:
  explicit AIManagerReceiverRemover(base::OnceClosure remove_callback)
      : remove_callback_(std::move(remove_callback)) {}
  ~AIManagerReceiverRemover() override { std::move(remove_callback_).Run(); }

  // `AIContextBoundObject` implementation.
  // Unlike the other implementation of `AIContextBoundObject`, the remover is
  // not a mojo interface implementation and the only case it should run the
  // deletion callback is when the object itself is deleted.
  void SetDeletionCallback(base::OnceClosure deletion_callback) override {}

 private:
  base::OnceClosure remove_callback_;
};

}  // namespace

AIManagerKeyedService::AIManagerKeyedService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      component_observer_(
          std::make_unique<AIOnDeviceModelComponentObserver>(this)) {}

AIManagerKeyedService::~AIManagerKeyedService() = default;

void AIManagerKeyedService::AddReceiver(
    mojo::PendingReceiver<blink::mojom::AIManager> receiver,
    base::SupportsUserData& context_user_data) {
  mojo::ReceiverId receiver_id =
      receivers_.Add(this, std::move(receiver), &context_user_data);
  AIContextBoundObjectSet* context_bound_object_set =
      AIContextBoundObjectSet::GetFromContext(context_user_data);
  context_bound_object_set->AddContextBoundObject(
      std::make_unique<AIManagerReceiverRemover>(
          base::BindOnce(&AIManagerKeyedService::RemoveReceiver,
                         weak_factory_.GetWeakPtr(), receiver_id)));
}

void AIManagerKeyedService::CanCreateAssistant(
    CanCreateAssistantCallback callback) {
  CanCreateSession(optimization_guide::ModelBasedCapabilityKey::kPromptApi,
                   std::move(callback));
}

std::unique_ptr<CreateAssistantOnDeviceSessionTask>
AIManagerKeyedService::CreateAssistantInternal(
    const blink::mojom::AIAssistantSamplingParamsPtr& sampling_params,
    AIContextBoundObjectSet& context_bound_object_set,
    base::OnceCallback<void(std::unique_ptr<AIAssistant>)> callback,
    const std::optional<const AIAssistant::Context>& context,
    base::SupportsUserData* context_user_data) {
  CHECK(browser_context_);
  auto task = std::make_unique<CreateAssistantOnDeviceSessionTask>(
      browser_context_.get(), sampling_params,
      base::BindOnce(
          [](base::WeakPtr<content::BrowserContext> browser_context,
             AIContextBoundObjectSet& context_bound_object_set,
             const std::optional<const AIAssistant::Context>& context,
             base::OnceCallback<void(std::unique_ptr<AIAssistant>)> callback,
             std::unique_ptr<
                 optimization_guide::OptimizationGuideModelExecutor::Session>
                 session) {
            if (!session) {
              std::move(callback).Run(nullptr);
              return;
            }

            mojo::PendingRemote<blink::mojom::AIAssistant> pending_remote;
            std::move(callback).Run(std::make_unique<AIAssistant>(
                std::move(session), browser_context, std::move(pending_remote),
                context_bound_object_set, context));
          },
          browser_context_->GetWeakPtr(), std::ref(context_bound_object_set),
          context, std::move(callback)));
  task->Start();
  return task;
}

void AIManagerKeyedService::CreateAssistant(
    mojo::PendingRemote<blink::mojom::AIManagerCreateAssistantClient> client,
    blink::mojom::AIAssistantCreateOptionsPtr options) {
  blink::mojom::AIAssistantSamplingParamsPtr sampling_params =
      std::move(options->sampling_params);

  // Since this is a mojo IPC implementation, the context should be
  // non-null;
  base::SupportsUserData* context_user_data = receivers_.current_context();
  CHECK(context_user_data);
  AIContextBoundObjectSet* context_bound_object_set =
      AIContextBoundObjectSet::GetFromContext(*context_user_data);
  CHECK(context_bound_object_set);

  auto create_assistant_callback = base::BindOnce(
      [](mojo::PendingRemote<blink::mojom::AIManagerCreateAssistantClient>
             client,
         AIContextBoundObjectSet& context_bound_object_set,
         blink::mojom::AIAssistantCreateOptionsPtr options,
         std::unique_ptr<AIAssistant> assistant) {
        mojo::Remote<blink::mojom::AIManagerCreateAssistantClient>
            client_remote(std::move(client));
        if (!assistant) {
          // TODO(crbug.com/343325183): probably we should consider
          // returning an error enum and throw a clear exception from
          // the blink side.
          client_remote->OnResult(
              mojo::PendingRemote<blink::mojom::AIAssistant>(),
              /*info=*/nullptr);
          return;
        }

        const std::optional<std::string>& system_prompt =
            options->system_prompt;
        std::vector<blink::mojom::AIAssistantInitialPromptPtr>&
            initial_prompts = options->initial_prompts;
        if (system_prompt.has_value() || !initial_prompts.empty()) {
          // If the initial prompt is provided, we need to set it and
          // invoke the callback after this, because the token counting
          // happens asynchronously.
          assistant->SetInitialPrompts(
              system_prompt, std::move(initial_prompts),
              base::BindOnce(
                  [](mojo::Remote<blink::mojom::AIManagerCreateAssistantClient>
                         client_remote,
                     mojo::PendingRemote<blink::mojom::AIAssistant> remote,
                     blink::mojom::AIAssistantInfoPtr info) {
                    client_remote->OnResult(std::move(remote), std::move(info));
                  },
                  std::move(client_remote)));
        } else {
          client_remote->OnResult(assistant->TakePendingRemote(),
                                  assistant->GetAssistantInfo());
        }

        context_bound_object_set.AddContextBoundObject(std::move(assistant));
      },
      std::move(client), std::ref(*context_bound_object_set),
      std::move(options));

  // When creating a new assistant, the `context` will not be set since it
  // should start fresh.
  auto task = CreateAssistantInternal(sampling_params, *context_bound_object_set,
                                      std::move(create_assistant_callback));
  if (task->IsPending()) {
    // Put `task` to AIContextBoundObjectSet to continue observing the model
    // availability.
    AIContextBoundObjectSet::GetFromContext(*context_user_data)
        ->AddContextBoundObject(std::move(task));
  }
}

void AIManagerKeyedService::CanCreateSummarizer(
    CanCreateSummarizerCallback callback) {
  CanCreateSession(optimization_guide::ModelBasedCapabilityKey::kSummarize,
                   std::move(callback));
}

void AIManagerKeyedService::CreateSummarizer(
    mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
    blink::mojom::AISummarizerCreateOptionsPtr options) {
  CreateContextBoundObjectTask<AISummarizer, blink::mojom::AISummarizer,
                               blink::mojom::AIManagerCreateSummarizerClient,
                               blink::mojom::AISummarizerCreateOptionsPtr>::
      CreateAndStart(browser_context_,
                     optimization_guide::ModelBasedCapabilityKey::kSummarize,
                     *receivers_.current_context(), std::move(options),
                     std::move(client));
}

void AIManagerKeyedService::GetModelInfo(GetModelInfoCallback callback) {
  auto default_sampling_params = GetAssistantDefaultSamplingParams();
  std::move(callback).Run(blink::mojom::AIModelInfo::New(
      default_sampling_params.top_k, GetAssistantModelMaxTopK(),
      default_sampling_params.temperature));
}

void AIManagerKeyedService::CreateWriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
    blink::mojom::AIWriterCreateOptionsPtr options) {
  CreateContextBoundObjectTask<AIWriter, blink::mojom::AIWriter,
                               blink::mojom::AIManagerCreateWriterClient,
                               blink::mojom::AIWriterCreateOptionsPtr>::
      CreateAndStart(browser_context_,
                     optimization_guide::ModelBasedCapabilityKey::kCompose,
                     *receivers_.current_context(), std::move(options),
                     std::move(client));
}

void AIManagerKeyedService::CreateRewriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
    blink::mojom::AIRewriterCreateOptionsPtr options) {
  if (options->tone != blink::mojom::AIRewriterTone::kAsIs &&
      options->length != blink::mojom::AIRewriterLength::kAsIs) {
    // TODO(crbug.com/358214322): Currently the combination of the tone and the
    // length option is not supported.
    // TODO(crbug.com/358214322): Return an error enum and throw a clear
    // exception from the blink side.
    mojo::Remote<blink::mojom::AIManagerCreateRewriterClient> client_remote(
        std::move(client));
    client_remote->OnResult(mojo::PendingRemote<blink::mojom::AIRewriter>());
    return;
  }
  CreateContextBoundObjectTask<AIRewriter, blink::mojom::AIRewriter,
                               blink::mojom::AIManagerCreateRewriterClient,
                               blink::mojom::AIRewriterCreateOptionsPtr>::
      CreateAndStart(browser_context_,
                     optimization_guide::ModelBasedCapabilityKey::kCompose,
                     *receivers_.current_context(), std::move(options),
                     std::move(client));
}

void AIManagerKeyedService::CanCreateSession(
    optimization_guide::ModelBasedCapabilityKey capability,
    CanCreateAssistantCallback callback) {
  auto model_path =
      optimization_guide::switches::GetOnDeviceModelExecutionOverride();
  if (model_path.has_value()) {
    // If the model path is provided, we do this additional check and post a
    // warning message to dev tools if it's invalid.
    // This needs to be done in a task runner with `MayBlock` trait.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(IsModelPathValid, model_path.value()),
        base::BindOnce(&AIManagerKeyedService::OnModelPathValidationComplete,
                       weak_factory_.GetWeakPtr(), model_path.value()));
  }

  // Check if the optimization guide service can create session.
  CHECK(browser_context_);
  OptimizationGuideKeyedService* service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_));

  // If the `OptimizationGuideKeyedService` cannot be retrieved, return false.
  if (!service) {
    std::move(callback).Run(
        /*result=*/
        blink::mojom::ModelAvailabilityCheckResult::kNoServiceNotRunning);
    return;
  }

  // If the `OptimizationGuideKeyedService` cannot create new session, return
  // false.
  optimization_guide::OnDeviceModelEligibilityReason
      on_device_model_eligibility_reason;
  if (!service->CanCreateOnDeviceSession(capability,
                                         &on_device_model_eligibility_reason)) {
    std::move(callback).Run(
        /*result=*/
        ConvertOnDeviceModelEligibilityReasonToModelAvailabilityCheckResult(
            on_device_model_eligibility_reason));
    return;
  }

  std::move(callback).Run(
      /*result=*/blink::mojom::ModelAvailabilityCheckResult::kReadily);
}

void AIManagerKeyedService::CreateAssistantForCloning(
    base::PassKey<AIAssistant> pass_key,
    blink::mojom::AIAssistantSamplingParamsPtr sampling_params,
    AIContextBoundObjectSet& context_bound_object_set,
    const AIAssistant::Context& context,
    mojo::Remote<blink::mojom::AIManagerCreateAssistantClient> client_remote) {
  auto create_assistant_callback = base::BindOnce(
      [](AIContextBoundObjectSet& context_bound_object_set,
         mojo::Remote<blink::mojom::AIManagerCreateAssistantClient>
             client_remote,
         std::unique_ptr<AIAssistant> assistant) {
        if (!assistant) {
          client_remote->OnResult(
              mojo::PendingRemote<blink::mojom::AIAssistant>(),
              /*info=*/nullptr);
          return;
        }

        client_remote->OnResult(assistant->TakePendingRemote(),
                                assistant->GetAssistantInfo());
        context_bound_object_set.AddContextBoundObject(std::move(assistant));
      },
      std::ref(context_bound_object_set), std::move(client_remote));
  // When cloning an existing assistant, the `context` from the source of clone
  // should be provided.
  auto task =
      CreateAssistantInternal(sampling_params, context_bound_object_set,
                              std::move(create_assistant_callback), context,
                              /*context_user_data=*/nullptr);
  // The on-device model must be available before the existing assistant was
  // created, so the `CreateAssistantOnDeviceSessionTask` should complete
  // without waiting for the on-device model availability changes.
  CHECK(!task->IsPending());
}

void AIManagerKeyedService::OnModelPathValidationComplete(
    const std::string& model_path,
    bool is_valid_path) {
  // TODO(crbug.com/346491542): Remove this when the error page is implemented.
  if (!is_valid_path) {
    VLOG(1) << base::StringPrintf(
        "Unable to create a session because the model path ('%s') is invalid.",
        model_path.c_str());
  }
}

void AIManagerKeyedService::RemoveReceiver(mojo::ReceiverId receiver_id) {
  receivers_.Remove(receiver_id);
}

optimization_guide::SamplingParams
AIManagerKeyedService::GetAssistantDefaultSamplingParams() {
  if (default_assistant_sampling_params_.has_value()) {
    return default_assistant_sampling_params_.value();
  }

  // Create a `kPromptApi` session without specifying the config params. The
  // session should be created using the default value from the model execution
  // config.
  // TODO(crbug.com/372349624): implement a way to fetch the default params
  // without creating a dummy session.
  OptimizationGuideKeyedService* service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_));
  using optimization_guide::SessionConfigParams;
  SessionConfigParams config_params = SessionConfigParams{
      .execution_mode = SessionConfigParams::ExecutionMode::kOnDeviceOnly,
      .logging_mode = SessionConfigParams::LoggingMode::kAlwaysDisable,
  };
  auto session = service->StartSession(
      optimization_guide::ModelBasedCapabilityKey::kPromptApi, config_params);

  default_assistant_sampling_params_ =
      session
          ? session->GetSamplingParams()
          : optimization_guide::SamplingParams{
                uint32_t(
                    optimization_guide::features::GetOnDeviceModelMaxTopK()),
                float(optimization_guide::features::
                          GetOnDeviceModelDefaultTemperature())};
  return default_assistant_sampling_params_.value();
}

uint32_t AIManagerKeyedService::GetAssistantModelMaxTopK() {
  int max_top_k = optimization_guide::features::GetOnDeviceModelMaxTopK();
  if (base::FeatureList::IsEnabled(
          features::kAIAssistantOverrideConfiguration)) {
    max_top_k = std::min(
        max_top_k, features::kAIAssistantOverrideConfigurationMaxTopK.Get());
  }
  return max_top_k;
}

void AIManagerKeyedService::AddModelDownloadProgressObserver(
    mojo::PendingRemote<blink ::mojom::ModelDownloadProgressObserver>
        observer_remote) {
  download_progress_observers_.Add(std::move(observer_remote));
}

void AIManagerKeyedService::SendDownloadProgressUpdate(
    uint64_t downloaded_bytes,
    uint64_t total_bytes) {
  for (auto& observer : download_progress_observers_) {
    observer->OnDownloadProgressUpdate(downloaded_bytes, total_bytes);
  }
}

void AIManagerKeyedService::SendDownloadProgressUpdateForTesting(
    uint64_t downloaded_bytes,
    uint64_t total_bytes) {
  SendDownloadProgressUpdate(downloaded_bytes, total_bytes);
}

void AIManagerKeyedService::OnTextModelDownloadProgressChange(
    base::PassKey<AIOnDeviceModelComponentObserver> observer_key,
    uint64_t downloaded_bytes,
    uint64_t total_bytes) {
  SendDownloadProgressUpdate(downloaded_bytes, total_bytes);
}

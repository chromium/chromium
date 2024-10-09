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
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
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
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
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

// Return the max top k value for the Assistant API. Note that this value won't
// exceed the max top k defined by the underlying on-device model.
int GetAssistantModelMaxTopK() {
  int max_top_k = optimization_guide::features::GetOnDeviceModelMaxTopK();
  if (base::FeatureList::IsEnabled(
          features::kAIAssistantOverrideConfiguration)) {
    max_top_k = std::min(
        max_top_k, features::kAIAssistantOverrideConfigurationMaxTopK.Get());
  }
  return max_top_k;
}

double GetAssistantModelDefaultTemperature() {
  if (base::FeatureList::IsEnabled(
          features::kAIAssistantOverrideConfiguration)) {
    return features::kAIAssistantOverrideConfigurationDefaultTemperature.Get();
  }
  return optimization_guide::features::GetOnDeviceModelDefaultTemperature();
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
    case optimization_guide::OnDeviceModelEligibilityReason::kModelNotAvailable:
      return blink::mojom::ModelAvailabilityCheckResult::kNoModelNotAvailable;
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
    case optimization_guide::OnDeviceModelEligibilityReason::kValidationPending:
      return blink::mojom::ModelAvailabilityCheckResult::kNoValidationPending;
    case optimization_guide::OnDeviceModelEligibilityReason::kValidationFailed:
      return blink::mojom::ModelAvailabilityCheckResult::kNoValidationFailed;
    case optimization_guide::OnDeviceModelEligibilityReason::
        kModelToBeInstalled:
      return blink::mojom::ModelAvailabilityCheckResult::kAfterDownload;
    case optimization_guide::OnDeviceModelEligibilityReason::kSuccess:
      NOTREACHED();
  }
  NOTREACHED();
}

// Currently, the following errors, which are used when a model may have been
// installed but not yet loaded, are treated as waitable.
static constexpr auto kWaitableReasons =
    base::MakeFixedFlatSet<optimization_guide::OnDeviceModelEligibilityReason>({
        optimization_guide::OnDeviceModelEligibilityReason::
            kConfigNotAvailableForFeature,
        optimization_guide::OnDeviceModelEligibilityReason::
            kSafetyModelNotAvailable,
        optimization_guide::OnDeviceModelEligibilityReason::
            kLanguageDetectionModelNotAvailable,
        optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled,
    });

// A base class for tasks which create an on-device session. See the method
// comment of `Run()` for the details.
class CreateOnDeviceSessionTask
    : public AIContextBoundObject,
      public optimization_guide::OnDeviceModelAvailabilityObserver {
 public:
  CreateOnDeviceSessionTask(content::BrowserContext& browser_context,
                            optimization_guide::ModelBasedCapabilityKey feature)
      : service_(OptimizationGuideKeyedServiceFactory::GetForProfile(
            Profile::FromBrowserContext(&browser_context))),
        feature_(feature) {}
  ~CreateOnDeviceSessionTask() override {
    if (observing_availability_) {
      service_->RemoveOnDeviceModelAvailabilityChangeObserver(feature_, this);
    }
  }
  CreateOnDeviceSessionTask(const CreateOnDeviceSessionTask&) = delete;
  CreateOnDeviceSessionTask& operator=(const CreateOnDeviceSessionTask&) =
      delete;

  bool observing_availability() const { return observing_availability_; }

  // Attempts to create an on-device session.
  //
  // * If `service_` is null, immediately calls `OnFinish()` with a nullptr,
  //   indicating failure.
  // * If creation succeeds, calls `OnFinish()` with the newly created session.
  // * If creation fails:
  //   * If the failure reason is in `kWaitableReasons` (indicating a
  //     potentially temporary issue):
  //     * Registers itself to observe model availability changes in `service_`.
  //     * Waits until the `reason` is no longer in `kWaitableReasons`, then
  //       retries session creation.
  //   * Otherwise (for non-recoverable errors), calls `OnFinish()` with a
  //     nullptr.
  void Run() {
    if (!service_) {
      OnFinish(nullptr);
      return;
    }
    if (auto session = StartSession()) {
      OnFinish(std::move(session));
      return;
    }
    optimization_guide::OnDeviceModelEligibilityReason reason;
    bool can_create = service_->CanCreateOnDeviceSession(feature_, &reason);
    CHECK(!can_create);
    if (!kWaitableReasons.contains(reason)) {
      OnFinish(nullptr);
      return;
    }
    observing_availability_ = true;
    service_->AddOnDeviceModelAvailabilityChangeObserver(feature_, this);
  }

 protected:
  // Cancels the creation task, and deletes itself.
  void Cancel() {
    CHECK(observing_availability_);
    CHECK(deletion_callback_);
    std::move(deletion_callback_).Run();
  }

  virtual void OnFinish(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session>
          session) = 0;

  virtual void UpdateSessionConfigParams(
      optimization_guide::SessionConfigParams* config_params) {}

 private:
  // `AIContextBoundObject` implementation.
  void SetDeletionCallback(base::OnceClosure deletion_callback) override {
    deletion_callback_ = std::move(deletion_callback);
  }

  // optimization_guide::OnDeviceModelAvailabilityObserver
  void OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelEligibilityReason reason) override {
    if (kWaitableReasons.contains(reason)) {
      return;
    }
    OnFinish(StartSession());
    std::move(deletion_callback_).Run();
  }

  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
  StartSession() {
    optimization_guide::SessionConfigParams config_params =
        optimization_guide::SessionConfigParams{
            .execution_mode = optimization_guide::SessionConfigParams::
                ExecutionMode::kOnDeviceOnly};
    UpdateSessionConfigParams(&config_params);
    return service_->StartSession(feature_, config_params);
  }

  const raw_ptr<OptimizationGuideKeyedService> service_;
  const optimization_guide::ModelBasedCapabilityKey feature_;
  bool observing_availability_ = false;
  base::OnceClosure deletion_callback_;
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
  static void Start(content::BrowserContext& browser_context,
                    optimization_guide::ModelBasedCapabilityKey feature,
                    AIContextBoundObjectSet::ReceiverContext context,
                    CreateOptionsPtrType options,
                    mojo::PendingRemote<ClientRemoteInterface> client) {
    auto task = std::make_unique<CreateContextBoundObjectTask>(
        base::PassKey<CreateContextBoundObjectTask>(), browser_context, feature,
        context, std::move(options), std::move(client));
    task->Run();
    if (task->observing_availability()) {
      // Put `task` to AIContextBoundObjectSet to continue observing the model
      // availability.
      AIContextBoundObjectSet::GetFromContext(context)->AddContextBoundObject(
          std::move(task));
    }
  }

  CreateContextBoundObjectTask(
      base::PassKey<CreateContextBoundObjectTask>,
      content::BrowserContext& browser_context,
      optimization_guide::ModelBasedCapabilityKey feature,
      AIContextBoundObjectSet::ReceiverContext context,
      CreateOptionsPtrType options,
      mojo::PendingRemote<ClientRemoteInterface> client)
      : CreateOnDeviceSessionTask(browser_context, feature),
        context_(AIContextBoundObjectSet::ToReceiverContextRawRef(context)),
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
    AIContextBoundObjectSet::GetFromContext(
        AIContextBoundObjectSet::ToReceiverContext(context_))
        ->AddContextBoundObject(std::make_unique<ContextBoundObjectType>(
            std::move(session), std::move(options_),
            pending_remote.InitWithNewPipeAndPassReceiver()));
    client_remote_->OnResult(std::move(pending_remote));
  }

 private:
  const AIContextBoundObjectSet::ReceiverContextRawRef context_;
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

// Implementation of the `CreateOnDeviceSessionTask` base class for AIAssistant.
class CreateAssistantOnDeviceSessionTask : public CreateOnDeviceSessionTask {
 public:
  CreateAssistantOnDeviceSessionTask(
      content::BrowserContext& browser_context,
      const blink::mojom::AIAssistantSamplingParamsPtr& sampling_params,
      base::OnceCallback<
          void(std::unique_ptr<
               optimization_guide::OptimizationGuideModelExecutor::Session>)>
          completion_callback)
      : CreateOnDeviceSessionTask(
            browser_context,
            optimization_guide::ModelBasedCapabilityKey::kPromptApi),
        completion_callback_(std::move(completion_callback)) {
    if (sampling_params) {
      sampling_params_ = optimization_guide::SamplingParams{
          .top_k = std::min(sampling_params->top_k,
                            uint32_t(GetAssistantModelMaxTopK())),
          .temperature = sampling_params->temperature};
    } else {
      sampling_params_ = optimization_guide::SamplingParams{
          .top_k = uint32_t(
              optimization_guide::features::GetOnDeviceModelDefaultTopK()),
          .temperature = float(GetAssistantModelDefaultTemperature())};
    }
  }
  ~CreateAssistantOnDeviceSessionTask() override = default;

  CreateAssistantOnDeviceSessionTask(
      const CreateAssistantOnDeviceSessionTask&) = delete;
  CreateAssistantOnDeviceSessionTask& operator=(
      const CreateAssistantOnDeviceSessionTask&) = delete;

 protected:
  // `CreateOnDeviceSessionTask` implementation.
  void OnFinish(std::unique_ptr<
                optimization_guide::OptimizationGuideModelExecutor::Session>
                    session) override {
    std::move(completion_callback_).Run(std::move(session));
  }

  void UpdateSessionConfigParams(
      optimization_guide::SessionConfigParams* config_params) override {
    config_params->sampling_params = sampling_params_;
  }

 private:
  std::optional<optimization_guide::SamplingParams> sampling_params_ =
      std::nullopt;
  base::OnceCallback<void(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session>)>
      completion_callback_;
};

}  // namespace

AIManagerKeyedService::AIManagerKeyedService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

AIManagerKeyedService::~AIManagerKeyedService() = default;

void AIManagerKeyedService::AddReceiver(
    mojo::PendingReceiver<blink::mojom::AIManager> receiver,
    AIContextBoundObjectSet::ReceiverContext context) {
  mojo::ReceiverId receiver_id =
      receivers_.Add(this, std::move(receiver), context);
  AIContextBoundObjectSet* context_bound_object_set =
      AIContextBoundObjectSet::GetFromContext(context);
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

void AIManagerKeyedService::CreateAssistantInternal(
    const blink::mojom::AIAssistantSamplingParamsPtr& sampling_params,
    AIContextBoundObjectSet* context_bound_object_set,
    base::OnceCallback<void(std::unique_ptr<AIAssistant>)> callback,
    const std::optional<const AIAssistant::Context>& context) {
  CHECK(browser_context_);
  auto task = std::make_unique<CreateAssistantOnDeviceSessionTask>(
      *browser_context_.get(), sampling_params,
      base::BindOnce(
          [](base::WeakPtr<content::BrowserContext> browser_context,
             AIContextBoundObjectSet* context_bound_object_set,
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
          browser_context_->GetWeakPtr(), context_bound_object_set, context,
          std::move(callback)));
  task->Run();
  if (task->observing_availability()) {
    // Put `task` to AIContextBoundObjectSet to continue observing the model
    // availability.
    AIContextBoundObjectSet::GetFromContext(browser_context_)
        ->AddContextBoundObject(std::move(task));
  }
}

void AIManagerKeyedService::CreateAssistant(
    mojo::PendingRemote<blink::mojom::AIManagerCreateAssistantClient> client,
    blink::mojom::AIAssistantCreateOptionsPtr options) {
  blink::mojom::AIAssistantSamplingParamsPtr sampling_params =
      std::move(options->sampling_params);

  // Since this is a mojo IPC implementation, the context should be
  // non-null;
  AIContextBoundObjectSet* context_bound_object_set =
      AIContextBoundObjectSet::GetFromContext(receivers_.current_context());

  CreateAssistantInternal(
      sampling_params, context_bound_object_set,
      base::BindOnce(
          [](mojo::PendingRemote<blink::mojom::AIManagerCreateAssistantClient>
                 client,
             AIContextBoundObjectSet* context_bound_object_set,
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
                      [](mojo::Remote<
                             blink::mojom::AIManagerCreateAssistantClient>
                             client_remote,
                         mojo::PendingRemote<blink::mojom::AIAssistant> remote,
                         blink::mojom::AIAssistantInfoPtr info) {
                        client_remote->OnResult(std::move(remote),
                                                std::move(info));
                      },
                      std::move(client_remote)));
            } else {
              client_remote->OnResult(assistant->TakePendingRemote(),
                                      assistant->GetAssistantInfo());
            }

            context_bound_object_set->AddContextBoundObject(
                std::move(assistant));
          },
          std::move(client), context_bound_object_set, std::move(options)));
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
      Start(*browser_context_,
            optimization_guide::ModelBasedCapabilityKey::kSummarize,
            receivers_.current_context(), std::move(options),
            std::move(client));
}

void AIManagerKeyedService::GetModelInfo(GetModelInfoCallback callback) {
  std::move(callback).Run(blink::mojom::AIModelInfo::New(
      optimization_guide::features::GetOnDeviceModelDefaultTopK(),
      GetAssistantModelMaxTopK(), GetAssistantModelDefaultTemperature()));
}

void AIManagerKeyedService::CreateWriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
    blink::mojom::AIWriterCreateOptionsPtr options) {
  CreateContextBoundObjectTask<AIWriter, blink::mojom::AIWriter,
                               blink::mojom::AIManagerCreateWriterClient,
                               blink::mojom::AIWriterCreateOptionsPtr>::
      Start(*browser_context_,
            optimization_guide::ModelBasedCapabilityKey::kCompose,
            receivers_.current_context(), std::move(options),
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
      Start(*browser_context_,
            optimization_guide::ModelBasedCapabilityKey::kCompose,
            receivers_.current_context(), std::move(options),
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
    AIContextBoundObjectSet* context_bound_object_set,
    const AIAssistant::Context& context,
    mojo::Remote<blink::mojom::AIManagerCreateAssistantClient> client_remote) {
  CreateAssistantInternal(
      sampling_params, context_bound_object_set,
      base::BindOnce(
          [](AIContextBoundObjectSet* context_bound_object_set,
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
            context_bound_object_set->AddContextBoundObject(
                std::move(assistant));
          },
          context_bound_object_set, std::move(client_remote)),
      context);
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

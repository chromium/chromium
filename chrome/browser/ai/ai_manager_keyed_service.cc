// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_manager_keyed_service.h"

#include <memory>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
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
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_rewriter.h"
#include "chrome/browser/ai/ai_summarizer.h"
#include "chrome/browser/ai/ai_text_session.h"
#include "chrome/browser/ai/ai_writer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom.h"
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
// TODO(crbug.com/361537114): Consider making the kModelToBeInstalled error
// waitable as well.
static constexpr auto kWaitableReasons =
    base::MakeFixedFlatSet<optimization_guide::OnDeviceModelEligibilityReason>({
        optimization_guide::OnDeviceModelEligibilityReason::
            kConfigNotAvailableForFeature,
        optimization_guide::OnDeviceModelEligibilityReason::
            kSafetyModelNotAvailable,
        optimization_guide::OnDeviceModelEligibilityReason::
            kLanguageDetectionModelNotAvailable,
    });

// A base class for tasks which create an on-device session. See the method
// comment of `Run()` for the details.
class CreateOnDeviceSessionTask
    : public AIContextBoundObject,
      public optimization_guide::OnDeviceModelAvailabilityObserver {
 public:
  explicit CreateOnDeviceSessionTask(
      content::BrowserContext& browser_context,
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

 protected:
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

void AIManagerKeyedService::CanCreateTextSession(
    CanCreateTextSessionCallback callback) {
  auto model_path =
      optimization_guide::switches::GetOnDeviceModelExecutionOverride();
  if (model_path) {
    CheckModelPathOverrideCanCreateSession(
        model_path.value(),
        optimization_guide::ModelBasedCapabilityKey::kPromptApi);
  }
  // If the model path is not provided, we skip the model path check.
  CanOptimizationGuideKeyedServiceCreateGenericSession(
      optimization_guide::ModelBasedCapabilityKey::kPromptApi,
      std::move(callback));
}

std::unique_ptr<AITextSession> AIManagerKeyedService::CreateTextSessionInternal(
    mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
    const blink::mojom::AITextSessionSamplingParamsPtr& sampling_params,
    AIContextBoundObjectSet* context_bound_object_set,
    const std::optional<const AITextSession::Context>& context) {
  CHECK(browser_context_);
  OptimizationGuideKeyedService* service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_.get()));
  if (!service) {
    return nullptr;
  }

  optimization_guide::SessionConfigParams config_params =
      optimization_guide::SessionConfigParams{
          .execution_mode = optimization_guide::SessionConfigParams::
              ExecutionMode::kOnDeviceOnly};
  if (sampling_params) {
    config_params.sampling_params = optimization_guide::SamplingParams{
        .top_k = sampling_params->top_k,
        .temperature = sampling_params->temperature};
  }

  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session = service->StartSession(
          optimization_guide::ModelBasedCapabilityKey::kPromptApi,
          config_params);
  if (!session) {
    return nullptr;
  }

  return std::make_unique<AITextSession>(
      std::move(session), browser_context_->GetWeakPtr(), std::move(receiver),
      context_bound_object_set, context);
}

void AIManagerKeyedService::CreateTextSession(
    mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
    blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
    const std::optional<std::string>& system_prompt,
    std::vector<blink::mojom::AIAssistantInitialPromptPtr> initial_prompts,
    CreateTextSessionCallback callback) {
  // Since this is a mojo IPC implementation, the context should be non-null;
  AIContextBoundObjectSet* context_bound_object_set =
      AIContextBoundObjectSet::GetFromContext(receivers_.current_context());
  std::unique_ptr<AITextSession> session = CreateTextSessionInternal(
      std::move(receiver), sampling_params, context_bound_object_set);
  if (!session) {
    // TODO(crbug.com/343325183): probably we should consider returning an error
    // enum and throw a clear exception from the blink side.
    std::move(callback).Run(nullptr);
    return;
  }

  if (system_prompt.has_value() || !initial_prompts.empty()) {
    // If the initial prompt is provided, we need to set it and invoke the
    // callback after this, because the token counting happens asynchronously.
    session->SetInitialPrompts(system_prompt, std::move(initial_prompts),
                               std::move(callback));
  } else {
    std::move(callback).Run(session->GetTextSessionInfo());
  }

  context_bound_object_set->AddContextBoundObject(std::move(session));
}

void AIManagerKeyedService::CanCreateSummarizer(
    CanCreateSummarizerCallback callback) {
  auto model_path =
      optimization_guide::switches::GetOnDeviceModelExecutionOverride();
  if (model_path) {
    CheckModelPathOverrideCanCreateSession(
        model_path.value(),
        optimization_guide::ModelBasedCapabilityKey::kSummarize);
  }
  // If the model path is not provided, we skip the model path check.
  CanOptimizationGuideKeyedServiceCreateGenericSession(
      optimization_guide::ModelBasedCapabilityKey::kSummarize,
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

void AIManagerKeyedService::GetTextModelInfo(
    GetTextModelInfoCallback callback) {
  std::move(callback).Run(blink::mojom::AITextModelInfo::New(
      optimization_guide::features::GetOnDeviceModelDefaultTopK(),
      optimization_guide::features::GetOnDeviceModelMaxTopK(),
      optimization_guide::features::GetOnDeviceModelDefaultTemperature()));
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

void AIManagerKeyedService::CheckModelPathOverrideCanCreateSession(
    const std::string& model_path,
    optimization_guide::ModelBasedCapabilityKey capability_) {
  // If the model path is provided, we do this additional check and post a
  // warning message to dev tools if it's invalid.
  // This needs to be done in a task runner with `MayBlock` trait.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(IsModelPathValid, model_path),
      base::BindOnce(&AIManagerKeyedService::OnModelPathValidationComplete,
                     weak_factory_.GetWeakPtr(), model_path));
}

void AIManagerKeyedService::
    CanOptimizationGuideKeyedServiceCreateGenericSession(
        optimization_guide::ModelBasedCapabilityKey capability,
        CanCreateTextSessionCallback callback) {
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

void AIManagerKeyedService::CreateTextSessionForCloning(
    base::PassKey<AITextSession> pass_key,
    mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
    blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
    AIContextBoundObjectSet* context_bound_object_set,
    const AITextSession::Context& context,
    CreateTextSessionCallback callback) {
  std::unique_ptr<AITextSession> session = CreateTextSessionInternal(
      std::move(receiver), sampling_params, context_bound_object_set, context);
  if (!session) {
    std::move(callback).Run(nullptr);
    return;
  }

  blink::mojom::AITextSessionInfoPtr session_info =
      session->GetTextSessionInfo();
  context_bound_object_set->AddContextBoundObject(std::move(session));
  std::move(callback).Run(std::move(session_info));
}

void AIManagerKeyedService::OnModelPathValidationComplete(
    const std::string& model_path,
    bool is_valid_path) {
  // TODO(crbug.com/346491542): Remove this when the error page is implemented.
  if (!is_valid_path) {
    VLOG(1) << base::StringPrintf(
        "Unable to create a text session because the model path ('%s') is "
        "invalid.",
        model_path.c_str());
  }
}

void AIManagerKeyedService::RemoveReceiver(mojo::ReceiverId receiver_id) {
  receivers_.Remove(receiver_id);
}

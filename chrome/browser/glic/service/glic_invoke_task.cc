// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_task.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "chrome/browser/glic/public/glic_context_menu_invocation_helper.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace glic {

namespace {

// Based on URLToImageMarkup from clipboard_utilities.cc.
std::u16string GetImageMarkup(const GURL& src_url,
                              content::RenderFrameHost* rfh) {
  if (!src_url.is_valid()) {
    return u"";
  }
  std::u16string alt = u"";
  auto* contents = content::WebContents::FromRenderFrameHost(rfh);
  if (contents) {
    std::u16string title = base::EscapeForHTML(contents->GetTitle());
    if (!title.empty()) {
      alt = base::StrCat({u" alt=\"", title, u"\""});
    }
  }
  std::u16string spec = base::EscapeForHTML(base::UTF8ToUTF16(src_url.spec()));
  return base::StrCat({u"<img src=\"", spec, u"\"", alt, u"></img>"});
}

ui::ClipboardMetadata CreateClipboardMetadata(
    ui::ClipboardFormatType format_type, size_t size, bool is_drag_and_drop) {
  ui::ClipboardMetadata metadata;
  metadata.format_type = format_type;
  metadata.size = size;
  metadata.is_drag_and_drop = is_drag_and_drop;
  return metadata;
}

void ExtractThumbnailData(const GlicInvokeOptions& options,
                          std::vector<uint8_t>& thumbnail_data) {
  if (options.additional_context && options.additional_context->context) {
    for (const auto& part : options.additional_context->context->parts) {
      if (part->is_data() && part->get_data()->mime_type == "image/png") {
        const auto& buffer = part->get_data()->data;
        thumbnail_data = std::vector<uint8_t>(buffer.begin(), buffer.end());
        break;
      }
    }
  }
}

void ExtractTextData(const GlicInvokeOptions& options,
                     std::u16string& text_data) {
  if (options.additional_context && options.additional_context->context) {
    for (const auto& part : options.additional_context->context->parts) {
      if (part->is_data() &&
          part->get_data()->mime_type == kMimeTypeGlicSelection) {
        const auto& buffer = part->get_data()->data;
        std::string utf8_text(buffer.begin(), buffer.end());
        text_data = base::UTF8ToUTF16(utf8_text);
        break;
      }
    }
  }
}

content::BrowserContext* GetBrowserContext(
    content::GlobalRenderFrameHostId rfh_id) {
  auto* rfh = content::RenderFrameHost::FromID(rfh_id);
  return rfh ? rfh->GetBrowserContext() : nullptr;
}

}  // namespace

SequentialTaskGroup::SequentialTaskGroup() = default;
SequentialTaskGroup::SequentialTaskGroup(
    std::vector<std::unique_ptr<GlicInvokeTask>> tasks)
    : tasks_(std::move(tasks)) {}
SequentialTaskGroup::~SequentialTaskGroup() = default;

void SequentialTaskGroup::Start(base::OnceClosure done_callback) {
  CHECK_EQ(next_task_index_, 0u);
  done_callback_ = std::move(done_callback);
  RunNextTask();
}

void SequentialTaskGroup::NotifySequenceCompleted(bool success) {
  for (auto& task : tasks_) {
    task->OnSequenceCompleted(success);
  }
}

std::optional<GlicTaskType> SequentialTaskGroup::GetLastActiveTaskType() const {
  if (next_task_index_ == 0 || tasks_.empty()) {
    return std::nullopt;
  }
  // Retrieve the task that is currently executing or just stopped.
  size_t idx = next_task_index_ - 1;
  return tasks_[idx]->GetType();
}

void SequentialTaskGroup::RunNextTask() {
  if (next_task_index_ >= tasks_.size()) {
    std::move(done_callback_).Run();
    return;
  }
  auto& task = tasks_[next_task_index_++];
  task->Start(base::BindOnce(&SequentialTaskGroup::RunNextTask,
                             weak_ptr_factory_.GetWeakPtr()));
}

ParallelTaskGroup::ParallelTaskGroup() = default;
ParallelTaskGroup::ParallelTaskGroup(
    std::vector<std::unique_ptr<GlicInvokeTask>> tasks)
    : tasks_(std::move(tasks)) {}
ParallelTaskGroup::~ParallelTaskGroup() = default;

void ParallelTaskGroup::Start(base::OnceClosure done_callback) {
  if (tasks_.empty()) {
    std::move(done_callback).Run();
    return;
  }
  base::RepeatingClosure barrier =
      base::BarrierClosure(tasks_.size(), std::move(done_callback));
  for (auto& task : tasks_) {
    task->Start(barrier);
  }
}

WaitForNavigationTask::WaitForNavigationTask(
    content::WebContents* web_contents) {
  Observe(web_contents);
}

WaitForNavigationTask::~WaitForNavigationTask() = default;

void WaitForNavigationTask::Start(base::OnceClosure done_callback) {
  if (!web_contents() ||
      !web_contents()->HasUncommittedNavigationInPrimaryMainFrame()) {
    std::move(done_callback).Run();
    return;
  }
  done_callback_ = std::move(done_callback);
}

void WaitForNavigationTask::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }
  Observe(nullptr);
  if (done_callback_) {
    std::move(done_callback_).Run();
  }
}

SetTabPendingActuationTask::SetTabPendingActuationTask(
    Profile* profile,
    tabs::TabHandle tab_handle)
    : profile_(profile), tab_handle_(tab_handle) {}

SetTabPendingActuationTask::~SetTabPendingActuationTask() = default;

void SetTabPendingActuationTask::Start(base::OnceClosure done_callback) {
  if (auto* actor_service = actor::ActorKeyedService::Get(profile_)) {
    actor_service->SetTabPendingActuation(tab_handle_);
  }
  std::move(done_callback).Run();
}

void SetTabPendingActuationTask::OnSequenceCompleted(bool success) {
  if (success) {
    return;
  }
  if (auto* actor_service = actor::ActorKeyedService::Get(profile_)) {
    actor_service->ClearTabPendingActuation(tab_handle_);
  }
}

ShowInstanceTask::ShowInstanceTask(GlicInstanceImpl& instance,
                                   ShowOptions options)
    : instance_(instance), options_(options) {}

ShowInstanceTask::~ShowInstanceTask() = default;

void ShowInstanceTask::Start(base::OnceClosure done_callback) {
  if (options_) {
    instance_->Show(*options_);
    options_.reset();
  }
  std::move(done_callback).Run();
}

SetupHiddenPanelTask::SetupHiddenPanelTask(GlicInstanceImpl& instance,
                                           tabs::TabInterface& tab)
    : instance_(instance), tab_(tab) {}

SetupHiddenPanelTask::~SetupHiddenPanelTask() = default;

void SetupHiddenPanelTask::Start(base::OnceClosure done_callback) {
  instance_->SuppressShowOnNextTabAddedToTask(true);
  instance_->BindTabWithoutShowing(&*tab_, GlicPinTrigger::kActuation,
                                   /*pin_on_bind=*/true);
  std::move(done_callback).Run();
}

MaybeInitializeHiddenClientTask::MaybeInitializeHiddenClientTask(
    GlicInstanceImpl* instance,
    mojom::InvocationSource invocation_source,
    mojom::FreOverride fre_override)
    : instance_(instance),
      invocation_source_(invocation_source),
      fre_override_(fre_override) {}

MaybeInitializeHiddenClientTask::~MaybeInitializeHiddenClientTask() = default;

// This task has no effect if the instance is not hidden.
void MaybeInitializeHiddenClientTask::Start(base::OnceClosure done_callback) {
  if (!instance_->HasActiveEmbedder()) {
    instance_->MaybeInitializeHiddenClient(invocation_source_, fre_override_);
    instance_->host().SetWebContentsVisibilityOverride(
        content::Visibility::VISIBLE);
    forced_shown_ = true;
  }
  std::move(done_callback).Run();
}

void MaybeInitializeHiddenClientTask::OnSequenceCompleted(bool success) {
  if (forced_shown_) {
    instance_->host().SetWebContentsVisibilityOverride(std::nullopt);
  }
}

WaitForClientConnectedTask::WaitForClientConnectedTask(Host& host)
    : host_(host) {
  observation_.Observe(&*host_);
}

WaitForClientConnectedTask::~WaitForClientConnectedTask() = default;

void WaitForClientConnectedTask::Start(base::OnceClosure done_callback) {
  if (host_->IsWebClientConnected()) {
    std::move(done_callback).Run();
    return;
  }
  done_callback_ = std::move(done_callback);
}

void WaitForClientConnectedTask::WebClientConnected() {
  observation_.Reset();
  if (done_callback_) {
    std::move(done_callback_).Run();
  }
}

PostCallbackTask::PostCallbackTask(base::OnceClosure callback)
    : callback_(std::move(callback)) {}

PostCallbackTask::~PostCallbackTask() = default;

void PostCallbackTask::Start(base::OnceClosure done_callback) {
  if (callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback_));
  }
  std::move(done_callback).Run();
}

StabilizationTask::StabilizationTask(content::WebContents* web_contents) {
  Observe(web_contents);
}

StabilizationTask::~StabilizationTask() = default;

void StabilizationTask::Start(base::OnceClosure done_callback) {
  done_callback_ = std::move(done_callback);
  stabilization_timer_.Start(
      FROM_HERE, base::Milliseconds(300),
      base::BindOnce(&StabilizationTask::OnStabilized, base::Unretained(this)));
}

void StabilizationTask::PrimaryMainFrameWasResized(bool width_changed) {
  if (stabilization_timer_.IsRunning()) {
    stabilization_timer_.Reset();
  }
}

void StabilizationTask::OnStabilized() {
  Observe(nullptr);
  std::move(done_callback_).Run();
}

WaitForFreCompletionTask::WaitForFreCompletionTask(
    ::Profile* profile,
    mojom::FreOverride fre_override)
    : profile_(profile), fre_override_(fre_override) {}

WaitForFreCompletionTask::~WaitForFreCompletionTask() = default;

void WaitForFreCompletionTask::Start(base::OnceClosure done_callback) {
  done_callback_ = std::move(done_callback);
  if (!ShouldWaitForFreCompletion()) {
    std::move(done_callback_).Run();
    return;
  }

  subscription_ = GlicKeyedService::Get(profile_)
                      ->enabling()
                      .RegisterProfileReadyStateChanged(base::BindRepeating(
                          &WaitForFreCompletionTask::OnProfileReadyStateChanged,
                          base::Unretained(this)));
}

void WaitForFreCompletionTask::OnProfileReadyStateChanged() {
  if (GlicEnabling::HasConsentedForProfile(profile_)) {
    subscription_ = {};
    std::move(done_callback_).Run();
  }
}

bool WaitForFreCompletionTask::ShouldWaitForFreCompletion() const {
  if (GlicEnabling::HasConsentedForProfile(profile_)) {
    return false;
  }
  return fre_override_ == mojom::FreOverride::kTrustFirstClick ||
         fre_override_ == mojom::FreOverride::kUnspecified;
}

SendToClientTask::SendToClientTask(
    GlicInstanceImpl* instance,
    mojom::InvokeOptionsPtr mojo_options,
    std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey)
    : instance_(instance),
      mojo_options_(std::move(mojo_options)),
      auto_submit_passkey_(std::move(auto_submit_passkey)) {}

SendToClientTask::~SendToClientTask() = default;

void SendToClientTask::Start(base::OnceClosure done_callback) {
  done_callback_ = std::move(done_callback);

  if (auto_submit_passkey_) {
    instance_->host().InvokeWithAutoSubmit(
        *auto_submit_passkey_, std::move(mojo_options_),
        base::BindOnce(&SendToClientTask::OnAck,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    instance_->host().Invoke(std::move(mojo_options_),
                             base::BindOnce(&SendToClientTask::OnAck,
                                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void SendToClientTask::OnAck() {
  std::move(done_callback_).Run();
}

// TODO(b/505088942): Add more robust error handling.
WaitForActuationTask::WaitForActuationTask(
    GlicInstanceImpl* instance,
    base::TimeDelta start_timeout,
    base::OnceCallback<void(GlicInvokeError)> error_callback)
    : instance_(instance),
      start_timeout_(start_timeout),
      error_callback_(std::move(error_callback)) {
  GlicActorTaskManager* task_manager = instance_->GetActorTaskManager();
  if (task_manager) {
    if (task_manager->IsActuating()) {
      did_start_ = true;
    }
    subscription_ =
        task_manager->AddActuatingChangedCallback(base::BindRepeating(
            &WaitForActuationTask::OnActuatingChanged, base::Unretained(this)));
  }
}

WaitForActuationTask::~WaitForActuationTask() = default;

void WaitForActuationTask::Start(base::OnceClosure done_callback) {
  done_callback_ = std::move(done_callback);

  GlicActorTaskManager* task_manager = instance_->GetActorTaskManager();
  if (!task_manager) {
    std::move(error_callback_).Run(GlicInvokeError::kInvalidConfiguration);
    return;
  }

  task_started_ = true;
  Update();
}

void WaitForActuationTask::OnActuatingChanged(bool actuating) {
  did_start_ = did_start_ || actuating;
  did_finish_ = did_start_ && !actuating;
  Update();
}

void WaitForActuationTask::Update() {
  if (!task_started_) {
    return;
  }

  if (did_finish_ && done_callback_) {
    timer_.Stop();
    subscription_ = {};  // Stop listening
    std::move(done_callback_).Run();
    return;
  }

  // Not done yet.
  if (!did_start_) {
    if (!timer_.IsRunning()) {
      timer_.Start(FROM_HERE, start_timeout_,
                   base::BindOnce(&WaitForActuationTask::OnTimeout,
                                  base::Unretained(this)));
    }
  } else {
    // Actuation started, stop the initial timeout timer if it was running.
    timer_.Stop();
  }
}

void WaitForActuationTask::OnTimeout() {
  timer_.Stop();
  subscription_ = {};
  std::move(error_callback_).Run(GlicInvokeError::kTimeout);
}

ClipboardPolicyTask::ClipboardPolicyTask(
    GlicInstanceImpl* instance,
    const GlicInvokeOptions& options,
    base::OnceCallback<void(GlicInvokeError)> error_callback)
    : instance_(instance), error_callback_(std::move(error_callback)) {
  if (!options.additional_context.has_value() ||
      !options.additional_context->source_rfh_id) {
    return;
  }
  source_rfh_id_ = options.additional_context->source_rfh_id;
  ExtractThumbnailData(options, thumbnail_data_);
  ExtractTextData(options, text_data_);
  src_url_ = GURL(options.additional_context->context->name.value_or(""));
  is_drag_and_drop_ =
      (options.GetInvocationSource() == mojom::InvocationSource::kWebDragDrop);
  auto* source_rfh = content::RenderFrameHost::FromID(source_rfh_id_);
  if (source_rfh) {
    image_markup_ = GetImageMarkup(src_url_, source_rfh);
  }
}

ClipboardPolicyTask::~ClipboardPolicyTask() = default;

bool ClipboardPolicyTask::TryCreateClipboardData(
    content::ClipboardPasteData& data,
    ui::ClipboardMetadata& metadata) {
  // Having both is invalid because ClipboardMetadata only supports one format.
  if (!thumbnail_data_.empty() && !text_data_.empty()) {
    std::move(error_callback_).Run(GlicInvokeError::kInvalidConfiguration);
    return false;
  }

  if (thumbnail_data_.empty() && text_data_.empty()) {
    std::move(error_callback_)
        .Run(GlicInvokeError::kAdditionalContextNoClipboardMetadata);
    return false;
  }

  ui::ClipboardFormatType format_type = ui::ClipboardFormatType::PngType();
  size_t data_size = thumbnail_data_.size();
  if (!text_data_.empty()) {
    format_type = ui::ClipboardFormatType::PlainTextType();
    data_size = text_data_.size() * sizeof(char16_t);
  }

  metadata = CreateClipboardMetadata(format_type, data_size, is_drag_and_drop_);
  data.png = thumbnail_data_;
  data.text = text_data_;
  data.html = image_markup_;
  return true;
}

CopyPolicyTask::CopyPolicyTask(
    GlicInstanceImpl* instance,
    const GlicInvokeOptions& options,
    base::OnceCallback<void(GlicInvokeError)> error_callback)
    : ClipboardPolicyTask(instance, options, std::move(error_callback)) {}

CopyPolicyTask::~CopyPolicyTask() = default;

void CopyPolicyTask::Start(base::OnceClosure done_callback) {
  done_callback_ = std::move(done_callback);

  if (!source_rfh_id_) {
    std::move(error_callback_)
        .Run(GlicInvokeError::kAdditionalContextNoSourceFrame);
    return;
  }

  auto* source_rfh = content::RenderFrameHost::FromID(source_rfh_id_);
  if (!source_rfh) {
    std::move(error_callback_)
        .Run(GlicInvokeError::kAdditionalContextNoSourceFrame);
    return;
  }

  content::ClipboardEndpoint source(
      ui::DataTransferEndpoint(
          source_rfh->GetMainFrame()->GetLastCommittedURL(),
          {.off_the_record =
               source_rfh->GetBrowserContext()->IsOffTheRecord()}),
      base::BindRepeating(
          [](content::GlobalRenderFrameHostId rfh_id)
              -> content::BrowserContext* {
            auto* rfh = content::RenderFrameHost::FromID(rfh_id);
            return rfh ? rfh->GetBrowserContext() : nullptr;
          },
          source_rfh->GetGlobalId()),
      *source_rfh);

  content::ClipboardPasteData data;
  ui::ClipboardMetadata metadata;
  if (!TryCreateClipboardData(data, metadata)) {
    return;
  }

  enterprise_data_protection::IsClipboardCopyAllowedByPolicy(
      source, metadata, data,
      base::BindOnce(&CopyPolicyTask::OnCopyPolicyCheckComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CopyPolicyTask::OnCopyPolicyCheckComplete(
    const ui::ClipboardFormatType& data_type,
    const content::ClipboardPasteData& data,
    std::optional<std::u16string> replacement_data) {
  if (replacement_data.has_value() || data.empty()) {
    std::move(error_callback_)
        .Run(GlicInvokeError::kAdditionalContextFailedCopyPolicy);
    return;
  }

  std::move(done_callback_).Run();
}

PastePolicyTask::PastePolicyTask(
    GlicInstanceImpl* instance,
    const GlicInvokeOptions& options,
    base::OnceCallback<void(GlicInvokeError)> error_callback)
    : ClipboardPolicyTask(instance, options, std::move(error_callback)) {
  if (!source_rfh_id_) {
    return;
  }
  auto* source_rfh = content::RenderFrameHost::FromID(source_rfh_id_);
  if (!source_rfh) {
    return;
  }

  content::ClipboardEndpoint source(
      ui::DataTransferEndpoint(
          source_rfh->GetMainFrame()->GetLastCommittedURL(),
          {.off_the_record =
               source_rfh->GetBrowserContext()->IsOffTheRecord()}),
      base::BindRepeating(&GetBrowserContext, source_rfh->GetGlobalId()),
      *source_rfh);

  cached_source_ = enterprise_data_protection::CacheFullPasteSource(source);
}

PastePolicyTask::~PastePolicyTask() = default;

void PastePolicyTask::Start(base::OnceClosure done_callback) {
  done_callback_ = std::move(done_callback);

  if (!cached_source_.has_value()) {
    std::move(error_callback_)
        .Run(GlicInvokeError::kAdditionalContextNoSourceFrame);
    return;
  }

  content::ClipboardPasteData data;
  ui::ClipboardMetadata metadata;
  if (!TryCreateClipboardData(data, metadata)) {
    return;
  }

  auto* host = &instance_->host();
  auto* glic_rfh = host->GetGuestMainFrame();
  if (!glic_rfh) {
    std::move(error_callback_)
        .Run(GlicInvokeError::kAdditionalContextNoClientFrame);
    return;
  }

  content::ClipboardEndpoint destination(
      ui::DataTransferEndpoint(
          glic_rfh->GetLastCommittedURL(),
          {.off_the_record = glic_rfh->GetBrowserContext()->IsOffTheRecord()}),
      base::BindRepeating(&GetBrowserContext, glic_rfh->GetGlobalId()),
      *glic_rfh);

  enterprise_data_protection::PasteIfAllowedByPolicy(
      cached_source_.value(), destination, metadata, std::move(data),
      base::BindOnce(&PastePolicyTask::OnPastePolicyCheckComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PastePolicyTask::OnPastePolicyCheckComplete(
    std::optional<content::ClipboardPasteData> data) {
  if (!data || (!thumbnail_data_.empty() && data->png.empty()) ||
      (!text_data_.empty() && data->text.empty())) {
    // Policy denied or error.
    std::move(error_callback_)
        .Run(GlicInvokeError::kAdditionalContextFailedPastePolicy);
    return;
  }
  std::move(done_callback_).Run();
}

std::optional<GlicTaskType> GlicInvokeTask::GetType() const {
  return std::nullopt;
}

std::optional<GlicTaskType> SequentialTaskGroup::GetType() const {
  return GlicTaskType::kSequentialTaskGroup;
}

std::optional<GlicTaskType> ParallelTaskGroup::GetType() const {
  return GlicTaskType::kParallelTaskGroup;
}

std::optional<GlicTaskType> WaitForNavigationTask::GetType() const {
  return GlicTaskType::kWaitForNavigation;
}

std::optional<GlicTaskType> SetTabPendingActuationTask::GetType() const {
  return GlicTaskType::kSetTabPendingActuation;
}

std::optional<GlicTaskType> ShowInstanceTask::GetType() const {
  return GlicTaskType::kShowInstance;
}

std::optional<GlicTaskType> SetupHiddenPanelTask::GetType() const {
  return GlicTaskType::kSetupHiddenPanel;
}

std::optional<GlicTaskType> MaybeInitializeHiddenClientTask::GetType() const {
  return GlicTaskType::kMaybeInitializeHiddenClient;
}

std::optional<GlicTaskType> WaitForClientConnectedTask::GetType() const {
  return GlicTaskType::kWaitForClientConnected;
}

std::optional<GlicTaskType> PostCallbackTask::GetType() const {
  return GlicTaskType::kPostCallback;
}

std::optional<GlicTaskType> StabilizationTask::GetType() const {
  return GlicTaskType::kStabilization;
}

std::optional<GlicTaskType> WaitForFreCompletionTask::GetType() const {
  return GlicTaskType::kWaitForFreCompletion;
}

std::optional<GlicTaskType> SendToClientTask::GetType() const {
  return GlicTaskType::kSendToClient;
}

std::optional<GlicTaskType> WaitForActuationTask::GetType() const {
  return GlicTaskType::kWaitForActuation;
}

std::optional<GlicTaskType> CopyPolicyTask::GetType() const {
  return GlicTaskType::kCopyPolicy;
}

std::optional<GlicTaskType> PastePolicyTask::GetType() const {
  return GlicTaskType::kPastePolicy;
}

}  // namespace glic

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
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_handle.h"
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

ui::ClipboardMetadata CreateClipboardMetadata(size_t size,
                                              bool is_drag_and_drop) {
  ui::ClipboardMetadata metadata;
  metadata.format_type = ui::ClipboardFormatType::PngType();
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

}  // namespace

SequentialTaskGroup::SequentialTaskGroup() = default;
SequentialTaskGroup::SequentialTaskGroup(
    std::vector<std::unique_ptr<GlicInvokeTask>> tasks)
    : tasks_(std::move(tasks)) {}
SequentialTaskGroup::~SequentialTaskGroup() = default;

void SequentialTaskGroup::Start(base::OnceClosure done_callback) {
  CHECK_EQ(current_task_index_, 0u);
  done_callback_ = std::move(done_callback);
  RunNextTask();
}

void SequentialTaskGroup::NotifySequenceCompleted(bool success) {
  for (auto& task : tasks_) {
    task->OnSequenceCompleted(success);
  }
}

void SequentialTaskGroup::RunNextTask() {
  if (current_task_index_ >= tasks_.size()) {
    std::move(done_callback_).Run();
    return;
  }
  auto& task = tasks_[current_task_index_++];
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

ShowInstanceTask::ShowInstanceTask(GlicInstanceImpl* instance,
                                   ShowOptions options)
    : instance_(instance), options_(options) {}

ShowInstanceTask::~ShowInstanceTask() = default;

void ShowInstanceTask::Start(base::OnceClosure done_callback) {
  instance_->Show(options_);
  std::move(done_callback).Run();
}

SetupHiddenPanelTask::SetupHiddenPanelTask(GlicInstanceImpl* instance,
                                           tabs::TabInterface* tab)
    : instance_(instance), tab_(tab) {}

SetupHiddenPanelTask::~SetupHiddenPanelTask() = default;

void SetupHiddenPanelTask::Start(base::OnceClosure done_callback) {
  instance_->SuppressShowOnNextTabAddedToTask(true);
  instance_->BindTabWithoutShowing(tab_, GlicPinTrigger::kActuation,
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
    if (content::WebContents* ui_contents =
            instance_->host().webui_contents()) {
      ui_contents->WasShown();
    }
    if (content::WebContents* client_contents =
            instance_->host().web_client_contents()) {
      client_contents->WasShown();
    }
    forced_shown_ = true;
  }
  std::move(done_callback).Run();
}

void MaybeInitializeHiddenClientTask::OnSequenceCompleted(bool success) {
  // Only revert to hidden if we forced it to be shown and there is still no
  // active embedder. This prevents overriding the state if an active embedder
  // was created while the task was running (as this code is async).
  if (forced_shown_ && !instance_->HasActiveEmbedder()) {
    if (content::WebContents* ui_contents =
            instance_->host().webui_contents()) {
      ui_contents->WasHidden();
    }
    if (content::WebContents* client_contents =
            instance_->host().web_client_contents()) {
      client_contents->WasHidden();
    }
  }
}

WaitForClientConnectedTask::WaitForClientConnectedTask(Host* host)
    : host_(host) {
  observation_.Observe(host_);
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

NotifyIsInvokingTask::NotifyIsInvokingTask(Host* host) : host_(host) {}

NotifyIsInvokingTask::~NotifyIsInvokingTask() = default;

void NotifyIsInvokingTask::Start(base::OnceClosure done_callback) {
  did_start_ = true;
  host_->NotifyIsInvoking(true);
  std::move(done_callback).Run();
}

void NotifyIsInvokingTask::OnSequenceCompleted(bool success) {
  if (did_start_) {
    host_->NotifyIsInvoking(false);
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
    base::OnceCallback<void(GlicInvokeError)> error_callback,
    base::OnceClosure on_actuation_started)
    : instance_(instance),
      start_timeout_(start_timeout),
      error_callback_(std::move(error_callback)),
      on_actuation_started_(std::move(on_actuation_started)) {
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

  if (did_start_ && on_actuation_started_) {
    std::move(on_actuation_started_).Run();
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
  src_url_ = GURL(options.additional_context->context->name.value_or(""));
  is_drag_and_drop_ =
      (options.GetInvocationSource() == mojom::InvocationSource::kWebDragDrop);
}

ClipboardPolicyTask::~ClipboardPolicyTask() = default;

void ClipboardPolicyTask::Start(base::OnceClosure done_callback) {
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

  ui::ClipboardMetadata metadata =
      CreateClipboardMetadata(thumbnail_data_.size(), is_drag_and_drop_);

  content::ClipboardPasteData data;
  data.png = thumbnail_data_;
  data.html = GetImageMarkup(src_url_, source_rfh);

  RunPolicyCheck(source, metadata, std::move(data), source_rfh);
}

CopyPolicyTask::CopyPolicyTask(
    GlicInstanceImpl* instance,
    const GlicInvokeOptions& options,
    base::OnceCallback<void(GlicInvokeError)> error_callback)
    : ClipboardPolicyTask(instance, options, std::move(error_callback)) {}

CopyPolicyTask::~CopyPolicyTask() = default;

void CopyPolicyTask::RunPolicyCheck(const content::ClipboardEndpoint& source,
                                    const ui::ClipboardMetadata& metadata,
                                    content::ClipboardPasteData data,
                                    content::RenderFrameHost* source_rfh) {
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

PastePolicyCheckTask::PastePolicyCheckTask(
    content::WebContents* web_contents,
    GlicInstanceImpl* instance,
    const GlicInvokeOptions& options,
    base::OnceCallback<void(GlicInvokeError)> error_callback)
    : ClipboardPolicyTask(instance, options, std::move(error_callback)) {
  Observe(web_contents);
}

PastePolicyCheckTask::~PastePolicyCheckTask() = default;

void PastePolicyCheckTask::RunPolicyCheck(
    const content::ClipboardEndpoint& source,
    const ui::ClipboardMetadata& metadata,
    content::ClipboardPasteData paste_data,
    content::RenderFrameHost* source_rfh) {
  if (thumbnail_data_.empty()) {
    std::move(error_callback_)
        .Run(GlicInvokeError::kAdditionalContextNoClipboardMetadata);
    return;
  }

  auto* host = &instance_->host();
  auto* glic_rfh = host->GetGuestMainFrame();
  if (!glic_rfh) {
    std::move(error_callback_)
        .Run(GlicInvokeError::kAdditionalContextNoClientFrame);
    return;
  }

  auto get_browser_context =
      [](content::GlobalRenderFrameHostId rfh_id) -> content::BrowserContext* {
    auto* rfh = content::RenderFrameHost::FromID(rfh_id);
    return rfh ? rfh->GetBrowserContext() : nullptr;
  };

  content::ClipboardEndpoint destination(
      ui::DataTransferEndpoint(
          glic_rfh->GetLastCommittedURL(),
          {.off_the_record = glic_rfh->GetBrowserContext()->IsOffTheRecord()}),
      base::BindRepeating(get_browser_context, glic_rfh->GetGlobalId()),
      *glic_rfh);

  enterprise_data_protection::PasteIfAllowedByPolicy(
      source, destination, metadata, std::move(paste_data),
      base::BindOnce(&PastePolicyCheckTask::OnPastePolicyCheckComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PastePolicyCheckTask::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  std::move(error_callback_)
      .Run(GlicInvokeError::kAdditionalContextSawNavigation);
}

void PastePolicyCheckTask::OnPastePolicyCheckComplete(
    std::optional<content::ClipboardPasteData> data) {
  Observe(nullptr);
  if (!data || data->png.empty()) {
    // Policy denied or error.
    std::move(error_callback_)
        .Run(GlicInvokeError::kAdditionalContextFailedPastePolicy);
    return;
  }
  std::move(done_callback_).Run();
}

}  // namespace glic

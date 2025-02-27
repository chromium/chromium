// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_annotation_manager.h"

#include "base/strings/escape.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/common/chrome_features.h"
#include "components/shared_highlighting/core/common/text_fragment.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace glic {

GlicAnnotationManager::GlicAnnotationManager(GlicKeyedService* service)
    : service_(service) {}

GlicAnnotationManager::~GlicAnnotationManager() = default;

void GlicAnnotationManager::ScrollTo(
    mojom::ScrollToParamsPtr params,
    mojom::WebClientHandler::ScrollToCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicScrollTo));
  MaybeFailAndResetTask(glic::mojom::ScrollToErrorReason::kNewerScrollToCall);

  mojom::ScrollToSelector* selector = params->selector.get();
  std::optional<shared_highlighting::TextFragment> text_fragment;
  // TODO(crbug.com/395872487): We need to verify text is from the main frame.
  if (selector->is_exact_text_selector()) {
    const std::string& exact_text = selector->get_exact_text_selector()->text;
    if (exact_text.empty()) {
      std::move(callback).Run(mojom::ScrollToErrorReason::kNotSupported);
      return;
    }
    text_fragment = shared_highlighting::TextFragment(exact_text);
  } else if (selector->is_text_fragment_selector()) {
    auto* text_fragment_selector = selector->get_text_fragment_selector().get();
    const std::string& text_start = text_fragment_selector->text_start;
    if (text_start.empty()) {
      std::move(callback).Run(mojom::ScrollToErrorReason::kNotSupported);
      return;
    }
    const std::string& text_end = text_fragment_selector->text_end;
    if (text_end.empty()) {
      std::move(callback).Run(mojom::ScrollToErrorReason::kNotSupported);
      return;
    }
    text_fragment = shared_highlighting::TextFragment(text_start, text_end,
                                                      /*prefix=*/std::string(),
                                                      /*suffix=*/std::string());
  } else {
    mojo::ReportBadMessage(
        "The client should have verified that one of the selector types was "
        "specified.");
    return;
  }

  // The only support selector types currently are text and text fragment, so
  // this must have a non-empty value.
  CHECK(text_fragment.has_value());

  if (!annotation_agent_container_.is_bound()) {
    auto focused_tab_data = service_->GetFocusedTabData();
    if (focused_tab_data.focused_tab_contents) {
      focused_primary_page_ =
          focused_tab_data.focused_tab_contents->GetPrimaryPage().GetWeakPtr();
    }
    if (!focused_primary_page_) {
      std::move(callback).Run(mojom::ScrollToErrorReason::kNoFocusedTab);
      return;
    }
    // Using base::Unretained is safe here because `service_` will outlive
    // `this`.
    tab_change_subscription_ = service_->AddFocusedTabChangedCallback(
        base::BindRepeating(&GlicAnnotationManager::OnFocusedTabChanged,
                            base::Unretained(this)));
    focused_primary_page_->GetMainDocument()
        .GetRemoteInterfaces()
        ->GetInterface(
            annotation_agent_container_.BindNewPipeAndPassReceiver());
  }

  mojo::PendingReceiver<blink::mojom::AnnotationAgentHost> agent_host_receiver;
  mojo::Remote<blink::mojom::AnnotationAgent> agent_remote;
  annotation_agent_container_->CreateAgent(
      agent_host_receiver.InitWithNewPipeAndPassRemote(),
      agent_remote.BindNewPipeAndPassReceiver(),
      blink::mojom::AnnotationType::kGlic,
      text_fragment->ToEscapedString(
          shared_highlighting::TextFragment::EscapedStringFormat::
              kWithoutTextDirective));
  annotation_task_ = std::make_unique<AnnotationTask>(
      std::move(agent_remote), std::move(agent_host_receiver),
      std::move(callback));
}

GlicAnnotationManager::AnnotationTask::AnnotationTask(
    mojo::Remote<blink::mojom::AnnotationAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::AnnotationAgentHost>
        agent_host_pending_receiver,
    mojom::WebClientHandler::ScrollToCallback callback)
    : annotation_agent_(std::move(agent_remote)),
      annotation_agent_host_receiver_(this,
                                      std::move(agent_host_pending_receiver)),
      scroll_to_callback_(std::move(callback)) {}

GlicAnnotationManager::AnnotationTask::~AnnotationTask() {
  if (scroll_to_callback_) {
    std::move(scroll_to_callback_)
        .Run(mojom::ScrollToErrorReason::kNotSupported);
  }
}

void GlicAnnotationManager::AnnotationTask::MaybeFailTask(
    mojom::ScrollToErrorReason error_reason) {
  if (!scroll_to_callback_) {
    return;
  }
  std::move(scroll_to_callback_).Run(error_reason);
  annotation_agent_.reset();
  annotation_agent_host_receiver_.reset();
}

void GlicAnnotationManager::AnnotationTask::DidFinishAttachment(
    const gfx::Rect& document_relative_rect) {
  if (document_relative_rect.IsEmpty()) {
    std::move(scroll_to_callback_)
        .Run(mojom::ScrollToErrorReason::kNoMatchFound);
    return;
  }

  annotation_agent_->ScrollIntoView();
  std::move(scroll_to_callback_).Run(std::nullopt);
}

void GlicAnnotationManager::MaybeFailAndResetTask(
    glic::mojom::ScrollToErrorReason error_reason) {
  if (annotation_task_) {
    annotation_task_->MaybeFailTask(error_reason);
    annotation_task_.reset();
  }
}

// Note: In addition to when the focused tab changes, this gets called when
// the currently focused tab navigates its primary page (i.e.
// PrimaryPageChanged). We also want to perform these steps in that scenario.
void GlicAnnotationManager::OnFocusedTabChanged(
    FocusedTabData focused_tab_data) {
  content::Page* prev_focused_primary_page = focused_primary_page_.get();
  content::Page* new_focused_primary_page = nullptr;
  if (focused_tab_data.focused_tab_contents) {
    new_focused_primary_page =
        &focused_tab_data.focused_tab_contents->GetPrimaryPage();
  }
  // If the focused tab hasn't changed and it's primary page hasn't changed, we
  // don't need to do anything.
  if (prev_focused_primary_page == new_focused_primary_page) {
    return;
  }
  MaybeFailAndResetTask(
      mojom::ScrollToErrorReason::kFocusedTabChangedOrNavigated);
  annotation_agent_container_.reset();
  tab_change_subscription_ = base::CallbackListSubscription();
}

}  // namespace glic

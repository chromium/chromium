// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_page_stability_waiter.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_change/password_change_logging_util.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/form_predictions_tracker.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace {

using Logger = password_manager::BrowserSavePasswordProgressLogger;
using password_change::LogMessage;

void RecordStabilityWaitDuration(base::Time start_time) {
  base::UmaHistogramTimes(
      "PasswordManager.PasswordChange.StabilityWaitDuration",
      base::Time::Now() - start_time);
}

}  // namespace

PasswordChangePageStabilityWaiter::PasswordChangePageStabilityWaiter(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    base::OnceClosure callback)
    : content::WebContentsObserver(web_contents),
      callback_(std::move(callback).Then(
          base::BindOnce(&RecordStabilityWaitDuration, base::Time::Now()))),
      client_(client) {
  CHECK(web_contents);
  // Set a timeout to prevent waiting forever if the page does not reach stable
  // state.
  timeout_timer_.Start(
      FROM_HERE, password_manager::features::kAwaitPageStabilityTimeout.Get(),
      this, &PasswordChangePageStabilityWaiter::OnTimeout);

  StartWaiting();
}

void PasswordChangePageStabilityWaiter::StartWaiting() {
  LogMessage(client_, Logger::STRING_PASSWORD_CHANGE_STABILITY_MONITOR_STARTED);

  if (web_contents()->IsLoading()) {
    return;
  }

  CheckPageStability();
}

void PasswordChangePageStabilityWaiter::CheckPageStability() {
  CHECK(web_contents());
  CHECK(web_contents()->GetPrimaryMainFrame());

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  web_contents()
      ->GetPrimaryMainFrame()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&chrome_render_frame);

  if (!chrome_render_frame) {
    OnTimeout();
    return;
  }

  chrome_render_frame->CreatePageStabilityMonitor(
      monitor_.BindNewPipeAndPassReceiver(), actor::TaskId(),
      /*supports_paint_stability=*/true);

  monitor_.set_disconnect_handler(
      base::BindOnce(&PasswordChangePageStabilityWaiter::CheckVisualState,
                     weak_ptr_factory_.GetWeakPtr()));

  monitor_->NotifyWhenStable(
      base::Seconds(0),
      base::BindOnce(&PasswordChangePageStabilityWaiter::CheckVisualState,
                     weak_ptr_factory_.GetWeakPtr()));
}

PasswordChangePageStabilityWaiter::~PasswordChangePageStabilityWaiter() =
    default;

void PasswordChangePageStabilityWaiter::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle && (!navigation_handle->IsInPrimaryMainFrame() ||
                            !navigation_handle->HasCommitted())) {
    return;
  }

  // Reset weak pointers to cancel pending checks since there was navigation on
  // a page.
  weak_ptr_factory_.InvalidateWeakPtrs();
  monitor_.reset();

  // Start over.
  StartWaiting();
}

void PasswordChangePageStabilityWaiter::DidStopLoading() {
  if (!monitor_) {
    CheckPageStability();
  }
}

void PasswordChangePageStabilityWaiter::CheckVisualState() {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kUseDetachedWidget)) {
    OnAllChecksCompleted();
    return;
  }

  web_contents()->GetPrimaryMainFrame()->InsertVisualStateCallback(
      base::IgnoreArgs<bool>(base::BindOnce(
          &PasswordChangePageStabilityWaiter::OnAllChecksCompleted,
          weak_ptr_factory_.GetWeakPtr())));
}

void PasswordChangePageStabilityWaiter::OnAllChecksCompleted() {
  LogMessage(client_,
             Logger::STRING_PASSWORD_CHANGE_STABILITY_MONITOR_SUCCEEDED);
  if (callback_) {
    std::move(callback_).Run();
  }
}

void PasswordChangePageStabilityWaiter::OnTimeout() {
  LogMessage(client_,
             Logger::STRING_PASSWORD_CHANGE_STABILITY_MONITOR_TIMED_OUT);
  // Even if disconnected, we invoke the callback unconditionally so that we
  // don't leave callers hanging forever in case of mojo errors.
  if (callback_) {
    std::move(callback_).Run();
  }
}

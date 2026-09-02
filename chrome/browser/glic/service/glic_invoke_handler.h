// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_

#include <memory>
#include <variant>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"
#include "components/tabs/public/tab_interface.h"

class Profile;

namespace glic {

class GlicInstanceImpl;

class SequentialTaskGroup;
enum class GlicTaskType;

// Handles an invocation of Glic, parsing options and communicating with the
// instance's host.
class GlicInvokeHandler {
 public:
  using CompletionCallback =
      base::OnceCallback<void(GlicInstance*, GlicInvokeHandler*)>;

  struct TabSurface {
    raw_ptr<tabs::TabInterface> tab;
    bool is_new = false;
  };

  using ResolvedTarget = std::variant<TabSurface, Floating>;

  static bool RequiresClientInvoke(const mojom::InvokeOptionsPtr& mojo_options,
                                   bool has_auto_submit_passkey);

  // Resolves the target surface to a specific tab.
  static ResolvedTarget ResolveTargetSurface(Profile* profile,
                                             const Target& target);

  // `tab` must be non-nullptr.
  // `completion_callback` should be called exactly once and results in
  // destruction of `this`.
  GlicInvokeHandler(
      GlicInstanceImpl& instance,
      ResolvedTarget resolved_target,
      GlicInvokeOptions options,
      GlicInvokeWithAutoSubmitOptions auto_submit_options,
      std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey,
      std::unique_ptr<GlicInvokeMetrics> invoke_metrics,
      CompletionCallback completion_callback);
  ~GlicInvokeHandler();

  GlicInvokeHandler(const GlicInvokeHandler&) = delete;
  GlicInvokeHandler& operator=(const GlicInvokeHandler&) = delete;

  // Kicks off the invocation process.
  void Invoke();

  // Cancels the invocation, generating an error callback.
  void Cancel(GlicInvokeError error);

  void set_completion_callback(CompletionCallback completion_callback) {
    completion_callback_ = std::move(completion_callback);
  }

  // Returns the task type of the last active task, if the invocation sequence
  // is currently running or failed. Returns std::nullopt if no tasks have been
  // started or if the `main_task_` is not present.
  std::optional<GlicTaskType> GetLastActiveTaskType() const;

 private:
  bool IsFloatingTarget() const;
  bool IsTabTarget() const;
  tabs::TabInterface& GetTab() const;
  mojom::InvokeOptionsPtr CreateMojoOptions();
  bool IsActuatingFeatureMode() const;

  // Deletes `this`. Exactly one of these methods will be called.
  void OnSuccess();
  void OnError(GlicInvokeError error);

  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  void OnInstanceWillBeDestroyed(GlicInstance* instance);
  void OnConversationInfoChanged(const mojom::ConversationInfo& info);
  void OnActuatingChanged(bool actuating);
  const base::raw_ref<GlicInstanceImpl> instance_;
  ResolvedTarget resolved_target_;
  GlicInvokeOptions options_;
  std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey_;
  // Calling this synchronously destroys `this`.
  GlicInvokeWithAutoSubmitOptions auto_submit_options_;
  CompletionCallback completion_callback_;

  bool should_wait_for_load_ = false;
  base::CallbackListSubscription instance_destruction_subscription_;
  base::CallbackListSubscription tab_destruction_subscription_;
  base::CallbackListSubscription conversation_subscription_;
  base::CallbackListSubscription actuating_subscription_;
  base::OneShotTimer timeout_timer_;

  std::unique_ptr<SequentialTaskGroup> main_task_;
  std::unique_ptr<GlicInvokeMetrics> metrics_;

  base::WeakPtrFactory<GlicInvokeHandler> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_

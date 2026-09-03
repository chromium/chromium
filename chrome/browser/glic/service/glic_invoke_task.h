// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_TASK_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_TASK_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metadata.h"
#include "url/gurl.h"

class Profile;

namespace glic {

enum class GlicTaskType : int {
  kSequentialTaskGroup = 1,
  kParallelTaskGroup = 2,
  kWaitForNavigation = 3,
  kSetTabPendingActuation = 4,
  kShowInstance = 5,
  kSetupHiddenPanel = 6,
  kMaybeInitializeHiddenClient = 7,
  kWaitForClientConnected = 8,
  kPostCallback = 9,
  kStabilization = 10,
  kWaitForFreCompletion = 11,
  kSendToClient = 12,
  kWaitForActuation = 13,
  kClipboardPolicy = 14,
  kCopyPolicy = 15,
  kPastePolicy = 16,
};

class GlicInvokeTask {
 public:
  virtual ~GlicInvokeTask() = default;
  virtual void Start(base::OnceClosure done_callback) = 0;
  // Called when the sequence of tasks completes (successfully or not).
  // This is where tasks should do cleanup.
  virtual void OnSequenceCompleted(bool success) {}
  virtual std::optional<GlicTaskType> GetType() const;
};

// Executes tasks sequentially in the order they were added.
// This class is not thread-safe and should only be used on a single sequence.
// It cannot be executed more than once.
class SequentialTaskGroup : public GlicInvokeTask {
 public:
  std::optional<GlicTaskType> GetType() const override;

  SequentialTaskGroup();
  explicit SequentialTaskGroup(
      std::vector<std::unique_ptr<GlicInvokeTask>> tasks);
  ~SequentialTaskGroup() override;

  void Start(base::OnceClosure done_callback) override;

  // Notifies all tasks in the group that the sequence has completed.
  void NotifySequenceCompleted(bool success);

  // Returns the type of the task that is currently executing, or the last
  // executed task if the group has finished/errored. If no tasks have started,
  // returns std::nullopt.
  std::optional<GlicTaskType> GetLastActiveTaskType() const;

 private:
  void RunNextTask();
  std::vector<std::unique_ptr<GlicInvokeTask>> tasks_;
  size_t next_task_index_ = 0;
  base::OnceClosure done_callback_;
  base::WeakPtrFactory<SequentialTaskGroup> weak_ptr_factory_{this};
};

// Executes all tasks in parallel. Completes when all tasks have completed.
// This class is not thread-safe and should only be used on a single sequence.
// It cannot be executed more than once.
class ParallelTaskGroup : public GlicInvokeTask {
 public:
  std::optional<GlicTaskType> GetType() const override;

  ParallelTaskGroup();
  explicit ParallelTaskGroup(
      std::vector<std::unique_ptr<GlicInvokeTask>> tasks);
  ~ParallelTaskGroup() override;

  void Start(base::OnceClosure done_callback) override;

 private:
  std::vector<std::unique_ptr<GlicInvokeTask>> tasks_;
};

class Host;
class GlicInstanceImpl;

// Task that waits for the primary main frame to finish navigation and commit.
class WaitForNavigationTask : public GlicInvokeTask,
                              public content::WebContentsObserver {
 public:
  std::optional<GlicTaskType> GetType() const override;

  explicit WaitForNavigationTask(content::WebContents* web_contents);
  ~WaitForNavigationTask() override;
  void Start(base::OnceClosure done_callback) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  base::OnceClosure done_callback_;
};

// Task that immediately marks a tab as pending actuation to show the visual
// indicator.
class SetTabPendingActuationTask : public GlicInvokeTask {
 public:
  std::optional<GlicTaskType> GetType() const override;

  SetTabPendingActuationTask(Profile* profile, tabs::TabHandle tab_handle);
  ~SetTabPendingActuationTask() override;
  void Start(base::OnceClosure done_callback) override;
  void OnSequenceCompleted(bool success) override;

 private:
  const raw_ptr<Profile> profile_;
  tabs::TabHandle tab_handle_;
};

// Task that shows the Glic instance.
class ShowInstanceTask : public GlicInvokeTask {
 public:
  std::optional<GlicTaskType> GetType() const override;

  ShowInstanceTask(GlicInstanceImpl& instance, ShowOptions options);
  ~ShowInstanceTask() override;
  void Start(base::OnceClosure done_callback) override;

 private:
  // Safe because GlicInvokeHandler (which owns this task) is owned by
  // GlicInstanceCoordinatorImpl and its lifetime is tied to the instance.
  const base::raw_ref<GlicInstanceImpl> instance_;
  std::optional<ShowOptions> options_;
};

// Task that sets up the instance for a hidden panel.
class SetupHiddenPanelTask : public GlicInvokeTask {
 public:
  std::optional<GlicTaskType> GetType() const override;

  SetupHiddenPanelTask(GlicInstanceImpl& instance, tabs::TabInterface& tab);
  ~SetupHiddenPanelTask() override;
  void Start(base::OnceClosure done_callback) override;

 private:
  const base::raw_ref<GlicInstanceImpl> instance_;
  const base::raw_ref<tabs::TabInterface> tab_;
};

class MaybeInitializeHiddenClientTask : public GlicInvokeTask {
 public:
  std::optional<GlicTaskType> GetType() const override;

  MaybeInitializeHiddenClientTask(GlicInstanceImpl* instance,
                                  mojom::InvocationSource invocation_source,
                                  mojom::FreOverride fre_override);
  ~MaybeInitializeHiddenClientTask() override;
  void Start(base::OnceClosure done_callback) override;
  void OnSequenceCompleted(bool success) override;

 private:
  raw_ptr<GlicInstanceImpl> instance_;
  mojom::InvocationSource invocation_source_;
  mojom::FreOverride fre_override_;
  bool forced_shown_ = false;
};

// Task that waits for the web client to be connected to the host.
class WaitForClientConnectedTask : public GlicInvokeTask,
                                   public Host::Observer {
 public:
  std::optional<GlicTaskType> GetType() const override;

  explicit WaitForClientConnectedTask(Host& host);
  ~WaitForClientConnectedTask() override;
  void Start(base::OnceClosure done_callback) override;
  void WebClientConnected() override;

 private:
  // Safe because GlicInvokeHandler (which owns this task) is owned by
  // GlicInstanceCoordinatorImpl and its lifetime is tied to the instance,
  // which owns the Host.
  const base::raw_ref<Host> host_;
  base::ScopedObservation<Host, Host::Observer> observation_{this};
  base::OnceClosure done_callback_;
};

// Task that posts a callback asynchronously.
class PostCallbackTask : public GlicInvokeTask {
 public:
  std::optional<GlicTaskType> GetType() const override;

  explicit PostCallbackTask(base::OnceClosure callback);
  ~PostCallbackTask() override;
  void Start(base::OnceClosure done_callback) override;

 private:
  base::OnceClosure callback_;
};

// Task that waits for the layout to stabilize after showing the panel.
class StabilizationTask : public GlicInvokeTask,
                          public content::WebContentsObserver {
 public:
  std::optional<GlicTaskType> GetType() const override;

  explicit StabilizationTask(content::WebContents* web_contents);
  ~StabilizationTask() override;
  void Start(base::OnceClosure done_callback) override;
  void PrimaryMainFrameWasResized(bool width_changed) override;

 private:
  void OnStabilized();
  base::OneShotTimer stabilization_timer_;
  base::OnceClosure done_callback_;
};

// Task that waits for the First Run Experience (FRE) to be completed if
// necessary.
class WaitForFreCompletionTask : public GlicInvokeTask {
 public:
  std::optional<GlicTaskType> GetType() const override;

  WaitForFreCompletionTask(::Profile* profile, mojom::FreOverride fre_override);
  ~WaitForFreCompletionTask() override;
  void Start(base::OnceClosure done_callback) override;

 private:
  void OnProfileReadyStateChanged();
  bool ShouldWaitForFreCompletion() const;

  raw_ptr<::Profile> profile_;
  mojom::FreOverride fre_override_;
  base::OnceClosure done_callback_;
  base::CallbackListSubscription subscription_;
};

// Task that sends the invocation to the client.
class SendToClientTask : public GlicInvokeTask {
 public:
  std::optional<GlicTaskType> GetType() const override;

  SendToClientTask(
      GlicInstanceImpl* instance,
      mojom::InvokeOptionsPtr mojo_options,
      std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey);
  ~SendToClientTask() override;
  void Start(base::OnceClosure done_callback) override;

 private:
  void OnAck();

  raw_ptr<GlicInstanceImpl> instance_;
  mojom::InvokeOptionsPtr mojo_options_;
  std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey_;
  base::OnceClosure done_callback_;
  base::WeakPtrFactory<SendToClientTask> weak_ptr_factory_{this};
};

// Task that waits for actuation (both start and complete).
class WaitForActuationTask : public GlicInvokeTask {
 public:
  std::optional<GlicTaskType> GetType() const override;

  WaitForActuationTask(
      GlicInstanceImpl* instance,
      base::TimeDelta start_timeout,
      base::OnceCallback<void(GlicInvokeError)> error_callback);
  ~WaitForActuationTask() override;
  void Start(base::OnceClosure done_callback) override;

 private:
  void OnTimeout();
  void OnActuatingChanged(bool actuating);
  void Update();

  raw_ptr<GlicInstanceImpl> instance_;
  base::TimeDelta start_timeout_;
  base::OnceCallback<void(GlicInvokeError)> error_callback_;
  base::OnceClosure done_callback_;

  base::OneShotTimer timer_;

  bool task_started_ = false;
  bool did_start_ = false;
  bool did_finish_ = false;
  base::CallbackListSubscription subscription_;
};

// Base class for tasks that perform enterprise data protection policy checks
// on clipboard-like data.
class ClipboardPolicyTask : public GlicInvokeTask {
 public:
  ClipboardPolicyTask(GlicInstanceImpl* instance,
                      const GlicInvokeOptions& options,
                      base::OnceCallback<void(GlicInvokeError)> error_callback);
  ~ClipboardPolicyTask() override;

 protected:
  bool TryCreateClipboardData(content::ClipboardPasteData& data,
                              ui::ClipboardMetadata& metadata);

  raw_ptr<GlicInstanceImpl> instance_;
  content::GlobalRenderFrameHostId source_rfh_id_;
  std::vector<uint8_t> thumbnail_data_;
  std::u16string text_data_;
  GURL src_url_;
  bool is_drag_and_drop_ = false;
  std::u16string image_markup_;
  base::OnceClosure done_callback_;
  base::OnceCallback<void(GlicInvokeError)> error_callback_;
};

class CopyPolicyTask : public ClipboardPolicyTask {
 public:
  std::optional<GlicTaskType> GetType() const override;

  CopyPolicyTask(GlicInstanceImpl* instance,
                 const GlicInvokeOptions& options,
                 base::OnceCallback<void(GlicInvokeError)> error_callback);
  ~CopyPolicyTask() override;

  void Start(base::OnceClosure done_callback) override;

 private:
  void OnCopyPolicyCheckComplete(
      const ui::ClipboardFormatType& data_type,
      const content::ClipboardPasteData& data,
      std::optional<std::u16string> replacement_data);

  base::WeakPtrFactory<CopyPolicyTask> weak_ptr_factory_{this};
};

class PastePolicyTask : public ClipboardPolicyTask {
 public:
  std::optional<GlicTaskType> GetType() const override;

  PastePolicyTask(GlicInstanceImpl* instance,
                  const GlicInvokeOptions& options,
                  base::OnceCallback<void(GlicInvokeError)> error_callback);
  ~PastePolicyTask() override;

  void Start(base::OnceClosure done_callback) override;

 private:
  void OnPastePolicyCheckComplete(
      std::optional<content::ClipboardPasteData> data);

  std::optional<enterprise_data_protection::FullPasteSource> cached_source_;

  base::WeakPtrFactory<PastePolicyTask> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_TASK_H_

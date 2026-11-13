// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TASK_UI_ACTOR_TASK_UI_MODEL_H_
#define CHROME_BROWSER_ACTOR_TASK_UI_ACTOR_TASK_UI_MODEL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "components/actor/core/task_id.h"

namespace actor {

// One per-profile view of the actor tasks that run through browser actuation,
// for the UI surfaces that show them: a side panel, an entry point,
// notifications, and later a task list. Every surface reads tasks through this
// interface instead of reading actor or transport internals, so the surfaces
// don't depend on each other or on where a task came from.
//
// The model combines what the browser knows about a task: its actor task, if
// one runs on this device, and the events its caller receives (progress, a
// request to the user, the outcome). It holds no surface-specific state.
// Controls act on the task itself, so they keep working when a surface is
// closed.
//
// Unlike `ui::ActorUiStateManager` (chrome/browser/actor/ui/), which drives
// the in-tab UI for actor tasks running on this device, such as the tab
// indicator, the overlay, and the handoff button, this model serves surfaces
// outside the tab, such as a side panel, an entry point, and notifications.
//
// Lives on the UI thread.
class ActorTaskUiModel {
 public:
  // Identifies a task in this model. Stable for the task's lifetime, including
  // before an actor task exists for it and after the actor task is gone.
  using TaskUiId = base::IdType32<class TaskUiIdTag>;

  // Where a task is in its life, as the UI presents it. Finer-grained actor
  // states (acting, reflecting) collapse into kRunning.
  enum class Lifecycle {
    // Requested, but no actions have run yet.
    kStarting,
    // The task is running.
    kRunning,
    // The user paused the task without taking control of its page. The task
    // waits until resumed.
    kPaused,
    // The task is waiting on the user, for example for a confirmation, a
    // reply, or an action on the page. See `TaskView::user_request`.
    kNeedsUser,
    // The user took control of the task's page. The task waits until the user
    // hands control back.
    kUserInControl,
    // Terminal states.
    kSucceeded,
    kFailed,
    kCancelled,
  };

  // The controls a surface can offer for a task. See
  // `TaskView::allowed_controls`.
  enum class Control {
    kStop,
    kPause,
    kTakeOver,
    kResume,
    kMinValue = kStop,
    kMaxValue = kResume,
  };
  using Controls =
      base::EnumSet<Control, Control::kMinValue, Control::kMaxValue>;

  // A line of the task's worklog: what the task is doing or just did.
  struct WorklogEntry {
    // When the task reported this entry.
    base::Time time;
    // What the task reported, ready to show.
    std::string text;
  };

  // What the task needs from the user before it can continue: a confirmation,
  // a sign-in, a reply, an action on the page, or something else. The
  // task writes it, not the user. Responding to it is a separate action, added
  // once the task protocol for it is agreed.
  struct UserRequest {
    enum class Kind {
      // Approve or reject an action before it runs.
      kConfirmation,
      // Answer in the surface, for example with text or a choice. The page
      // stays with the task.
      kReply,
      // Do something on the page yourself, then hand control back. Surfaces
      // offer `TakeOver()` for it.
      kActOnPage,
      // A common case of `kActOnPage` with its own copy: sign in to the site,
      // on the page or with saved credentials, then let the task continue.
      kSignIn,
      // Anything else the task needs from the user.
      kOther,
    };

    Kind kind = Kind::kOther;
    // Text from the task that explains what it needs, ready to show.
    std::string message;
  };

  // A snapshot of one task. Surfaces render it as is; they don't keep it.
  struct TaskView {
    TaskView();
    TaskView(const TaskView&);
    TaskView& operator=(const TaskView&);
    TaskView(TaskView&&) noexcept;
    TaskView& operator=(TaskView&&) noexcept;
    ~TaskView();

    // Identifies this task in the model.
    TaskUiId id;
    // Identifies who started the task, so surfaces can show its name and
    // icon. Opaque to this model.
    std::string caller_id;
    // The actor task that acts for this task on this device, if there is one.
    std::optional<TaskId> actor_task_id;
    // The server-side task ID, when the task came from the server.
    std::optional<std::string> server_task_id;

    // Where the task is in its life.
    Lifecycle lifecycle = Lifecycle::kStarting;
    // A caller-defined step, for example "filling in the form", that a
    // surface maps to its own copy. Unset when the caller defines no steps.
    std::optional<std::string> phase;
    // Set only while `lifecycle` is `Lifecycle::kNeedsUser`.
    std::optional<UserRequest> user_request;
    // A caller-defined outcome code, set once the task ends, for surfaces
    // that show more than success or failure.
    std::optional<std::string> outcome_code;
    // Oldest first.
    std::vector<WorklogEntry> worklog;

    // The surface the task most recently acted in on this device: a tab, or a
    // headless page that isn't in any tab (`GetTabHandle()` is then null). Kept
    // after the task ends. Null when the task has no surface on this device.
    ActorSurfaceHandle actor_surface_handle;

    // When the task was requested.
    base::Time start_time;

    // Which controls the task allows right now, given its lifecycle and its
    // caller. Surfaces offer only these. Empty when none apply.
    Controls allowed_controls;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnTaskAdded(const TaskView& task) {}
    // Called whenever any field of `task` changes, including a new worklog
    // entry.
    virtual void OnTaskUpdated(const TaskView& task) {}
    // The model no longer lists the task, for example after a finished task
    // ages out.
    virtual void OnTaskRemoved(TaskUiId id) {}
  };

  virtual ~ActorTaskUiModel() = default;

  // Running tasks and recently finished ones, most recently started first.
  virtual std::vector<TaskView> GetTasks() const = 0;
  virtual std::optional<TaskView> GetTask(TaskUiId id) const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Controls. Each one acts on the task, not on a surface, and does nothing
  // unless `TaskView::allowed_controls` contains it. The result arrives
  // asynchronously, as a lifecycle change through `Observer::OnTaskUpdated()`.

  // Ends the task as cancelled.
  virtual void Stop(TaskUiId id) = 0;
  // Pauses the task without giving the user its page. The task waits until
  // `Resume()`.
  virtual void Pause(TaskUiId id) = 0;
  // Gives the user control of the task's tab and brings it to the front. For a
  // headless task, only a surface the actor layer owns can be moved into a
  // tab; otherwise the caller decides how to show the page.
  virtual void TakeOver(TaskUiId id) = 0;
  // Hands control back to the task after `kPaused`, `kNeedsUser`, or
  // `kUserInControl`.
  virtual void Resume(TaskUiId id) = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TASK_UI_ACTOR_TASK_UI_MODEL_H_

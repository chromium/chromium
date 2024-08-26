// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_glib.h"

#include <fcntl.h>
#include <glib.h>
#include <math.h>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"

namespace base {

namespace {

// Priorities of event sources are important to let everything be processed.
// In particular, GTK event source should have the highest priority (because
// UI events come from it), then Wayland events (the ones coming from the FD
// watcher), and the lowest priority is GLib events (our base message pump).
//
// The g_source API uses ints to denote priorities, and the lower is its value,
// the higher is the priority (i.e., they are ordered backwards).
constexpr int kPriorityWork = G_PRIORITY_DEFAULT_IDLE;
constexpr int kPriorityFdWatch = G_PRIORITY_DEFAULT_IDLE - 10;

// See the explanation above.
static_assert(G_PRIORITY_DEFAULT < kPriorityFdWatch &&
                  kPriorityFdWatch < kPriorityWork,
              "Wrong priorities are set for event sources!");

// Return a timeout suitable for the glib loop according to |next_task_time|, -1
// to block forever, 0 to return right away, or a timeout in milliseconds from
// now.
int GetTimeIntervalMilliseconds(TimeTicks next_task_time) {
  if (next_task_time.is_null())
    return 0;
  else if (next_task_time.is_max())
    return -1;

  auto timeout_ms =
      (next_task_time - TimeTicks::Now()).InMillisecondsRoundedUp();

  return timeout_ms < 0 ? 0 : saturated_cast<int>(timeout_ms);
}

bool RunningOnMainThread() {
  auto pid = getpid();
  auto tid = PlatformThread::CurrentId();
  return pid > 0 && tid > 0 && pid == tid;
}

// A brief refresher on GLib:
//     GLib sources have four callbacks: Prepare, Check, Dispatch and Finalize.
// On each iteration of the GLib pump, it calls each source's Prepare function.
// This function should return TRUE if it wants GLib to call its Dispatch, and
// FALSE otherwise.  It can also set a timeout in this case for the next time
// Prepare should be called again (it may be called sooner).
//     After the Prepare calls, GLib does a poll to check for events from the
// system.  File descriptors can be attached to the sources.  The poll may block
// if none of the Prepare calls returned TRUE.  It will block indefinitely, or
// by the minimum time returned by a source in Prepare.
//     After the poll, GLib calls Check for each source that returned FALSE
// from Prepare.  The return value of Check has the same meaning as for Prepare,
// making Check a second chance to tell GLib we are ready for Dispatch.
//     Finally, GLib calls Dispatch for each source that is ready.  If Dispatch
// returns FALSE, GLib will destroy the source.  Dispatch calls may be recursive
// (i.e., you can call Run from them), but Prepare and Check cannot.
//     Finalize is called when the source is destroyed.
// NOTE: It is common for subsystems to want to process pending events while
// doing intensive work, for example the flash plugin. They usually use the
// following pattern (recommended by the GTK docs):
// while (gtk_events_pending()) {
//   gtk_main_iteration();
// }
//
// gtk_events_pending just calls g_main_context_pending, which does the
// following:
// - Call prepare on all the sources.
// - Do the poll with a timeout of 0 (not blocking).
// - Call check on all the sources.
// - *Does not* call dispatch on the sources.
// - Return true if any of prepare() or check() returned true.
//
// gtk_main_iteration just calls g_main_context_iteration, which does the whole
// thing, respecting the timeout for the poll (and block, although it is to if
// gtk_events_pending returned true), and call dispatch.
//
// Thus it is important to only return true from prepare or check if we
// actually have events or work to do. We also need to make sure we keep
// internal state consistent so that if prepare/check return true when called
// from gtk_events_pending, they will still return true when called right
// after, from gtk_main_iteration.
//
// For the GLib pump we try to follow the Windows UI pump model:
// - Whenever we receive a wakeup event or the timer for delayed work expires,
// we run DoWork. That part will also run in the other event pumps.
// - We also run DoWork, and possibly DoIdleWork, in the main loop,
// around event handling.
//
// ---------------------------------------------------------------------------
//
// An overview on the way that we track work items:
//
//     ScopedDoWorkItems are used by this pump to track native work. They are
// stored by value in |state_| and are set/cleared as the pump runs. Their
// setting and clearing is done in the functions
// {Set,Clear,EnsureSet,EnsureCleared}ScopedWorkItem. Control flow in GLib is
// quite non-obvious because chrome is not notified when a nested loop is
// entered/exited. To detect nested loops, MessagePumpGlib uses
// |state_->do_work_depth| which is incremented when DoWork is entered, and a
// GLib library function, g_main_depth(), which indicates the current number of
// Dispatch() calls on the stack. To react to them, two separate
// ScopedDoWorkItems are used (a standard one used for all native work, and a
// second one used exclusively for forcing nesting when there is a native loop
// spinning).  Note that `ThreadController` flags all nesting as
// `Phase::kNested` so separating native and application work while nested isn't
// supported nor a goal.
//
//     It should also be noted that a second GSource has been added to GLib,
// referred to as the "observer" source. It is used because in the case where
// native work occurs on wakeup that is higher priority than Chrome (all of
// GTK), chrome won't even get notified that the pump is awake.
//
//     There are several cases to consider wrt. nesting level and order. In
// order, we have:
// A. [root] -> MessagePump::Run() -> native event -> g_main_context_iteration
// B. [root] -> MessagePump::Run() -> DoWork -> g_main_context_iteration
// C. [root] -> native -> DoWork -> MessagePump -> [...]
// The second two cases are identical for our purposes, and the last one turns
// out to be handled without any extra headache.
//
//     Consider nesting case A, where native work is called from
// |g_main_context_iteration()| from the pump, and that native work spins up a
// loop. For our purposes, this is a nested loop, because control is not
// returned to the pump once one iteration of the pump is complete. In this
// case, the pump needs to enter nesting without DoWork being involved at
// all. This is accomplished using |MessagePumpGlib::NestIfRequired()|, which is
// called during the Prepare() phase of GLib. As the pump records state on entry
// and exit from GLib using |OnEntryToGlib| and |OnExitFromGlib|, we can compare
// |g_main_depth| at |HandlePrepare| with the one before we entered
// |g_main_context_iteration|. If it is higher, there is a native loop being
// spun, and |RegisterNesting| is called, forcing nesting by initializing two
// work items at once. These are destroyed after the exit from
// |g_main_context_iteration| using |OnExitFromGlib|.
//
//     Then, considering nesting case B, |state_->do_work_depth| is incremented
// during any Chrome work, to allow the pump to detect re-entrancy during a
// chrome work item. This is required because `g_main_depth` is not incremented
// in any `DoWork` call not occuring during `Dispatch()` (i.e. during
// `MessagePumpGlib::Run()`). In this case, a nested loop is recorded, and the
// pump sets-and-clears scoped work items during Prepare, Check, and Dispatch. A
// work item can never be active when control flow returns to GLib (i.e. on
// return) during a nested loop, because the nested loop could exit at any
// point. This is fine because TimeKeeper is only concerned with the fact that a
// nested loop is in progress, as opposed to the various phases of the nested
// loop.
//
//     Finally, consider nesting case C, where a native loop is spinning
// entirely outside of Chrome, such as inside a signal handler, the pump might
// create and destroy DoWorkItems during Prepare() and Check(), but these work
// items will always get cleared during Dispatch(), before the pump enters a
// DoWork(), leading to the pump showing non-nested native work without the
// thread controller being active, the correct situation (which won't occur
// outside of startup or shutdown).  Once Dispatch() is called, the pump's
// nesting tracking works correctly, as state_->do_work_depth is increased, and
// upon re-entrancy we detect the nested loop, which is correct, as this is the
// only point at which the loop actually becomes "nested".
//
// -----------------------------------------------------------------------------
//
// As an overview of the steps taken by MessagePumpGLib to ensure that nested
// loops are detected adequately during each phase of the GLib loop:
//
// 0: Before entering GLib:
// 0.1: Record state about current state of GLib (g_main_depth()) for
// case 1.1.2.
//
// 1: Prepare.
// 1.1: Detection of nested loops

// 1.1.1: If |state_->do_work_depth| > 0, we are in nesting case B detailed
//        above. A work item must be newly created during this function to
//        trigger nesting, and is destroyed to ensure proper destruction order
//        in the case where GLib quits after Prepare().
//
// 1.1.2: Otherwise, check if we are in nesting case A above. If yes, trigger
//        nesting using ScopedDoWorkItems. The nesting will be cleared at exit
//        from GLib.
//
//        This check occurs only in |HandleObserverPrepare|, not in
//        |HandlePrepare|.
//
//        A third party is running a glib message loop. Since Chrome work is
//        registered with GLib at |G_PRIORITY_DEFAULT_IDLE|, a relatively low
//        priority, sources of default-or-higher priority will be Dispatch()ed
//        first. Since only one source is Dispatched per loop iteration,
//        |HandlePrepare| can get called several times in a row in the case that
//        there are any other events in the queue. A ScopedDoWorkItem is created
//        and destroyed to record this. That work item triggers nesting.
//
// 1.2: Other considerations
// 1.2.1: Sleep occurs between Prepare() and Check(). If Chrome will pass a
//        nonzero poll time to GLib, the inner ScopedDoWorkItem is cleared and
//        BeforeWait() is called. In nesting case A, the nesting work item will
//        not be cleared. A nested loop will typically not block.
//
//        Since Prepare() is called before Check() in all cases, the bulk of
//        nesting detection is done in Prepare().
//
// 2: Check.
// 2.1: Detection of nested loops:
// 2.1.1: In nesting case B, |ClearScopedWorkItem()| on exit.  A third party is
//        running a glib message loop. It is possible that at any point the
//        nested message loop will quit. In this case, we don't want to leave a
//        nested DoWorkItem on the stack.
//
// 2.2: Other considerations
// 2.2.1: A ScopedDoWorkItem may be created (if it was not already present) at
//        the entry to Check() to record a wakeup in the case that the pump
//        slept. It is important to note that this occurs both in
//        |HandleObserverCheck| and |HandleCheck| to ensure that at every point
//        as the pump enters the Dispatch phase it is awake. In the case it is
//        already awake, this is a very cheap operation.
//
// 3: Dispatch
// 3.1 Detection of nested loops
// 3.1.1: |state_->do_work_depth| is incremented on entry and decremented on
//        exit. This is used to detect nesting case B.
//
// 3.1.2: Nested loops can be quit at any point, and so ScopedDoWorkItems can't
//        be left on the stack for the same reasons as in 1.1.1/2.1.1.
//
// 3.2 Other considerations
// 3.2.1: Since DoWork creates its own work items, ScopedDoWorkItems are not
//        used as this would trigger nesting in all cases.
//
// 4: Post GLib
// 4.1: Detection of nested loops
// 4.1.1: |state_->do_work_depth| is also increased during the DoWork in Run()
//        as nesting in that case [calling glib from third party code] needs to
//        clear all work items after return to avoid improper destruction order.
//
// 4.2: Other considerations:
// 4.2.1: DoWork uses its own work item, so no ScopedDoWorkItems are active in
//        this case.

struct WorkSource : public GSource {
  raw_ptr<MessagePumpGlib> pump;
};

gboolean WorkSourcePrepare(GSource* source, gint* timeout_ms) {
  *timeout_ms = static_cast<WorkSource*>(source)->pump->HandlePrepare();
  // We always return FALSE, so that our timeout is honored.  If we were
  // to return TRUE, the timeout would be considered to be 0 and the poll
  // would never block.  Once the poll is finished, Check will be called.
  return FALSE;
}

gboolean WorkSourceCheck(GSource* source) {
  // Only return TRUE if Dispatch should be called.
  return static_cast<WorkSource*>(source)->pump->HandleCheck();
}

gboolean WorkSourceDispatch(GSource* source,
                            GSourceFunc unused_func,
                            gpointer unused_data) {
  static_cast<WorkSource*>(source)->pump->HandleDispatch();
  // Always return TRUE so our source stays registered.
  return TRUE;
}

void WorkSourceFinalize(GSource* source) {
  // Since the WorkSource object memory is managed by glib, WorkSource implicit
  // destructor is never called, and thus WorkSource's raw_ptr never release
  // its internal reference on the pump pointer. This leads to adding pressure
  // to the BRP quarantine.
  static_cast<WorkSource*>(source)->pump = nullptr;
}

// I wish these could be const, but g_source_new wants non-const.
GSourceFuncs g_work_source_funcs = {WorkSourcePrepare, WorkSourceCheck,
                                    WorkSourceDispatch, WorkSourceFinalize};

struct ObserverSource : public GSource {
  raw_ptr<MessagePumpGlib> pump;
};

gboolean ObserverPrepare(GSource* gsource, gint* timeout_ms) {
  auto* source = static_cast<ObserverSource*>(gsource);
  source->pump->HandleObserverPrepare();
  *timeout_ms = -1;
  // We always want to poll.
  return FALSE;
}

gboolean ObserverCheck(GSource* gsource) {
  auto* source = static_cast<ObserverSource*>(gsource);
  return source->pump->HandleObserverCheck();
}

void ObserverFinalize(GSource* source) {
  // Read the comment in `WorkSourceFinalize`, the issue is exactly the same.
  static_cast<ObserverSource*>(source)->pump = nullptr;
}

GSourceFuncs g_observer_funcs = {ObserverPrepare, ObserverCheck, nullptr,
                                 ObserverFinalize};

struct FdWatchSource : public GSource {
  raw_ptr<MessagePumpGlib> pump;
  raw_ptr<MessagePumpGlib::FdWatchController> controller;
};

gboolean FdWatchSourcePrepare(GSource* source, gint* timeout_ms) {
  *timeout_ms = -1;
  return FALSE;
}

gboolean FdWatchSourceCheck(GSource* gsource) {
  auto* source = static_cast<FdWatchSource*>(gsource);
  return source->pump->HandleFdWatchCheck(source->controller) ? TRUE : FALSE;
}

gboolean FdWatchSourceDispatch(GSource* gsource,
                               GSourceFunc unused_func,
                               gpointer unused_data) {
  auto* source = static_cast<FdWatchSource*>(gsource);
  source->pump->HandleFdWatchDispatch(source->controller);
  return TRUE;
}

void FdWatchSourceFinalize(GSource* gsource) {
  // Read the comment in `WorkSourceFinalize`, the issue is exactly the same.
  auto* source = static_cast<FdWatchSource*>(gsource);
  source->pump = nullptr;
  source->controller = nullptr;
}

GSourceFuncs g_fd_watch_source_funcs = {
    FdWatchSourcePrepare, FdWatchSourceCheck, FdWatchSourceDispatch,
    FdWatchSourceFinalize};

}  // namespace

struct MessagePumpGlib::RunState {
  explicit RunState(Delegate* delegate) : delegate(delegate) {
    CHECK(delegate);
  }

  const raw_ptr<Delegate> delegate;

  // Used to flag that the current Run() invocation should return ASAP.
  bool should_quit = false;

  // Keeps track of the number of calls to DoWork() on the stack for the current
  // Run() invocation. Used to detect reentrancy from DoWork in order to make
  // decisions about tracking nested work.
  int do_work_depth = 0;

  // Value of g_main_depth() captured before the call to
  // g_main_context_iteration() in Run(). nullopt if Run() is not calling
  // g_main_context_iteration(). Used to track whether the pump has forced a
  // nested state due to a native pump.
  std::optional<int> g_depth_on_iteration;

  // Used to keep track of the native event work items processed by the message
  // pump.
  Delegate::ScopedDoWorkItem scoped_do_work_item;

  // Used to force the pump into a nested state when a native runloop was
  // dispatched from main.
  Delegate::ScopedDoWorkItem native_loop_do_work_item;

  // The information of the next task available at this run-level. Stored in
  // RunState because different set of tasks can be accessible at various
  // run-levels (e.g. non-nestable tasks).
  Delegate::NextWorkInfo next_work_info;
};

MessagePumpGlib::MessagePumpGlib()
    : state_(nullptr), wakeup_gpollfd_(std::make_unique<GPollFD>()) {
  DCHECK(!g_main_context_get_thread_default());
  if (RunningOnMainThread()) {
    context_ = g_main_context_default();
  } else {
    owned_context_ = std::unique_ptr<GMainContext, GMainContextDeleter>(
        g_main_context_new());
    context_ = owned_context_.get();
    g_main_context_push_thread_default(context_);
  }

  // Create our wakeup pipe, which is used to flag when work was scheduled.
  int fds[2];
  [[maybe_unused]] int ret = pipe2(fds, O_CLOEXEC);
  DCHECK_EQ(ret, 0);

  wakeup_pipe_read_ = fds[0];
  wakeup_pipe_write_ = fds[1];
  wakeup_gpollfd_->fd = wakeup_pipe_read_;
  wakeup_gpollfd_->events = G_IO_IN;

  observer_source_ = std::unique_ptr<GSource, GSourceDeleter>(
      g_source_new(&g_observer_funcs, sizeof(ObserverSource)));
  static_cast<ObserverSource*>(observer_source_.get())->pump = this;
  g_source_attach(observer_source_.get(), context_);

  work_source_ = std::unique_ptr<GSource, GSourceDeleter>(
      g_source_new(&g_work_source_funcs, sizeof(WorkSource)));
  static_cast<WorkSource*>(work_source_.get())->pump = this;
  g_source_add_poll(work_source_.get(), wakeup_gpollfd_.get());
  g_source_set_priority(work_source_.get(), kPriorityWork);
  // This is needed to allow Run calls inside Dispatch.
  g_source_set_can_recurse(work_source_.get(), TRUE);
  g_source_attach(work_source_.get(), context_);
}

MessagePumpGlib::~MessagePumpGlib() {
  work_source_.reset();
  close(wakeup_pipe_read_);
  close(wakeup_pipe_write_);
  context_ = nullptr;
  owned_context_.reset();
}

MessagePumpGlib::FdWatchController::FdWatchController(const Location& location)
    : FdWatchControllerInterface(location) {}

MessagePumpGlib::FdWatchController::~FdWatchController() {
  if (IsInitialized()) {
    auto* source = static_cast<FdWatchSource*>(source_);
    source->controller = nullptr;

    CHECK(StopWatchingFileDescriptor());
  }
  if (was_destroyed_) {
    DCHECK(!*was_destroyed_);
    *was_destroyed_ = true;
  }
}

bool MessagePumpGlib::FdWatchController::StopWatchingFileDescriptor() {
  if (!IsInitialized())
    return false;

  g_source_destroy(source_);
  g_source_unref(source_.ExtractAsDangling());
  watcher_ = nullptr;
  return true;
}

bool MessagePumpGlib::FdWatchController::IsInitialized() const {
  return !!source_;
}

bool MessagePumpGlib::FdWatchController::InitOrUpdate(int fd,
                                                      int mode,
                                                      FdWatcher* watcher) {
  gushort event_flags = 0;
  if (mode & WATCH_READ) {
    event_flags |= G_IO_IN;
  }
  if (mode & WATCH_WRITE) {
    event_flags |= G_IO_OUT;
  }

  if (!IsInitialized()) {
    poll_fd_ = std::make_unique<GPollFD>();
    poll_fd_->fd = fd;
  } else {
    if (poll_fd_->fd != fd)
      return false;
    // Combine old/new event masks.
    event_flags |= poll_fd_->events;
    // Destroy previous source
    bool stopped = StopWatchingFileDescriptor();
    DCHECK(stopped);
  }
  poll_fd_->events = event_flags;
  poll_fd_->revents = 0;

  source_ = g_source_new(&g_fd_watch_source_funcs, sizeof(FdWatchSource));
  DCHECK(source_);
  g_source_add_poll(source_, poll_fd_.get());
  g_source_set_can_recurse(source_, TRUE);
  g_source_set_callback(source_, nullptr, nullptr, nullptr);
  g_source_set_priority(source_, kPriorityFdWatch);

  watcher_ = watcher;
  return true;
}

bool MessagePumpGlib::FdWatchController::Attach(MessagePumpGlib* pump) {
  DCHECK(pump);
  if (!IsInitialized()) {
    return false;
  }
  auto* source = static_cast<FdWatchSource*>(source_);
  source->controller = this;
  source->pump = pump;
  g_source_attach(source_, pump->context_);
  return true;
}

void MessagePumpGlib::FdWatchController::NotifyCanRead() {
  if (!watcher_)
    return;
  DCHECK(poll_fd_);
  watcher_->OnFileCanReadWithoutBlocking(poll_fd_->fd);
}

void MessagePumpGlib::FdWatchController::NotifyCanWrite() {
  if (!watcher_)
    return;
  DCHECK(poll_fd_);
  watcher_->OnFileCanWriteWithoutBlocking(poll_fd_->fd);
}

bool MessagePumpGlib::WatchFileDescriptor(int fd,
                                          bool persistent,
                                          int mode,
                                          FdWatchController* controller,
                                          FdWatcher* watcher) {
  DCHECK_GE(fd, 0);
  DCHECK(controller);
  DCHECK(watcher);
  DCHECK(mode == WATCH_READ || mode == WATCH_WRITE || mode == WATCH_READ_WRITE);
  // WatchFileDescriptor should be called on the pump thread. It is not
  // threadsafe, so the watcher may never be registered.
  DCHECK_CALLED_ON_VALID_THREAD(watch_fd_caller_checker_);

  if (!controller->InitOrUpdate(fd, mode, watcher)) {
    DPLOG(ERROR) << "FdWatchController init failed (fd=" << fd << ")";
    return false;
  }
  return controller->Attach(this);
}

void MessagePumpGlib::HandleObserverPrepare() {
  // |state_| may be null during tests.
  if (!state_) {
    return;
  }

  if (state_->do_work_depth > 0) {
    // Contingency 1.1.1 detailed above
    SetScopedWorkItem();
    ClearScopedWorkItem();
  } else {
    // Contingency 1.1.2 detailed above
    NestIfRequired();
  }

  return;
}

bool MessagePumpGlib::HandleObserverCheck() {
  // |state_| may be null in tests.
  if (!state_) {
    return FALSE;
  }

  // Make sure we record the fact that we're awake. Chrome won't get Check()ed
  // if a higher priority work item returns TRUE from Check().
  EnsureSetScopedWorkItem();
  if (state_->do_work_depth > 0) {
    // Contingency 2.1.1
    ClearScopedWorkItem();
  }

  // The observer never needs to run anything.
  return FALSE;
}

// Return the timeout we want passed to poll.
int MessagePumpGlib::HandlePrepare() {
  // |state_| may be null during tests.
  if (!state_)
    return 0;

  const int next_wakeup_millis =
      GetTimeIntervalMilliseconds(state_->next_work_info.delayed_run_time);
  if (next_wakeup_millis != 0) {
    // When this is called, it is not possible to know for sure if a
    // ScopedWorkItem is on the stack, because HandleObserverCheck may have set
    // it during an iteration of the pump where a high priority native work item
    // executed.
    EnsureClearedScopedWorkItem();
    state_->delegate->BeforeWait();
  }

  return next_wakeup_millis;
}

bool MessagePumpGlib::HandleCheck() {
  if (!state_)  // state_ may be null during tests.
    return false;

  // Ensure pump is awake.
  EnsureSetScopedWorkItem();

  if (state_->do_work_depth > 0) {
    // Contingency 2.1.1
    ClearScopedWorkItem();
  }

  // We usually have a single message on the wakeup pipe, since we are only
  // signaled when the queue went from empty to non-empty, but there can be
  // two messages if a task posted a task, hence we read at most two bytes.
  // The glib poll will tell us whether there was data, so this read
  // shouldn't block.
  if (wakeup_gpollfd_->revents & G_IO_IN) {
    char msg[2];
    const long num_bytes = HANDLE_EINTR(read(wakeup_pipe_read_, msg, 2));
    if (num_bytes < 1) {
      NOTREACHED() << "Error reading from the wakeup pipe.";
    }
    DCHECK((num_bytes == 1 && msg[0] == '!') ||
           (num_bytes == 2 && msg[0] == '!' && msg[1] == '!'));
    // Since we ate the message, we need to record that we have immediate work,
    // because HandleCheck() may be called without HandleDispatch being called
    // afterwards.
    state_->next_work_info = {TimeTicks()};
    return true;
  }

  // As described in the summary at the top : Check is a second-chance to
  // Prepare, verify whether we have work ready again.
  if (GetTimeIntervalMilliseconds(state_->next_work_info.delayed_run_time) ==
      0) {
    return true;
  }

  return false;
}

void MessagePumpGlib::HandleDispatch() {
  // Contingency 3.2.1
  EnsureClearedScopedWorkItem();

  // Contingency 3.1.1
  ++state_->do_work_depth;
  state_->next_work_info = state_->delegate->DoWork();
  --state_->do_work_depth;

  if (state_ && state_->do_work_depth > 0) {
    // Contingency 3.1.2
    EnsureClearedScopedWorkItem();
  }
}

void MessagePumpGlib::Run(Delegate* delegate) {
  RunState state(delegate);

  RunState* previous_state = state_;
  state_ = &state;

  // We really only do a single task for each iteration of the loop.  If we
  // have done something, assume there is likely something more to do.  This
  // will mean that we don't block on the message pump until there was nothing
  // more to do.  We also set this to true to make sure not to block on the
  // first iteration of the loop, so RunUntilIdle() works correctly.
  bool more_work_is_plausible = true;

  // We run our own loop instead of using g_main_loop_quit in one of the
  // callbacks.  This is so we only quit our own loops, and we don't quit
  // nested loops run by others.  TODO(deanm): Is this what we want?
  for (;;) {
    // ScopedWorkItem to account for any native work until the runloop starts
    // running chrome work.
    SetScopedWorkItem();

    // Don't block if we think we have more work to do.
    bool block = !more_work_is_plausible;

    OnEntryToGlib();
    more_work_is_plausible = g_main_context_iteration(context_, block);
    OnExitFromGlib();

    if (state_->should_quit)
      break;

    // Contingency 4.2.1
    EnsureClearedScopedWorkItem();

    // Contingency 4.1.1
    ++state_->do_work_depth;
    state_->next_work_info = state_->delegate->DoWork();
    --state_->do_work_depth;

    more_work_is_plausible |= state_->next_work_info.is_immediate();
    if (state_->should_quit)
      break;

    if (more_work_is_plausible)
      continue;

    state_->delegate->DoIdleWork();
    if (state_->should_quit)
      break;
  }

  state_ = previous_state;
}

void MessagePumpGlib::Quit() {
  if (state_) {
    state_->should_quit = true;
  } else {
    NOTREACHED() << "Quit called outside Run!";
  }
}

void MessagePumpGlib::ScheduleWork() {
  // This can be called on any thread, so we don't want to touch any state
  // variables as we would then need locks all over.  This ensures that if
  // we are sleeping in a poll that we will wake up.
  char msg = '!';
  if (HANDLE_EINTR(write(wakeup_pipe_write_, &msg, 1)) != 1) {
    NOTREACHED() << "Could not write to the UI message loop wakeup pipe!";
  }
}

void MessagePumpGlib::ScheduleDelayedWork(
    const Delegate::NextWorkInfo& next_work_info) {
  // We need to wake up the loop in case the poll timeout needs to be
  // adjusted.  This will cause us to try to do work, but that's OK.
  ScheduleWork();
}

bool MessagePumpGlib::HandleFdWatchCheck(FdWatchController* controller) {
  DCHECK(controller);
  gushort flags = controller->poll_fd_->revents;
  return (flags & G_IO_IN) || (flags & G_IO_OUT);
}

void MessagePumpGlib::HandleFdWatchDispatch(FdWatchController* controller) {
  DCHECK(controller);
  DCHECK(controller->poll_fd_);
  gushort flags = controller->poll_fd_->revents;
  if ((flags & G_IO_IN) && (flags & G_IO_OUT)) {
    // Both callbacks will be called. It is necessary to check that
    // |controller| is not destroyed.
    bool controller_was_destroyed = false;
    controller->was_destroyed_ = &controller_was_destroyed;
    controller->NotifyCanWrite();
    if (!controller_was_destroyed)
      controller->NotifyCanRead();
    if (!controller_was_destroyed)
      controller->was_destroyed_ = nullptr;
  } else if (flags & G_IO_IN) {
    controller->NotifyCanRead();
  } else if (flags & G_IO_OUT) {
    controller->NotifyCanWrite();
  }
}

bool MessagePumpGlib::ShouldQuit() const {
  CHECK(state_);
  return state_->should_quit;
}

void MessagePumpGlib::SetScopedWorkItem() {
  // |state_| can be null during tests
  if (!state_) {
    return;
  }
  // If there exists a ScopedDoWorkItem in the current RunState, it cannot be
  // overwritten.
  CHECK(state_->scoped_do_work_item.IsNull());

  // In the case that we're more than two work items deep, don't bother tracking
  // individual native events anymore. Note that this won't cause out-of-order
  // end work items, because the work item is cleared before entering the second
  // DoWork().
  if (state_->do_work_depth < 2) {
    state_->scoped_do_work_item = state_->delegate->BeginWorkItem();
  }
}

void MessagePumpGlib::ClearScopedWorkItem() {
  // |state_| can be null during tests
  if (!state_) {
    return;
  }

  CHECK(!state_->scoped_do_work_item.IsNull());
  // See identical check in SetScopedWorkItem
  if (state_->do_work_depth < 2) {
    state_->scoped_do_work_item = Delegate::ScopedDoWorkItem();
  }
}

void MessagePumpGlib::EnsureSetScopedWorkItem() {
  // |state_| can be null during tests
  if (!state_) {
    return;
  }
  if (state_->scoped_do_work_item.IsNull()) {
    SetScopedWorkItem();
  }
}

void MessagePumpGlib::EnsureClearedScopedWorkItem() {
  // |state_| can be null during tests
  if (!state_) {
    return;
  }
  if (!state_->scoped_do_work_item.IsNull()) {
    ClearScopedWorkItem();
  }
}

void MessagePumpGlib::RegisterNested() {
  // |state_| can be null during tests
  if (!state_) {
    return;
  }
  CHECK(state_->native_loop_do_work_item.IsNull());

  // Transfer `scoped_do_work_item` to `native_do_work_item`, and so the
  // ephemeral `scoped_do_work_item` will be coming in and out of existence on
  // top of `native_do_work_item`, whose state hasn't been deleted.

  if (state_->scoped_do_work_item.IsNull()) {
    state_->native_loop_do_work_item = state_->delegate->BeginWorkItem();
  } else {
    // This clears state_->scoped_do_work_item.
    state_->native_loop_do_work_item = std::move(state_->scoped_do_work_item);
  }
  SetScopedWorkItem();
  ClearScopedWorkItem();
}

void MessagePumpGlib::UnregisterNested() {
  // |state_| can be null during tests
  if (!state_) {
    return;
  }
  CHECK(!state_->native_loop_do_work_item.IsNull());

  EnsureClearedScopedWorkItem();
  // Nesting exits here.
  state_->native_loop_do_work_item = Delegate::ScopedDoWorkItem();
}

void MessagePumpGlib::NestIfRequired() {
  // |state_| can be null during tests
  if (!state_) {
    return;
  }
  if (state_->native_loop_do_work_item.IsNull() &&
      state_->g_depth_on_iteration.has_value() &&
      g_main_depth() != state_->g_depth_on_iteration.value()) {
    RegisterNested();
  }
}

void MessagePumpGlib::UnnestIfRequired() {
  // |state_| can be null during tests
  if (!state_) {
    return;
  }
  if (!state_->native_loop_do_work_item.IsNull()) {
    UnregisterNested();
  }
}

void MessagePumpGlib::OnEntryToGlib() {
  // |state_| can be null during tests
  if (!state_) {
    return;
  }
  CHECK(!state_->g_depth_on_iteration.has_value());
  state_->g_depth_on_iteration.emplace(g_main_depth());
}

void MessagePumpGlib::OnExitFromGlib() {
  // |state_| can be null during tests
  if (!state_) {
    return;
  }
  state_->g_depth_on_iteration.reset();
  UnnestIfRequired();
}

}  // namespace base

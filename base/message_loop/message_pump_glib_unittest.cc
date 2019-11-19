// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_glib.h"

#include <glib.h>
#include <math.h>

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_type.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

// This class injects dummy "events" into the GLib loop. When "handled" these
// events can run tasks. This is intended to mock gtk events (the corresponding
// GLib source runs at the same priority).
class EventInjector {
 public:
  EventInjector() : processed_events_(0) {
    source_ = static_cast<Source*>(g_source_new(&SourceFuncs, sizeof(Source)));
    source_->injector = this;
    g_source_attach(source_, nullptr);
    g_source_set_can_recurse(source_, TRUE);
  }

  ~EventInjector() {
    g_source_destroy(source_);
    g_source_unref(source_);
  }

  int HandlePrepare() {
    // If the queue is empty, block.
    if (events_.empty())
      return -1;
    TimeDelta delta = events_[0].time - Time::NowFromSystemTime();
    return std::max(0, static_cast<int>(ceil(delta.InMillisecondsF())));
  }

  bool HandleCheck() {
    if (events_.empty())
      return false;
    return events_[0].time <= Time::NowFromSystemTime();
  }

  void HandleDispatch() {
    if (events_.empty())
      return;
    Event event = std::move(events_[0]);
    events_.erase(events_.begin());
    ++processed_events_;
    if (!event.callback.is_null())
      std::move(event.callback).Run();
    else if (!event.task.is_null())
      std::move(event.task).Run();
  }

  // Adds an event to the queue. When "handled", executes |callback|.
  // delay_ms is relative to the last event if any, or to Now() otherwise.
  void AddEvent(int delay_ms, OnceClosure callback) {
    AddEventHelper(delay_ms, std::move(callback), OnceClosure());
  }

  void AddDummyEvent(int delay_ms) {
    AddEventHelper(delay_ms, OnceClosure(), OnceClosure());
  }

  void AddEventAsTask(int delay_ms, OnceClosure task) {
    AddEventHelper(delay_ms, OnceClosure(), std::move(task));
  }

  void Reset() {
    processed_events_ = 0;
    events_.clear();
  }

  int processed_events() const { return processed_events_; }

 private:
  struct Event {
    Time time;
    OnceClosure callback;
    OnceClosure task;
  };

  struct Source : public GSource {
    EventInjector* injector;
  };

  void AddEventHelper(int delay_ms, OnceClosure callback, OnceClosure task) {
    Time last_time;
    if (!events_.empty())
      last_time = (events_.end()-1)->time;
    else
      last_time = Time::NowFromSystemTime();

    Time future = last_time + TimeDelta::FromMilliseconds(delay_ms);
    EventInjector::Event event = {future, std::move(callback), std::move(task)};
    events_.push_back(std::move(event));
  }

  static gboolean Prepare(GSource* source, gint* timeout_ms) {
    *timeout_ms = static_cast<Source*>(source)->injector->HandlePrepare();
    return FALSE;
  }

  static gboolean Check(GSource* source) {
    return static_cast<Source*>(source)->injector->HandleCheck();
  }

  static gboolean Dispatch(GSource* source,
                           GSourceFunc unused_func,
                           gpointer unused_data) {
    static_cast<Source*>(source)->injector->HandleDispatch();
    return TRUE;
  }

  Source* source_;
  std::vector<Event> events_;
  int processed_events_;
  static GSourceFuncs SourceFuncs;
  DISALLOW_COPY_AND_ASSIGN(EventInjector);
};

GSourceFuncs EventInjector::SourceFuncs = {EventInjector::Prepare,
                                           EventInjector::Check,
                                           EventInjector::Dispatch, nullptr};

void IncrementInt(int *value) {
  ++*value;
}

// Checks how many events have been processed by the injector.
void ExpectProcessedEvents(EventInjector* injector, int count) {
  EXPECT_EQ(injector->processed_events(), count);
}

// Posts a task on the current message loop.
void PostMessageLoopTask(const Location& from_here, OnceClosure task) {
  ThreadTaskRunnerHandle::Get()->PostTask(from_here, std::move(task));
}

// Test fixture.
class MessagePumpGLibTest : public testing::Test {
 public:
  MessagePumpGLibTest() : loop_(nullptr), injector_(nullptr) {}

  // Overridden from testing::Test:
  void SetUp() override {
    loop_ = new MessageLoop(MessagePumpType::UI);
    injector_ = new EventInjector();
  }
  void TearDown() override {
    delete injector_;
    injector_ = nullptr;
    delete loop_;
    loop_ = nullptr;
  }

  MessageLoop* loop() const { return loop_; }
  EventInjector* injector() const { return injector_; }

 private:
  MessageLoop* loop_;
  EventInjector* injector_;
  DISALLOW_COPY_AND_ASSIGN(MessagePumpGLibTest);
};

}  // namespace

TEST_F(MessagePumpGLibTest, TestQuit) {
  // Checks that Quit works and that the basic infrastructure is working.

  // Quit from a task
  RunLoop().RunUntilIdle();
  EXPECT_EQ(0, injector()->processed_events());

  injector()->Reset();
  // Quit from an event
  RunLoop run_loop;
  injector()->AddEvent(0, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(1, injector()->processed_events());
}

TEST_F(MessagePumpGLibTest, TestEventTaskInterleave) {
  // Checks that tasks posted by events are executed before the next event if
  // the posted task queue is empty.
  // MessageLoop doesn't make strong guarantees that it is the case, but the
  // current implementation ensures it and the tests below rely on it.
  // If changes cause this test to fail, it is reasonable to change it, but
  // TestWorkWhileWaitingForEvents and TestEventsWhileWaitingForWork have to be
  // changed accordingly, otherwise they can become flaky.
  injector()->AddEventAsTask(0, DoNothing());
  OnceClosure check_task =
      BindOnce(&ExpectProcessedEvents, Unretained(injector()), 2);
  OnceClosure posted_task =
      BindOnce(&PostMessageLoopTask, FROM_HERE, std::move(check_task));
  injector()->AddEventAsTask(0, std::move(posted_task));
  injector()->AddEventAsTask(0, DoNothing());
  {
    RunLoop run_loop;
    injector()->AddEvent(0, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(4, injector()->processed_events());

  injector()->Reset();
  injector()->AddEventAsTask(0, DoNothing());
  check_task = BindOnce(&ExpectProcessedEvents, Unretained(injector()), 2);
  posted_task =
      BindOnce(&PostMessageLoopTask, FROM_HERE, std::move(check_task));
  injector()->AddEventAsTask(0, std::move(posted_task));
  injector()->AddEventAsTask(10, DoNothing());
  {
    RunLoop run_loop;
    injector()->AddEvent(0, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(4, injector()->processed_events());
}

TEST_F(MessagePumpGLibTest, TestWorkWhileWaitingForEvents) {
  int task_count = 0;
  // Tests that we process tasks while waiting for new events.
  // The event queue is empty at first.
  for (int i = 0; i < 10; ++i) {
    loop()->task_runner()->PostTask(FROM_HERE,
                                    BindOnce(&IncrementInt, &task_count));
  }
  // After all the previous tasks have executed, enqueue an event that will
  // quit.
  {
    RunLoop run_loop;
    loop()->task_runner()->PostTask(
        FROM_HERE, BindOnce(&EventInjector::AddEvent, Unretained(injector()), 0,
                            run_loop.QuitClosure()));
    run_loop.Run();
  }
  ASSERT_EQ(10, task_count);
  EXPECT_EQ(1, injector()->processed_events());

  // Tests that we process delayed tasks while waiting for new events.
  injector()->Reset();
  task_count = 0;
  for (int i = 0; i < 10; ++i) {
    loop()->task_runner()->PostDelayedTask(FROM_HERE,
                                           BindOnce(&IncrementInt, &task_count),
                                           TimeDelta::FromMilliseconds(10 * i));
  }
  // After all the previous tasks have executed, enqueue an event that will
  // quit.
  // This relies on the fact that delayed tasks are executed in delay order.
  // That is verified in message_loop_unittest.cc.
  {
    RunLoop run_loop;
    loop()->task_runner()->PostDelayedTask(
        FROM_HERE,
        BindOnce(&EventInjector::AddEvent, Unretained(injector()), 0,
                 run_loop.QuitClosure()),
        TimeDelta::FromMilliseconds(150));
    run_loop.Run();
  }
  ASSERT_EQ(10, task_count);
  EXPECT_EQ(1, injector()->processed_events());
}

TEST_F(MessagePumpGLibTest, TestEventsWhileWaitingForWork) {
  // Tests that we process events while waiting for work.
  // The event queue is empty at first.
  for (int i = 0; i < 10; ++i) {
    injector()->AddDummyEvent(0);
  }
  // After all the events have been processed, post a task that will check that
  // the events have been processed (note: the task executes after the event
  // that posted it has been handled, so we expect 11 at that point).
  OnceClosure check_task =
      BindOnce(&ExpectProcessedEvents, Unretained(injector()), 11);
  OnceClosure posted_task =
      BindOnce(&PostMessageLoopTask, FROM_HERE, std::move(check_task));
  injector()->AddEventAsTask(10, std::move(posted_task));

  // And then quit (relies on the condition tested by TestEventTaskInterleave).
  RunLoop run_loop;
  injector()->AddEvent(10, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(12, injector()->processed_events());
}

namespace {

// This class is a helper for the concurrent events / posted tasks test below.
// It will quit the main loop once enough tasks and events have been processed,
// while making sure there is always work to do and events in the queue.
class ConcurrentHelper : public RefCounted<ConcurrentHelper>  {
 public:
  ConcurrentHelper(EventInjector* injector, OnceClosure done_closure)
      : injector_(injector),
        done_closure_(std::move(done_closure)),
        event_count_(kStartingEventCount),
        task_count_(kStartingTaskCount) {}

  void FromTask() {
    if (task_count_ > 0) {
      --task_count_;
    }
    if (task_count_ == 0 && event_count_ == 0) {
      std::move(done_closure_).Run();
    } else {
      ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, BindOnce(&ConcurrentHelper::FromTask, this));
    }
  }

  void FromEvent() {
    if (event_count_ > 0) {
      --event_count_;
    }
    if (task_count_ == 0 && event_count_ == 0) {
      std::move(done_closure_).Run();
    } else {
      injector_->AddEventAsTask(0,
                                BindOnce(&ConcurrentHelper::FromEvent, this));
    }
  }

  int event_count() const { return event_count_; }
  int task_count() const { return task_count_; }

 private:
  friend class RefCounted<ConcurrentHelper>;

  ~ConcurrentHelper() {}

  static const int kStartingEventCount = 20;
  static const int kStartingTaskCount = 20;

  EventInjector* injector_;
  OnceClosure done_closure_;
  int event_count_;
  int task_count_;
};

}  // namespace

TEST_F(MessagePumpGLibTest, TestConcurrentEventPostedTask) {
  // Tests that posted tasks don't starve events, nor the opposite.
  // We use the helper class above. We keep both event and posted task queues
  // full, the helper verifies that both tasks and events get processed.
  // If that is not the case, either event_count_ or task_count_ will not get
  // to 0, and MessageLoop::QuitWhenIdle() will never be called.
  RunLoop run_loop;
  scoped_refptr<ConcurrentHelper> helper =
      new ConcurrentHelper(injector(), run_loop.QuitClosure());

  // Add 2 events to the queue to make sure it is always full (when we remove
  // the event before processing it).
  injector()->AddEventAsTask(0, BindOnce(&ConcurrentHelper::FromEvent, helper));
  injector()->AddEventAsTask(0, BindOnce(&ConcurrentHelper::FromEvent, helper));

  // Similarly post 2 tasks.
  loop()->task_runner()->PostTask(
      FROM_HERE, BindOnce(&ConcurrentHelper::FromTask, helper));
  loop()->task_runner()->PostTask(
      FROM_HERE, BindOnce(&ConcurrentHelper::FromTask, helper));

  run_loop.Run();
  EXPECT_EQ(0, helper->event_count());
  EXPECT_EQ(0, helper->task_count());
}

namespace {

void AddEventsAndDrainGLib(EventInjector* injector, OnceClosure on_drained) {
  // Add a couple of dummy events
  injector->AddDummyEvent(0);
  injector->AddDummyEvent(0);
  // Then add an event that will quit the main loop.
  injector->AddEvent(0, std::move(on_drained));

  // Post a couple of dummy tasks
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, DoNothing());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, DoNothing());

  // Drain the events
  while (g_main_context_pending(nullptr)) {
    g_main_context_iteration(nullptr, FALSE);
  }
}

}  // namespace

TEST_F(MessagePumpGLibTest, TestDrainingGLib) {
  // Tests that draining events using GLib works.
  RunLoop run_loop;
  loop()->task_runner()->PostTask(
      FROM_HERE, BindOnce(&AddEventsAndDrainGLib, Unretained(injector()),
                          run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(3, injector()->processed_events());
}

namespace {

// Helper class that lets us run the GLib message loop.
class GLibLoopRunner : public RefCounted<GLibLoopRunner> {
 public:
  GLibLoopRunner() : quit_(false) { }

  void RunGLib() {
    while (!quit_) {
      g_main_context_iteration(nullptr, TRUE);
    }
  }

  void RunLoop() {
    while (!quit_) {
      g_main_context_iteration(nullptr, TRUE);
    }
  }

  void Quit() {
    quit_ = true;
  }

  void Reset() {
    quit_ = false;
  }

 private:
  friend class RefCounted<GLibLoopRunner>;

  ~GLibLoopRunner() {}

  bool quit_;
};

void TestGLibLoopInternal(EventInjector* injector, OnceClosure done) {
  // Allow tasks to be processed from 'native' event loops.
  MessageLoopCurrent::Get()->SetNestableTasksAllowed(true);
  scoped_refptr<GLibLoopRunner> runner = new GLibLoopRunner();

  int task_count = 0;
  // Add a couple of dummy events
  injector->AddDummyEvent(0);
  injector->AddDummyEvent(0);
  // Post a couple of dummy tasks
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&IncrementInt, &task_count));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&IncrementInt, &task_count));
  // Delayed events
  injector->AddDummyEvent(10);
  injector->AddDummyEvent(10);
  // Delayed work
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, BindOnce(&IncrementInt, &task_count),
      TimeDelta::FromMilliseconds(30));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, BindOnce(&GLibLoopRunner::Quit, runner),
      TimeDelta::FromMilliseconds(40));

  // Run a nested, straight GLib message loop.
  runner->RunGLib();

  ASSERT_EQ(3, task_count);
  EXPECT_EQ(4, injector->processed_events());
  std::move(done).Run();
}

void TestGtkLoopInternal(EventInjector* injector, OnceClosure done) {
  // Allow tasks to be processed from 'native' event loops.
  MessageLoopCurrent::Get()->SetNestableTasksAllowed(true);
  scoped_refptr<GLibLoopRunner> runner = new GLibLoopRunner();

  int task_count = 0;
  // Add a couple of dummy events
  injector->AddDummyEvent(0);
  injector->AddDummyEvent(0);
  // Post a couple of dummy tasks
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&IncrementInt, &task_count));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&IncrementInt, &task_count));
  // Delayed events
  injector->AddDummyEvent(10);
  injector->AddDummyEvent(10);
  // Delayed work
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, BindOnce(&IncrementInt, &task_count),
      TimeDelta::FromMilliseconds(30));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, BindOnce(&GLibLoopRunner::Quit, runner),
      TimeDelta::FromMilliseconds(40));

  // Run a nested, straight Gtk message loop.
  runner->RunLoop();

  ASSERT_EQ(3, task_count);
  EXPECT_EQ(4, injector->processed_events());
  std::move(done).Run();
}

}  // namespace

TEST_F(MessagePumpGLibTest, TestGLibLoop) {
  // Tests that events and posted tasks are correctly executed if the message
  // loop is not run by MessageLoop::Run() but by a straight GLib loop.
  // Note that in this case we don't make strong guarantees about niceness
  // between events and posted tasks.
  RunLoop run_loop;
  loop()->task_runner()->PostTask(
      FROM_HERE, BindOnce(&TestGLibLoopInternal, Unretained(injector()),
                          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(MessagePumpGLibTest, TestGtkLoop) {
  // Tests that events and posted tasks are correctly executed if the message
  // loop is not run by MessageLoop::Run() but by a straight Gtk loop.
  // Note that in this case we don't make strong guarantees about niceness
  // between events and posted tasks.
  RunLoop run_loop;
  loop()->task_runner()->PostTask(
      FROM_HERE, BindOnce(&TestGtkLoopInternal, Unretained(injector()),
                          run_loop.QuitClosure()));
  run_loop.Run();
}

// Tests for WatchFileDescriptor API
class MessagePumpGLibFdWatchTest : public testing::Test {
 protected:
  MessagePumpGLibFdWatchTest()
      : io_thread_("MessagePumpGLibFdWatchTestIOThread") {}
  ~MessagePumpGLibFdWatchTest() override = default;

  void SetUp() override {
    Thread::Options options(MessagePumpType::IO, 0);
    ASSERT_TRUE(io_thread_.StartWithOptions(options));
    int ret = pipe(pipefds_);
    ASSERT_EQ(0, ret);
  }

  void TearDown() override {
    if (IGNORE_EINTR(close(pipefds_[0])) < 0)
      PLOG(ERROR) << "close";
    if (IGNORE_EINTR(close(pipefds_[1])) < 0)
      PLOG(ERROR) << "close";
  }

  void WaitUntilIoThreadStarted() {
    ASSERT_TRUE(io_thread_.WaitUntilThreadStarted());
  }

  scoped_refptr<SingleThreadTaskRunner> io_runner() const {
    return io_thread_.task_runner();
  }

  void SimulateEvent(MessagePumpGlib* pump,
                     MessagePumpGlib::FdWatchController* controller) {
    controller->poll_fd_->revents = G_IO_IN | G_IO_OUT;
    pump->HandleFdWatchDispatch(controller);
  }

  int pipefds_[2];

 private:
  Thread io_thread_;
};

namespace {

class BaseWatcher : public MessagePumpGlib::FdWatcher {
 public:
  explicit BaseWatcher(MessagePumpGlib::FdWatchController* controller)
      : controller_(controller) {
    DCHECK(controller_);
  }
  ~BaseWatcher() override = default;

  // base:MessagePumpGlib::FdWatcher interface
  void OnFileCanReadWithoutBlocking(int /* fd */) override { NOTREACHED(); }
  void OnFileCanWriteWithoutBlocking(int /* fd */) override { NOTREACHED(); }

 protected:
  MessagePumpGlib::FdWatchController* controller_;
};

class DeleteWatcher : public BaseWatcher {
 public:
  explicit DeleteWatcher(
      std::unique_ptr<MessagePumpGlib::FdWatchController> controller)
      : BaseWatcher(controller.get()),
        owned_controller_(std::move(controller)) {}

  ~DeleteWatcher() override { DCHECK(!controller_); }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {
    DCHECK(owned_controller_);
    owned_controller_.reset();
    controller_ = nullptr;
  }

 private:
  std::unique_ptr<MessagePumpGlib::FdWatchController> owned_controller_;
};

class StopWatcher : public BaseWatcher {
 public:
  explicit StopWatcher(MessagePumpGlib::FdWatchController* controller)
      : BaseWatcher(controller) {}

  ~StopWatcher() override = default;

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {
    controller_->StopWatchingFileDescriptor();
  }
};

void QuitMessageLoopAndStart(OnceClosure quit_closure) {
  std::move(quit_closure).Run();

  RunLoop runloop(RunLoop::Type::kNestableTasksAllowed);
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, runloop.QuitClosure());
  runloop.Run();
}

class NestedPumpWatcher : public MessagePumpGlib::FdWatcher {
 public:
  NestedPumpWatcher() = default;
  ~NestedPumpWatcher() override = default;

  void OnFileCanReadWithoutBlocking(int /* fd */) override {
    RunLoop runloop;
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, BindOnce(&QuitMessageLoopAndStart, runloop.QuitClosure()));
    runloop.Run();
  }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {}
};

class QuitWatcher : public BaseWatcher {
 public:
  QuitWatcher(MessagePumpGlib::FdWatchController* controller,
              base::OnceClosure quit_closure)
      : BaseWatcher(controller), quit_closure_(std::move(quit_closure)) {}

  void OnFileCanReadWithoutBlocking(int /* fd */) override {
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

void WriteFDWrapper(const int fd,
                    const char* buf,
                    int size,
                    WaitableEvent* event) {
  ASSERT_TRUE(WriteFileDescriptor(fd, buf, size));
}

}  // namespace

// Tests that MessagePumpGlib::FdWatcher::OnFileCanReadWithoutBlocking is not
// called for a READ_WRITE event, when the controller is destroyed in
// OnFileCanWriteWithoutBlocking callback.
TEST_F(MessagePumpGLibFdWatchTest, DeleteWatcher) {
  auto pump = std::make_unique<MessagePumpGlib>();
  auto controller_ptr =
      std::make_unique<MessagePumpGlib::FdWatchController>(FROM_HERE);
  auto* controller = controller_ptr.get();

  DeleteWatcher watcher(std::move(controller_ptr));
  pump->WatchFileDescriptor(pipefds_[1], false,
                            MessagePumpGlib::WATCH_READ_WRITE, controller,
                            &watcher);

  SimulateEvent(pump.get(), controller);
}

// Tests that MessagePumpGlib::FdWatcher::OnFileCanReadWithoutBlocking is not
// called for a READ_WRITE event, when the watcher calls
// StopWatchingFileDescriptor in OnFileCanWriteWithoutBlocking callback.
TEST_F(MessagePumpGLibFdWatchTest, StopWatcher) {
  std::unique_ptr<MessagePumpGlib> pump(new MessagePumpGlib);
  MessagePumpGlib::FdWatchController controller(FROM_HERE);
  StopWatcher watcher(&controller);
  pump->WatchFileDescriptor(pipefds_[1], false,
                            MessagePumpGlib::WATCH_READ_WRITE, &controller,
                            &watcher);

  SimulateEvent(pump.get(), &controller);
}

// Tests that FdWatcher works properly with nested loops.
TEST_F(MessagePumpGLibFdWatchTest, NestedPumpWatcher) {
  MessageLoop loop(MessagePumpType::UI);
  std::unique_ptr<MessagePumpGlib> pump(new MessagePumpGlib);
  MessagePumpGlib::FdWatchController controller(FROM_HERE);
  NestedPumpWatcher watcher;
  pump->WatchFileDescriptor(pipefds_[1], false, MessagePumpGlib::WATCH_READ,
                            &controller, &watcher);

  SimulateEvent(pump.get(), &controller);
}

// Tests that MessagePumpGlib quits immediately when it is quit from
// libevent's event_base_loop().
TEST_F(MessagePumpGLibFdWatchTest, QuitWatcher) {
  auto pump_ptr = std::make_unique<MessagePumpGlib>();
  MessagePumpGlib* pump = pump_ptr.get();
  MessageLoop loop(std::move(pump_ptr));
  RunLoop run_loop;
  MessagePumpGlib::FdWatchController controller(FROM_HERE);
  QuitWatcher delegate(&controller, run_loop.QuitClosure());
  WaitableEvent event;
  auto watcher = std::make_unique<WaitableEventWatcher>();

  pump->WatchFileDescriptor(pipefds_[0], false, MessagePumpGlib::WATCH_READ,
                            &controller, &delegate);

  // Make the IO thread wait for |event| before writing to pipefds[1].
  const char buf = 0;
  WaitableEventWatcher::EventCallback write_fd_task =
      BindOnce(&WriteFDWrapper, pipefds_[1], &buf, 1);
  io_runner()->PostTask(
      FROM_HERE, BindOnce(IgnoreResult(&WaitableEventWatcher::StartWatching),
                          Unretained(watcher.get()), &event,
                          std::move(write_fd_task), io_runner()));

  // Queue |event| to signal on |MessageLoopCurrentForUI::Get()|.
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&event)));

  // Now run the MessageLoop.
  run_loop.Run();

  // StartWatching can move |watcher| to IO thread. Release on IO thread.
  io_runner()->PostTask(FROM_HERE, BindOnce(&WaitableEventWatcher::StopWatching,
                                            Owned(std::move(watcher))));
}

}  // namespace base

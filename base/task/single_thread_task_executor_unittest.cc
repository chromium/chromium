// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_executor.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_type.h"
#include "base/pending_task.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/post_task.h"
#include "base/task/task_observer.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(OS_ANDROID)
#include "base/android/java_handler_thread.h"
#include "base/android/jni_android.h"
#include "base/test/android/java_handler_thread_helpers.h"
#endif

#if defined(OS_WIN)
#include "base/message_loop/message_pump_win.h"
#include "base/process/memory.h"
#include "base/win/current_module.h"
#include "base/win/message_window.h"
#include "base/win/scoped_handle.h"

#include <windows.h>
#endif

using ::testing::IsNull;
using ::testing::NotNull;

namespace base {

// TODO(darin): Platform-specific MessageLoop tests should be grouped together
// to avoid chopping this file up with so many #ifdefs.

TEST(SingleThreadTaskExecutorTest, GetTaskExecutorForCurrentThread) {
  EXPECT_THAT(GetTaskExecutorForCurrentThread(), IsNull());

  {
    SingleThreadTaskExecutor single_thread_task_executor;
    EXPECT_THAT(GetTaskExecutorForCurrentThread(), NotNull());
  }

  EXPECT_THAT(GetTaskExecutorForCurrentThread(), IsNull());
}

TEST(SingleThreadTaskExecutorTest,
     GetTaskExecutorForCurrentThreadInPostedTask) {
  SingleThreadTaskExecutor single_thread_task_executor;
  TaskExecutor* task_executor = GetTaskExecutorForCurrentThread();

  EXPECT_THAT(task_executor, NotNull());

  RunLoop run_loop;
  single_thread_task_executor.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        EXPECT_EQ(GetTaskExecutorForCurrentThread(), task_executor);
        run_loop.Quit();
      }));

  run_loop.Run();
}

namespace {

class Foo : public RefCounted<Foo> {
 public:
  Foo() : test_count_(0) {}

  void Test0() { ++test_count_; }

  void Test1ConstRef(const std::string& a) {
    ++test_count_;
    result_.append(a);
  }

  void Test1Ptr(std::string* a) {
    ++test_count_;
    result_.append(*a);
  }

  void Test1Int(int a) { test_count_ += a; }

  void Test2Ptr(std::string* a, std::string* b) {
    ++test_count_;
    result_.append(*a);
    result_.append(*b);
  }

  void Test2Mixed(const std::string& a, std::string* b) {
    ++test_count_;
    result_.append(a);
    result_.append(*b);
  }

  int test_count() const { return test_count_; }
  const std::string& result() const { return result_; }

 private:
  friend class RefCounted<Foo>;

  ~Foo() = default;

  int test_count_;
  std::string result_;

  DISALLOW_COPY_AND_ASSIGN(Foo);
};

// This function runs slowly to simulate a large amount of work being done.
static void SlowFunc(TimeDelta pause, int* quit_counter) {
  PlatformThread::Sleep(pause);
  if (--(*quit_counter) == 0)
    RunLoop::QuitCurrentWhenIdleDeprecated();
}

// This function records the time when Run was called in a Time object, which is
// useful for building a variety of SingleThreadTaskExecutor tests.
static void RecordRunTimeFunc(TimeTicks* run_time, int* quit_counter) {
  *run_time = TimeTicks::Now();

  // Cause our Run function to take some time to execute.  As a result we can
  // count on subsequent RecordRunTimeFunc()s running at a future time,
  // without worry about the resolution of our system clock being an issue.
  SlowFunc(TimeDelta::FromMilliseconds(10), quit_counter);
}

enum TaskType {
  MESSAGEBOX,
  ENDDIALOG,
  RECURSIVE,
  TIMEDMESSAGELOOP,
  QUITMESSAGELOOP,
  ORDERED,
  PUMPS,
  SLEEP,
  RUNS,
};

// Saves the order in which the tasks executed.
struct TaskItem {
  TaskItem(TaskType t, int c, bool s) : type(t), cookie(c), start(s) {}

  TaskType type;
  int cookie;
  bool start;

  bool operator==(const TaskItem& other) const {
    return type == other.type && cookie == other.cookie && start == other.start;
  }
};

std::ostream& operator<<(std::ostream& os, TaskType type) {
  switch (type) {
    case MESSAGEBOX:
      os << "MESSAGEBOX";
      break;
    case ENDDIALOG:
      os << "ENDDIALOG";
      break;
    case RECURSIVE:
      os << "RECURSIVE";
      break;
    case TIMEDMESSAGELOOP:
      os << "TIMEDMESSAGELOOP";
      break;
    case QUITMESSAGELOOP:
      os << "QUITMESSAGELOOP";
      break;
    case ORDERED:
      os << "ORDERED";
      break;
    case PUMPS:
      os << "PUMPS";
      break;
    case SLEEP:
      os << "SLEEP";
      break;
    default:
      NOTREACHED();
      os << "Unknown TaskType";
      break;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const TaskItem& item) {
  if (item.start)
    return os << item.type << " " << item.cookie << " starts";
  return os << item.type << " " << item.cookie << " ends";
}

class TaskList {
 public:
  void RecordStart(TaskType type, int cookie) {
    TaskItem item(type, cookie, true);
    DVLOG(1) << item;
    task_list_.push_back(item);
  }

  void RecordEnd(TaskType type, int cookie) {
    TaskItem item(type, cookie, false);
    DVLOG(1) << item;
    task_list_.push_back(item);
  }

  size_t Size() { return task_list_.size(); }

  TaskItem Get(int n) { return task_list_[n]; }

 private:
  std::vector<TaskItem> task_list_;
};

class DummyTaskObserver : public TaskObserver {
 public:
  explicit DummyTaskObserver(int num_tasks)
      : num_tasks_started_(0), num_tasks_processed_(0), num_tasks_(num_tasks) {}

  DummyTaskObserver(int num_tasks, int num_tasks_started)
      : num_tasks_started_(num_tasks_started),
        num_tasks_processed_(0),
        num_tasks_(num_tasks) {}

  ~DummyTaskObserver() override = default;

  void WillProcessTask(const PendingTask& pending_task,
                       bool /* was_blocked_or_low_priority */) override {
    num_tasks_started_++;
    EXPECT_LE(num_tasks_started_, num_tasks_);
    EXPECT_EQ(num_tasks_started_, num_tasks_processed_ + 1);
  }

  void DidProcessTask(const PendingTask& pending_task) override {
    num_tasks_processed_++;
    EXPECT_LE(num_tasks_started_, num_tasks_);
    EXPECT_EQ(num_tasks_started_, num_tasks_processed_);
  }

  int num_tasks_started() const { return num_tasks_started_; }
  int num_tasks_processed() const { return num_tasks_processed_; }

 private:
  int num_tasks_started_;
  int num_tasks_processed_;
  const int num_tasks_;

  DISALLOW_COPY_AND_ASSIGN(DummyTaskObserver);
};

// A method which reposts itself |depth| times.
void RecursiveFunc(TaskList* order, int cookie, int depth) {
  order->RecordStart(RECURSIVE, cookie);
  if (depth > 0) {
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, BindOnce(&RecursiveFunc, order, cookie, depth - 1));
  }
  order->RecordEnd(RECURSIVE, cookie);
}

void QuitFunc(TaskList* order, int cookie) {
  order->RecordStart(QUITMESSAGELOOP, cookie);
  RunLoop::QuitCurrentWhenIdleDeprecated();
  order->RecordEnd(QUITMESSAGELOOP, cookie);
}

#if defined(OS_WIN)

void SubPumpFunc(OnceClosure on_done) {
  CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop
      allow_nestable_tasks;
  MSG msg;
  while (::GetMessage(&msg, NULL, 0, 0)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }
  std::move(on_done).Run();
}

const wchar_t kMessageBoxTitle[] = L"SingleThreadTaskExecutor Unit Test";

// SingleThreadTaskExecutor implicitly start a "modal message loop". Modal
// dialog boxes, common controls (like OpenFile) and StartDoc printing function
// can cause implicit message loops.
void MessageBoxFunc(TaskList* order, int cookie, bool is_reentrant) {
  order->RecordStart(MESSAGEBOX, cookie);
  absl::optional<CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop>
      maybe_allow_nesting;
  if (is_reentrant)
    maybe_allow_nesting.emplace();
  ::MessageBox(NULL, L"Please wait...", kMessageBoxTitle, MB_OK);
  order->RecordEnd(MESSAGEBOX, cookie);
}

// Will end the MessageBox.
void EndDialogFunc(TaskList* order, int cookie) {
  order->RecordStart(ENDDIALOG, cookie);
  HWND window = GetActiveWindow();
  if (window != NULL) {
    EXPECT_NE(::EndDialog(window, IDCONTINUE), 0);
    // Cheap way to signal that the window wasn't found if RunEnd() isn't
    // called.
    order->RecordEnd(ENDDIALOG, cookie);
  }
}

// A method which posts a RecursiveFunc that will want to run while
// ::MessageBox() is active.
void RecursiveFuncWin(scoped_refptr<SingleThreadTaskRunner> task_runner,
                      HANDLE event,
                      bool expect_window,
                      TaskList* order,
                      bool message_box_is_reentrant) {
  task_runner->PostTask(FROM_HERE, BindOnce(&RecursiveFunc, order, 1, 2));
  task_runner->PostTask(
      FROM_HERE, BindOnce(&MessageBoxFunc, order, 2, message_box_is_reentrant));
  task_runner->PostTask(FROM_HERE, BindOnce(&RecursiveFunc, order, 3, 2));
  // The trick here is that for nested task processing, this task will be
  // ran _inside_ the MessageBox message loop, dismissing the MessageBox
  // without a chance.
  // For non-nested task processing, this will be executed _after_ the
  // MessageBox will have been dismissed by the code below, where
  // expect_window_ is true.
  task_runner->PostTask(FROM_HERE, BindOnce(&EndDialogFunc, order, 4));
  task_runner->PostTask(FROM_HERE, BindOnce(&QuitFunc, order, 5));

  // Enforce that every tasks are sent before starting to run the main thread
  // message loop.
  ASSERT_TRUE(SetEvent(event));

  // Poll for the MessageBox. Don't do this at home! At the speed we do it,
  // you will never realize one MessageBox was shown.
  for (; expect_window;) {
    HWND window = ::FindWindowW(L"#32770", kMessageBoxTitle);
    if (window) {
      // Dismiss it.
      for (;;) {
        HWND button = ::FindWindowExW(window, NULL, L"Button", NULL);
        if (button != NULL) {
          EXPECT_EQ(0, ::SendMessage(button, WM_LBUTTONDOWN, 0, 0));
          EXPECT_EQ(0, ::SendMessage(button, WM_LBUTTONUP, 0, 0));
          break;
        }
      }
      break;
    }
  }
}

#endif  // defined(OS_WIN)

void Post128KTasksThenQuit(SingleThreadTaskRunner* executor_task_runner,
                           TimeTicks begin_ticks,
                           TimeTicks last_post_ticks,
                           TimeDelta slowest_delay,
                           OnceClosure on_done,
                           int num_posts_done = 0) {
  const int kNumTimes = 128000;

  // Tasks should be running on a decent heart beat. Some platforms/bots however
  // have a hard time posting+running *all* tasks before test timeout, add
  // detailed logging for diagnosis where this flakes.
  const auto now = TimeTicks::Now();
  const auto scheduling_delay = now - last_post_ticks;
  if (scheduling_delay > slowest_delay)
    slowest_delay = scheduling_delay;

  if (num_posts_done == kNumTimes) {
    std::move(on_done).Run();
    return;
  } else if (now - begin_ticks >= TestTimeouts::action_max_timeout()) {
    ADD_FAILURE() << "Couldn't run all tasks."
                  << "\nNumber of tasks remaining: "
                  << kNumTimes - num_posts_done
                  << "\nSlowest scheduling delay: " << slowest_delay
                  << "\nAverage per task: "
                  << (now - begin_ticks) / num_posts_done;
    std::move(on_done).Run();
    return;
  }

  executor_task_runner->PostTask(
      FROM_HERE,
      BindOnce(&Post128KTasksThenQuit, Unretained(executor_task_runner),
               begin_ticks, now, slowest_delay, std::move(on_done),
               num_posts_done + 1));
}

#if defined(OS_WIN)

class TestIOHandler : public MessagePumpForIO::IOHandler {
 public:
  TestIOHandler(const wchar_t* name, HANDLE signal);

  void OnIOCompleted(MessagePumpForIO::IOContext* context,
                     DWORD bytes_transfered,
                     DWORD error) override;

  void Init();
  OVERLAPPED* context() { return &context_.overlapped; }
  DWORD size() { return sizeof(buffer_); }

 private:
  char buffer_[48];
  MessagePumpForIO::IOContext context_;
  HANDLE signal_;
  win::ScopedHandle file_;
};

TestIOHandler::TestIOHandler(const wchar_t* name, HANDLE signal)
    : MessagePumpForIO::IOHandler(FROM_HERE), signal_(signal) {
  memset(buffer_, 0, sizeof(buffer_));

  file_.Set(CreateFile(name, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                       FILE_FLAG_OVERLAPPED, NULL));
  EXPECT_TRUE(file_.IsValid());
}

void TestIOHandler::Init() {
  CurrentIOThread::Get()->RegisterIOHandler(file_.Get(), this);

  DWORD read;
  EXPECT_FALSE(ReadFile(file_.Get(), buffer_, size(), &read, context()));
  EXPECT_EQ(static_cast<DWORD>(ERROR_IO_PENDING), GetLastError());
}

void TestIOHandler::OnIOCompleted(MessagePumpForIO::IOContext* context,
                                  DWORD bytes_transfered,
                                  DWORD error) {
  ASSERT_TRUE(context == &context_);
  ASSERT_TRUE(SetEvent(signal_));
}

void RunTest_IOHandler() {
  win::ScopedHandle callback_called(CreateEvent(NULL, TRUE, FALSE, NULL));
  ASSERT_TRUE(callback_called.IsValid());

  const wchar_t* kPipeName = L"\\\\.\\pipe\\iohandler_pipe";
  win::ScopedHandle server(
      CreateNamedPipe(kPipeName, PIPE_ACCESS_OUTBOUND, 0, 1, 0, 0, 0, NULL));
  ASSERT_TRUE(server.IsValid());

  Thread thread("IOHandler test");
  Thread::Options options;
  options.message_pump_type = MessagePumpType::IO;
  ASSERT_TRUE(thread.StartWithOptions(std::move(options)));

  TestIOHandler handler(kPipeName, callback_called.Get());
  thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&TestIOHandler::Init, Unretained(&handler)));
  // Make sure the thread runs and sleeps for lack of work.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));

  const char buffer[] = "Hello there!";
  DWORD written;
  EXPECT_TRUE(WriteFile(server.Get(), buffer, sizeof(buffer), &written, NULL));

  DWORD result = WaitForSingleObject(callback_called.Get(), 1000);
  EXPECT_EQ(WAIT_OBJECT_0, result);

  thread.Stop();
}

#endif  // defined(OS_WIN)

}  // namespace

//-----------------------------------------------------------------------------
// Each test is run against each type of SingleThreadTaskExecutor.  That way we
// are sure that SingleThreadTaskExecutor works properly in all configurations.
// Of course, in some cases, a unit test may only be for a particular type of
// loop.

class SingleThreadTaskExecutorTypedTest
    : public ::testing::TestWithParam<MessagePumpType> {
 public:
  SingleThreadTaskExecutorTypedTest() = default;
  ~SingleThreadTaskExecutorTypedTest() = default;

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<MessagePumpType> param_info) {
    switch (param_info.param) {
      case MessagePumpType::DEFAULT:
        return "default_pump";
      case MessagePumpType::IO:
        return "IO_pump";
      case MessagePumpType::UI:
        return "UI_pump";
      case MessagePumpType::CUSTOM:
        break;
#if defined(OS_ANDROID)
      case MessagePumpType::JAVA:
        break;
#endif  // defined(OS_ANDROID)
#if defined(OS_APPLE)
      case MessagePumpType::NS_RUNLOOP:
        break;
#endif  // defined(OS_APPLE)
#if defined(OS_WIN)
      case MessagePumpType::UI_WITH_WM_QUIT_SUPPORT:
        break;
#endif  // defined(OS_WIN)
    }
    NOTREACHED();
    return "";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleThreadTaskExecutorTypedTest);
};

TEST_P(SingleThreadTaskExecutorTypedTest, PostTask) {
  SingleThreadTaskExecutor executor(GetParam());
  // Add tests to message loop
  scoped_refptr<Foo> foo(new Foo());
  std::string a("a"), b("b"), c("c"), d("d");
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&Foo::Test0, foo));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test1ConstRef, foo, a));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&Foo::Test1Ptr, foo, &b));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&Foo::Test1Int, foo, 100));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test2Ptr, foo, &a, &c));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test2Mixed, foo, a, &d));
  // After all tests, post a message that will shut down the message loop
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&RunLoop::QuitCurrentWhenIdleDeprecated));

  // Now kick things off
  RunLoop().Run();

  EXPECT_EQ(foo->test_count(), 105);
  EXPECT_EQ(foo->result(), "abacad");
}

TEST_P(SingleThreadTaskExecutorTypedTest, PostDelayedTask_Basic) {
  SingleThreadTaskExecutor executor(GetParam());

  // Test that PostDelayedTask results in a delayed task.

  const TimeDelta kDelay = TimeDelta::FromMilliseconds(100);

  int num_tasks = 1;
  TimeTicks run_time;

  TimeTicks time_before_run = TimeTicks::Now();
  executor.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time, &num_tasks), kDelay);
  RunLoop().Run();
  TimeTicks time_after_run = TimeTicks::Now();

  EXPECT_EQ(0, num_tasks);
  EXPECT_LT(kDelay, time_after_run - time_before_run);
}

TEST_P(SingleThreadTaskExecutorTypedTest, PostDelayedTask_InDelayOrder) {
  SingleThreadTaskExecutor executor(GetParam());

  // Test that two tasks with different delays run in the right order.
  int num_tasks = 2;
  TimeTicks run_time1, run_time2;

  executor.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time1, &num_tasks),
      TimeDelta::FromMilliseconds(200));
  // If we get a large pause in execution (due to a context switch) here, this
  // test could fail.
  executor.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time2, &num_tasks),
      TimeDelta::FromMilliseconds(10));

  RunLoop().Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time2 < run_time1);
}

TEST_P(SingleThreadTaskExecutorTypedTest, PostDelayedTask_InPostOrder) {
  SingleThreadTaskExecutor executor(GetParam());

  // Test that two tasks with the same delay run in the order in which they
  // were posted.
  //
  // NOTE: This is actually an approximate test since the API only takes a
  // "delay" parameter, so we are not exactly simulating two tasks that get
  // posted at the exact same time.  It would be nice if the API allowed us to
  // specify the desired run time.

  const TimeDelta kDelay = TimeDelta::FromMilliseconds(100);

  int num_tasks = 2;
  TimeTicks run_time1, run_time2;

  executor.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time1, &num_tasks), kDelay);
  executor.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time2, &num_tasks), kDelay);

  RunLoop().Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time1 < run_time2);
}

TEST_P(SingleThreadTaskExecutorTypedTest, PostDelayedTask_InPostOrder_2) {
  SingleThreadTaskExecutor executor(GetParam());

  // Test that a delayed task still runs after a normal tasks even if the
  // normal tasks take a long time to run.

  const TimeDelta kPause = TimeDelta::FromMilliseconds(50);

  int num_tasks = 2;
  TimeTicks run_time;

  executor.task_runner()->PostTask(FROM_HERE,
                                   BindOnce(&SlowFunc, kPause, &num_tasks));
  executor.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time, &num_tasks),
      TimeDelta::FromMilliseconds(10));

  TimeTicks time_before_run = TimeTicks::Now();
  RunLoop().Run();
  TimeTicks time_after_run = TimeTicks::Now();

  EXPECT_EQ(0, num_tasks);

  EXPECT_LT(kPause, time_after_run - time_before_run);
}

TEST_P(SingleThreadTaskExecutorTypedTest, PostDelayedTask_InPostOrder_3) {
  SingleThreadTaskExecutor executor(GetParam());

  // Test that a delayed task still runs after a pile of normal tasks.  The key
  // difference between this test and the previous one is that here we return
  // the SingleThreadTaskExecutor a lot so we give the SingleThreadTaskExecutor
  // plenty of opportunities to maybe run the delayed task.  It should know not
  // to do so until the delayed task's delay has passed.

  int num_tasks = 11;
  TimeTicks run_time1, run_time2;

  // Clutter the ML with tasks.
  for (int i = 1; i < num_tasks; ++i)
    executor.task_runner()->PostTask(
        FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time1, &num_tasks));

  executor.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time2, &num_tasks),
      TimeDelta::FromMilliseconds(1));

  RunLoop().Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time2 > run_time1);
}

TEST_P(SingleThreadTaskExecutorTypedTest, PostDelayedTask_SharedTimer) {
  SingleThreadTaskExecutor executor(GetParam());

  // Test that the interval of the timer, used to run the next delayed task, is
  // set to a value corresponding to when the next delayed task should run.

  // By setting num_tasks to 1, we ensure that the first task to run causes the
  // run loop to exit.
  int num_tasks = 1;
  TimeTicks run_time1, run_time2;

  executor.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time1, &num_tasks),
      TimeDelta::FromSeconds(1000));
  executor.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time2, &num_tasks),
      TimeDelta::FromMilliseconds(10));

  TimeTicks start_time = TimeTicks::Now();

  RunLoop().Run();
  EXPECT_EQ(0, num_tasks);

  // Ensure that we ran in far less time than the slower timer.
  TimeDelta total_time = TimeTicks::Now() - start_time;
  EXPECT_GT(5000, total_time.InMilliseconds());

  // In case both timers somehow run at nearly the same time, sleep a little
  // and then run all pending to force them both to have run.  This is just
  // encouraging flakiness if there is any.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
  RunLoop().RunUntilIdle();

  EXPECT_TRUE(run_time1.is_null());
  EXPECT_FALSE(run_time2.is_null());
}

namespace {

// This is used to inject a test point for recording the destructor calls for
// Closure objects send to MessageLoop::PostTask(). It is awkward usage since we
// are trying to hook the actual destruction, which is not a common operation.
class RecordDeletionProbe : public RefCounted<RecordDeletionProbe> {
 public:
  RecordDeletionProbe(RecordDeletionProbe* post_on_delete, bool* was_deleted)
      : post_on_delete_(post_on_delete), was_deleted_(was_deleted) {}
  void Run() {}

 private:
  friend class RefCounted<RecordDeletionProbe>;

  ~RecordDeletionProbe() {
    *was_deleted_ = true;
    if (post_on_delete_.get())
      ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, BindOnce(&RecordDeletionProbe::Run, post_on_delete_));
  }

  scoped_refptr<RecordDeletionProbe> post_on_delete_;
  bool* was_deleted_;
};

}  // namespace

/* TODO(darin): SingleThreadTaskExecutor does not support deleting all tasks in
 */
/* the destructor. */
/* Fails, http://crbug.com/50272. */
TEST_P(SingleThreadTaskExecutorTypedTest, DISABLED_EnsureDeletion) {
  bool a_was_deleted = false;
  bool b_was_deleted = false;
  {
    SingleThreadTaskExecutor executor(GetParam());
    executor.task_runner()->PostTask(
        FROM_HERE, BindOnce(&RecordDeletionProbe::Run,
                            new RecordDeletionProbe(nullptr, &a_was_deleted)));
    // TODO(ajwong): Do we really need 1000ms here?
    executor.task_runner()->PostDelayedTask(
        FROM_HERE,
        BindOnce(&RecordDeletionProbe::Run,
                 new RecordDeletionProbe(nullptr, &b_was_deleted)),
        TimeDelta::FromMilliseconds(1000));
  }
  EXPECT_TRUE(a_was_deleted);
  EXPECT_TRUE(b_was_deleted);
}

/* TODO(darin): SingleThreadTaskExecutor does not support deleting all tasks in
 */
/* the destructor. */
/* Fails, http://crbug.com/50272. */
TEST_P(SingleThreadTaskExecutorTypedTest, DISABLED_EnsureDeletion_Chain) {
  bool a_was_deleted = false;
  bool b_was_deleted = false;
  bool c_was_deleted = false;
  {
    SingleThreadTaskExecutor executor(GetParam());
    // The scoped_refptr for each of the below is held either by the chained
    // RecordDeletionProbe, or the bound RecordDeletionProbe::Run() callback.
    RecordDeletionProbe* a = new RecordDeletionProbe(nullptr, &a_was_deleted);
    RecordDeletionProbe* b = new RecordDeletionProbe(a, &b_was_deleted);
    RecordDeletionProbe* c = new RecordDeletionProbe(b, &c_was_deleted);
    executor.task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&RecordDeletionProbe::Run, c));
  }
  EXPECT_TRUE(a_was_deleted);
  EXPECT_TRUE(b_was_deleted);
  EXPECT_TRUE(c_was_deleted);
}

namespace {

void NestingFunc(int* depth) {
  if (*depth > 0) {
    *depth -= 1;
    ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                            BindOnce(&NestingFunc, depth));

    RunLoop(RunLoop::Type::kNestableTasksAllowed).Run();
  }
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

}  // namespace

TEST_P(SingleThreadTaskExecutorTypedTest, Nesting) {
  SingleThreadTaskExecutor executor(GetParam());

  int depth = 50;
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&NestingFunc, &depth));
  RunLoop().Run();
  EXPECT_EQ(depth, 0);
}

TEST_P(SingleThreadTaskExecutorTypedTest, Recursive) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&RecursiveFunc, &order, 1, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&RecursiveFunc, &order, 2, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&QuitFunc, &order, 3));

  RunLoop().Run();

  // FIFO order.
  ASSERT_EQ(14U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order.Get(5), TaskItem(QUITMESSAGELOOP, 3, false));
  EXPECT_EQ(order.Get(6), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(7), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(8), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(9), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(10), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(11), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 2, false));
}

namespace {

void OrderedFunc(TaskList* order, int cookie) {
  order->RecordStart(ORDERED, cookie);
  order->RecordEnd(ORDERED, cookie);
}

}  // namespace

// Tests that non nestable tasks run in FIFO if there are no nested loops.
TEST_P(SingleThreadTaskExecutorTypedTest, NonNestableWithNoNesting) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  ThreadTaskRunnerHandle::Get()->PostNonNestableTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 1));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&QuitFunc, &order, 3));
  RunLoop().Run();

  // FIFO order.
  ASSERT_EQ(6U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(ORDERED, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(ORDERED, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order.Get(5), TaskItem(QUITMESSAGELOOP, 3, false));
}

namespace {

void FuncThatPumps(TaskList* order, int cookie) {
  order->RecordStart(PUMPS, cookie);
  RunLoop(RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  order->RecordEnd(PUMPS, cookie);
}

void SleepFunc(TaskList* order, int cookie, TimeDelta delay) {
  order->RecordStart(SLEEP, cookie);
  PlatformThread::Sleep(delay);
  order->RecordEnd(SLEEP, cookie);
}

}  // namespace

// Tests that non nestable tasks don't run when there's code in the call stack.
TEST_P(SingleThreadTaskExecutorTypedTest, NonNestableDelayedInNestedLoop) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&FuncThatPumps, &order, 1));
  ThreadTaskRunnerHandle::Get()->PostNonNestableTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 3));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&SleepFunc, &order, 4, TimeDelta::FromMilliseconds(50)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 5));
  ThreadTaskRunnerHandle::Get()->PostNonNestableTask(
      FROM_HERE, BindOnce(&QuitFunc, &order, 6));

  RunLoop().Run();

  // FIFO order.
  ASSERT_EQ(12U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(PUMPS, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(2), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(order.Get(3), TaskItem(SLEEP, 4, true));
  EXPECT_EQ(order.Get(4), TaskItem(SLEEP, 4, false));
  EXPECT_EQ(order.Get(5), TaskItem(ORDERED, 5, true));
  EXPECT_EQ(order.Get(6), TaskItem(ORDERED, 5, false));
  EXPECT_EQ(order.Get(7), TaskItem(PUMPS, 1, false));
  EXPECT_EQ(order.Get(8), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(9), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(10), TaskItem(QUITMESSAGELOOP, 6, true));
  EXPECT_EQ(order.Get(11), TaskItem(QUITMESSAGELOOP, 6, false));
}

namespace {

void FuncThatRuns(TaskList* order, int cookie, RunLoop* run_loop) {
  order->RecordStart(RUNS, cookie);
  run_loop->Run();
  order->RecordEnd(RUNS, cookie);
}

void FuncThatQuitsNow() {
  base::RunLoop::QuitCurrentDeprecated();
}

}  // namespace

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(SingleThreadTaskExecutorTypedTest, QuitNow) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&FuncThatQuitsNow));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 3));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&FuncThatQuitsNow));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 4));  // never runs

  RunLoop().Run();

  ASSERT_EQ(6U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitTop) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          outer_run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_run_loop.QuitClosure());

  outer_run_loop.Run();

  ASSERT_EQ(4U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitNested) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          outer_run_loop.QuitClosure());

  outer_run_loop.Run();

  ASSERT_EQ(4U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Quits current loop and immediately runs a nested loop.
void QuitAndRunNestedLoop(TaskList* order,
                          int cookie,
                          RunLoop* outer_run_loop,
                          RunLoop* nested_run_loop) {
  order->RecordStart(RUNS, cookie);
  outer_run_loop->Quit();
  nested_run_loop->Run();
  order->RecordEnd(RUNS, cookie);
}

// Test that we can run nested loop after quitting the current one.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopNestedAfterQuit) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop;

  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&QuitAndRunNestedLoop, &order, 1, &outer_run_loop,
                          &nested_run_loop));

  outer_run_loop.Run();

  ASSERT_EQ(2U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitBogus) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);
  RunLoop bogus_run_loop;

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          bogus_run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          outer_run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_run_loop.QuitClosure());

  outer_run_loop.Run();

  ASSERT_EQ(4U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitDeep) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_loop1(RunLoop::Type::kNestableTasksAllowed);
  RunLoop nested_loop2(RunLoop::Type::kNestableTasksAllowed);
  RunLoop nested_loop3(RunLoop::Type::kNestableTasksAllowed);
  RunLoop nested_loop4(RunLoop::Type::kNestableTasksAllowed);

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_loop1)));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 2, Unretained(&nested_loop2)));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 3, Unretained(&nested_loop3)));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 4, Unretained(&nested_loop4)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 5));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          outer_run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 6));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_loop1.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 7));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_loop2.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 8));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_loop3.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 9));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_loop4.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 10));

  outer_run_loop.Run();

  ASSERT_EQ(18U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 3, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 4, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 5, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 5, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 6, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 6, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 7, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 7, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 8, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 8, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 9, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 9, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 4, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 3, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit works before RunWithID.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitOrderBefore) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop run_loop;

  run_loop.Quit();

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 1));  // never runs
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatQuitsNow));  // never runs

  run_loop.Run();

  ASSERT_EQ(0U, order.Size());
}

// Tests RunLoopQuit works during RunWithID.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitOrderDuring) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop run_loop;

  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 1));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 2));  // never runs
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatQuitsNow));  // never runs

  run_loop.Run();

  ASSERT_EQ(2U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit works after RunWithID.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitOrderAfter) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&FuncThatQuitsNow));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 3));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, nested_run_loop.QuitClosure());  // has no affect
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 4));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&FuncThatQuitsNow));

  nested_run_loop.allow_quit_current_deprecated_ = true;

  RunLoop outer_run_loop;
  outer_run_loop.Run();

  ASSERT_EQ(8U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 4, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 4, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Regression test for crbug.com/170904 where posting tasks recursively caused
// the message loop to hang in MessagePumpGLib, due to the buffer of the
// internal pipe becoming full. Test all SingleThreadTaskExecutor types to
// ensure this issue does not exist in other MessagePumps.
//
// On Linux, the pipe buffer size is 64KiB by default. The bug caused one byte
// accumulated in the pipe per two posts, so we should repeat 128K times to
// reproduce the bug.
#if defined(OS_CHROMEOS)
// TODO(crbug.com/1188497): This test is unreasonably slow on CrOS and flakily
// times out (100x slower than other platforms which take < 1s to complete
// it).
#define MAYBE_RecursivePostsDoNotFloodPipe DISABLED_RecursivePostsDoNotFloodPipe
#else
#define MAYBE_RecursivePostsDoNotFloodPipe RecursivePostsDoNotFloodPipe
#endif
TEST_P(SingleThreadTaskExecutorTypedTest, MAYBE_RecursivePostsDoNotFloodPipe) {
  SingleThreadTaskExecutor executor(GetParam());
  const auto begin_ticks = TimeTicks::Now();
  RunLoop run_loop;
  Post128KTasksThenQuit(executor.task_runner().get(), begin_ticks, begin_ticks,
                        TimeDelta(), run_loop.QuitClosure());
  run_loop.Run();
}

TEST_P(SingleThreadTaskExecutorTypedTest, NestableTasksAllowedAtTopLevel) {
  SingleThreadTaskExecutor executor(GetParam());
  EXPECT_TRUE(CurrentThread::Get()->NestableTasksAllowed());
}

// Nestable tasks shouldn't be allowed to run reentrantly by default (regression
// test for https://crbug.com/754112).
TEST_P(SingleThreadTaskExecutorTypedTest, NestableTasksDisallowedByDefault) {
  SingleThreadTaskExecutor executor(GetParam());
  RunLoop run_loop;
  executor.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(
          [](RunLoop* run_loop) {
            EXPECT_FALSE(CurrentThread::Get()->NestableTasksAllowed());
            run_loop->Quit();
          },
          Unretained(&run_loop)));
  run_loop.Run();
}

TEST_P(SingleThreadTaskExecutorTypedTest,
       NestableTasksProcessedWhenRunLoopAllows) {
  SingleThreadTaskExecutor executor(GetParam());
  RunLoop run_loop;
  executor.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(
          [](RunLoop* run_loop) {
            // This test would hang if this RunLoop wasn't of type
            // kNestableTasksAllowed (i.e. this is testing that this is
            // processed and doesn't hang).
            RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);
            ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                BindOnce(
                    [](RunLoop* nested_run_loop) {
                      // Each additional layer of application task nesting
                      // requires its own allowance. The kNestableTasksAllowed
                      // RunLoop allowed this task to be processed but further
                      // nestable tasks are by default disallowed from this
                      // layer.
                      EXPECT_FALSE(
                          CurrentThread::Get()->NestableTasksAllowed());
                      nested_run_loop->Quit();
                    },
                    Unretained(&nested_run_loop)));
            nested_run_loop.Run();

            run_loop->Quit();
          },
          Unretained(&run_loop)));
  run_loop.Run();
}

TEST_P(SingleThreadTaskExecutorTypedTest,
       NestableTasksAllowedExplicitlyInScope) {
  SingleThreadTaskExecutor executor(GetParam());
  RunLoop run_loop;
  executor.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(
          [](RunLoop* run_loop) {
            {
              CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop
                  allow_nestable_tasks;
              EXPECT_TRUE(CurrentThread::Get()->NestableTasksAllowed());
            }
            EXPECT_FALSE(CurrentThread::Get()->NestableTasksAllowed());
            run_loop->Quit();
          },
          Unretained(&run_loop)));
  run_loop.Run();
}

TEST_P(SingleThreadTaskExecutorTypedTest, IsIdleForTesting) {
  SingleThreadTaskExecutor executor(GetParam());
  EXPECT_TRUE(CurrentThread::Get()->IsIdleForTesting());
  executor.task_runner()->PostTask(FROM_HERE, BindOnce([]() {}));
  executor.task_runner()->PostDelayedTask(FROM_HERE, BindOnce([]() {}),
                                          TimeDelta::FromMilliseconds(10));
  EXPECT_FALSE(CurrentThread::Get()->IsIdleForTesting());
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(CurrentThread::Get()->IsIdleForTesting());

  PlatformThread::Sleep(TimeDelta::FromMilliseconds(20));
  EXPECT_TRUE(CurrentThread::Get()->IsIdleForTesting());
}

TEST_P(SingleThreadTaskExecutorTypedTest, IsIdleForTestingNonNestableTask) {
  SingleThreadTaskExecutor executor(GetParam());
  RunLoop run_loop;
  EXPECT_TRUE(CurrentThread::Get()->IsIdleForTesting());
  bool nested_task_run = false;
  executor.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);

        executor.task_runner()->PostNonNestableTask(
            FROM_HERE, BindLambdaForTesting([&]() { nested_task_run = true; }));

        executor.task_runner()->PostTask(
            FROM_HERE, BindLambdaForTesting([&]() {
              EXPECT_FALSE(nested_task_run);
              EXPECT_TRUE(CurrentThread::Get()->IsIdleForTesting());
            }));

        nested_run_loop.RunUntilIdle();
        EXPECT_FALSE(nested_task_run);
        EXPECT_FALSE(CurrentThread::Get()->IsIdleForTesting());
      }));

  run_loop.RunUntilIdle();

  EXPECT_TRUE(nested_task_run);
  EXPECT_TRUE(CurrentThread::Get()->IsIdleForTesting());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SingleThreadTaskExecutorTypedTest,
                         ::testing::Values(MessagePumpType::DEFAULT,
                                           MessagePumpType::UI,
                                           MessagePumpType::IO),
                         SingleThreadTaskExecutorTypedTest::ParamInfoToString);

#if defined(OS_WIN)

// Verifies that the SingleThreadTaskExecutor ignores WM_QUIT, rather than
// quitting. Users of SingleThreadTaskExecutor typically expect to control when
// their RunLoops stop Run()ning explicitly, via QuitClosure() etc (see
// https://crbug.com/720078).
TEST(SingleThreadTaskExecutorTest, WmQuitIsIgnored) {
  SingleThreadTaskExecutor executor(MessagePumpType::UI);

  // Post a WM_QUIT message to the current thread.
  ::PostQuitMessage(0);

  // Post a task to the current thread, with a small delay to make it less
  // likely that we process the posted task before looking for WM_* messages.
  bool task_was_run = false;
  RunLoop run_loop;
  executor.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          [](bool* flag, OnceClosure closure) {
            *flag = true;
            std::move(closure).Run();
          },
          &task_was_run, run_loop.QuitClosure()),
      TestTimeouts::tiny_timeout());

  // Run the loop, and ensure that the posted task is processed before we quit.
  run_loop.Run();
  EXPECT_TRUE(task_was_run);
}

TEST(SingleThreadTaskExecutorTest, PostDelayedTask_SharedTimer_SubPump) {
  SingleThreadTaskExecutor executor(MessagePumpType::UI);

  // Test that the interval of the timer, used to run the next delayed task, is
  // set to a value corresponding to when the next delayed task should run.

  // By setting num_tasks to 1, we ensure that the first task to run causes the
  // run loop to exit.
  int num_tasks = 1;
  TimeTicks run_time;

  RunLoop run_loop;

  executor.task_runner()->PostTask(
      FROM_HERE, BindOnce(&SubPumpFunc, run_loop.QuitClosure()));

  // This very delayed task should never run.
  executor.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time, &num_tasks),
      TimeDelta::FromSeconds(1000));

  // This slightly delayed task should run from within SubPumpFunc.
  executor.task_runner()->PostDelayedTask(FROM_HERE,
                                          BindOnce(&::PostQuitMessage, 0),
                                          TimeDelta::FromMilliseconds(10));

  Time start_time = Time::Now();

  run_loop.Run();
  EXPECT_EQ(1, num_tasks);

  // Ensure that we ran in far less time than the slower timer.
  TimeDelta total_time = Time::Now() - start_time;
  EXPECT_GT(5000, total_time.InMilliseconds());

  // In case both timers somehow run at nearly the same time, sleep a little
  // and then run all pending to force them both to have run.  This is just
  // encouraging flakiness if there is any.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
  RunLoop().RunUntilIdle();

  EXPECT_TRUE(run_time.is_null());
}

namespace {

// When this fires (per the associated WM_TIMER firing), it posts an
// application task to quit the native loop.
bool QuitOnSystemTimer(UINT message,
                       WPARAM wparam,
                       LPARAM lparam,
                       LRESULT* result) {
  if (message == static_cast<UINT>(WM_TIMER)) {
    ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                            BindOnce(&::PostQuitMessage, 0));
  }
  *result = 0;
  return true;
}

// When this fires (per the associated WM_TIMER firing), it posts a delayed
// application task to quit the native loop.
bool DelayedQuitOnSystemTimer(UINT message,
                              WPARAM wparam,
                              LPARAM lparam,
                              LRESULT* result) {
  if (message == static_cast<UINT>(WM_TIMER)) {
    ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, BindOnce(&::PostQuitMessage, 0),
        TimeDelta::FromMilliseconds(10));
  }
  *result = 0;
  return true;
}

}  // namespace

// This is a regression test for
// https://crrev.com/c/1455266/9/base/message_loop/message_pump_win.cc#125
// See below for the delayed task version.
TEST(SingleThreadTaskExecutorTest, PostImmediateTaskFromSystemPump) {
  SingleThreadTaskExecutor executor(MessagePumpType::UI);

  RunLoop run_loop;

  // A native message window to generate a system message which invokes
  // QuitOnSystemTimer() when the native timer fires.
  win::MessageWindow local_message_window;
  local_message_window.Create(BindRepeating(&QuitOnSystemTimer));
  ASSERT_TRUE(::SetTimer(local_message_window.hwnd(), 0, 20, nullptr));

  // The first task will enter a native message loop. This test then verifies
  // that the pump is able to run an immediate application task after the native
  // pump went idle.
  executor.task_runner()->PostTask(
      FROM_HERE, BindOnce(&SubPumpFunc, run_loop.QuitClosure()));

  // Test success is determined by not hanging in this Run() call.
  run_loop.Run();
}

// This is a regression test for
// https://crrev.com/c/1455266/9/base/message_loop/message_pump_win.cc#125 This
// is the delayed task equivalent of the above PostImmediateTaskFromSystemPump
// test.
//
// As a reminder of how this works, here's the sequence of events in this test:
//  1) Test start:
//       work_deduplicator.cc(24): BindToCurrentThread
//       work_deduplicator.cc(34): OnWorkRequested
//       thread_controller_with_message_pump_impl.cc(237) : DoWork
//       work_deduplicator.cc(50): OnWorkStarted
//  2) SubPumpFunc entered:
//       message_loop_unittest.cc(278): SubPumpFunc
//  3) ScopedAllowApplicationTasksInNativeNestedLoop triggers nested
//     ScheduleWork: work_deduplicator.cc(34): OnWorkRequested
//  4) Nested system loop starts and pumps internal kMsgHaveWork:
//       message_loop_unittest.cc(282): SubPumpFunc : Got Message
//       message_pump_win.cc(302): HandleWorkMessage
//       thread_controller_with_message_pump_impl.cc(237) : DoWork
//  5) Attempt to DoWork(), there's nothing to do, NextWorkInfo indicates delay.
//       work_deduplicator.cc(50): OnWorkStarted
//       work_deduplicator.cc(58): WillCheckForMoreWork
//       work_deduplicator.cc(67): DidCheckForMoreWork
//  6) Return control to HandleWorkMessage() which schedules native timer
//     and goes to sleep (no kMsgHaveWork in native queue).
//       message_pump_win.cc(328): HandleWorkMessage ScheduleNativeTimer
//  7) Native timer fires and posts the delayed application task:
//       message_loop_unittest.cc(282): SubPumpFunc : Got Message
//       message_loop_unittest.cc(1581): DelayedQuitOnSystemTimer
//  !! This is the critical step verified by this test. Since the
//     ThreadController is idle after (6), it won't be invoked again and thus
//     won't get a chance to return a NextWorkInfo that indicates the next
//     delay. A native timer is thus required to have SubPumpFunc handle it.
//       work_deduplicator.cc(42): OnDelayedWorkRequested
//       message_pump_win.cc(129): ScheduleDelayedWork
//  9) The scheduled native timer fires and runs application task binding
//     ::PostQuitMessage :
//       message_loop_unittest.cc(282) SubPumpFunc : Got Message
//       work_deduplicator.cc(50): OnWorkStarted
//       thread_controller_with_message_pump_impl.cc(237) : DoWork
//  10) SequenceManager updates delay to none and notifies
//      (TODO(scheduler-dev): Could remove this step but WorkDeduplicator knows
//                            to ignore at least):
//       work_deduplicator.cc(42): OnDelayedWorkRequested
//  11) Nested application task completes and SubPumpFunc unwinds:
//       work_deduplicator.cc(58): WillCheckForMoreWork
//       work_deduplicator.cc(67): DidCheckForMoreWork
//  12) ~ScopedAllowApplicationTasksInNativeNestedLoop() makes sure
//      WorkDeduplicator knows we're back in DoWork() (not relevant in this test
//      but important overall). work_deduplicator.cc(50): OnWorkStarted
//  13) Application task which ran SubPumpFunc completes and test finishes.
//       work_deduplicator.cc(67): DidCheckForMoreWork
TEST(SingleThreadTaskExecutorTest, PostDelayedTaskFromSystemPump) {
  SingleThreadTaskExecutor executor(MessagePumpType::UI);

  RunLoop run_loop;

  // A native message window to generate a system message which invokes
  // DelayedQuitOnSystemTimer() when the native timer fires.
  win::MessageWindow local_message_window;
  local_message_window.Create(BindRepeating(&DelayedQuitOnSystemTimer));
  ASSERT_TRUE(::SetTimer(local_message_window.hwnd(), 0, 20, nullptr));

  // The first task will enter a native message loop. This test then verifies
  // that the pump is able to run a delayed application task after the native
  // pump went idle.
  executor.task_runner()->PostTask(
      FROM_HERE, BindOnce(&SubPumpFunc, run_loop.QuitClosure()));

  // Test success is determined by not hanging in this Run() call.
  run_loop.Run();
}

TEST(SingleThreadTaskExecutorTest, WmQuitIsVisibleToSubPump) {
  SingleThreadTaskExecutor executor(MessagePumpType::UI);

  // Regression test for https://crbug.com/888559. When processing a
  // kMsgHaveWork we peek and remove the next message and dispatch that ourself,
  // to minimize impact of these messages on message-queue processing. If we
  // received kMsgHaveWork dispatched by a nested pump (e.g. ::GetMessage()
  // loop) then there is a risk that the next message is that loop's WM_QUIT
  // message, which must be processed directly by ::GetMessage() for the loop to
  // actually quit. This test verifies that WM_QUIT exits works as expected even
  // if it happens to immediately follow a kMsgHaveWork in the queue.

  RunLoop run_loop;

  // This application task will enter the subpump.
  executor.task_runner()->PostTask(
      FROM_HERE, BindOnce(&SubPumpFunc, run_loop.QuitClosure()));

  // This application task will post a native WM_QUIT.
  executor.task_runner()->PostTask(FROM_HERE, BindOnce(&::PostQuitMessage, 0));

  // The presence of this application task means that the pump will see a
  // non-empty queue after processing the previous application task (which
  // posted the WM_QUIT) and hence will repost a kMsgHaveWork message in the
  // native event queue. Without the fix to https://crbug.com/888559, this would
  // previously result in the subpump processing kMsgHaveWork and it stealing
  // the WM_QUIT message, leaving the test hung in the subpump.
  executor.task_runner()->PostTask(FROM_HERE, DoNothing());

  // Test success is determined by not hanging in this Run() call.
  run_loop.Run();
}

TEST(SingleThreadTaskExecutorTest,
     RepostingWmQuitDoesntStarveUpcomingNativeLoop) {
  SingleThreadTaskExecutor executor(MessagePumpType::UI);

  // This test ensures that application tasks are being processed by the native
  // subpump despite the kMsgHaveWork event having already been consumed by the
  // time the subpump is entered. This is subtly enforced by
  // CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop which
  // will ScheduleWork() upon construction (and if it's absent, the
  // SingleThreadTaskExecutor shouldn't process application tasks so
  // kMsgHaveWork is irrelevant). Note: This test also fails prior to the fix
  // for https://crbug.com/888559 (in fact, the last two tasks are sufficient as
  // a regression test), probably because of a dangling kMsgHaveWork recreating
  // the effect from
  // SingleThreadTaskExecutorTest.NativeMsgProcessingDoesntStealWmQuit.

  RunLoop run_loop;

  // This application task will post a native WM_QUIT which will be ignored
  // by the main message pump.
  executor.task_runner()->PostTask(FROM_HERE, BindOnce(&::PostQuitMessage, 0));

  // Make sure the pump does a few extra cycles and processes (ignores) the
  // WM_QUIT.
  executor.task_runner()->PostTask(FROM_HERE, DoNothing());
  executor.task_runner()->PostTask(FROM_HERE, DoNothing());

  // This application task will enter the subpump.
  executor.task_runner()->PostTask(
      FROM_HERE, BindOnce(&SubPumpFunc, run_loop.QuitClosure()));

  // Post an application task that will post WM_QUIT to the nested loop. The
  // test will hang if the subpump doesn't process application tasks as it
  // should.
  executor.task_runner()->PostTask(FROM_HERE, BindOnce(&::PostQuitMessage, 0));

  // Test success is determined by not hanging in this Run() call.
  run_loop.Run();
}

// TODO(https://crbug.com/890016): Enable once multiple layers of nested loops
// works.
TEST(SingleThreadTaskExecutorTest,
     DISABLED_UnwindingMultipleSubPumpsDoesntStarveApplicationTasks) {
  SingleThreadTaskExecutor executor(MessagePumpType::UI);

  // Regression test for https://crbug.com/890016.
  // Tests that the subpump is still processing application tasks after
  // unwinding from nested subpumps (i.e. that they didn't consume the last
  // kMsgHaveWork).

  RunLoop run_loop;

  // Enter multiple levels of nested subpumps.
  executor.task_runner()->PostTask(
      FROM_HERE, BindOnce(&SubPumpFunc, run_loop.QuitClosure()));
  executor.task_runner()->PostTask(FROM_HERE,
                                   BindOnce(&SubPumpFunc, DoNothing::Once()));
  executor.task_runner()->PostTask(FROM_HERE,
                                   BindOnce(&SubPumpFunc, DoNothing::Once()));

  // Quit two layers (with tasks in between to allow each quit to be handled
  // before continuing -- ::PostQuitMessage() sets a bit, it's not a real queued
  // message :
  // https://blogs.msdn.microsoft.com/oldnewthing/20051104-33/?p=33453).
  executor.task_runner()->PostTask(FROM_HERE, BindOnce(&::PostQuitMessage, 0));
  executor.task_runner()->PostTask(FROM_HERE, DoNothing());
  executor.task_runner()->PostTask(FROM_HERE, DoNothing());
  executor.task_runner()->PostTask(FROM_HERE, BindOnce(&::PostQuitMessage, 0));
  executor.task_runner()->PostTask(FROM_HERE, DoNothing());
  executor.task_runner()->PostTask(FROM_HERE, DoNothing());

  bool last_task_ran = false;
  executor.task_runner()->PostTask(
      FROM_HERE, BindOnce([](bool* to_set) { *to_set = true; },
                          Unretained(&last_task_ran)));

  executor.task_runner()->PostTask(FROM_HERE, BindOnce(&::PostQuitMessage, 0));

  run_loop.Run();

  EXPECT_TRUE(last_task_ran);
}

namespace {

// A side effect of this test is the generation a beep. Sorry.
void RunTest_NestingDenial2(MessagePumpType message_pump_type) {
  SingleThreadTaskExecutor executor(message_pump_type);

  Thread worker("NestingDenial2_worker");
  Thread::Options options;
  options.message_pump_type = message_pump_type;
  ASSERT_EQ(true, worker.StartWithOptions(std::move(options)));
  TaskList order;
  win::ScopedHandle event(CreateEvent(NULL, FALSE, FALSE, NULL));
  worker.task_runner()->PostTask(
      FROM_HERE, BindOnce(&RecursiveFuncWin, ThreadTaskRunnerHandle::Get(),
                          event.Get(), true, &order, false));
  // Let the other thread execute.
  WaitForSingleObject(event.Get(), INFINITE);
  RunLoop().Run();

  ASSERT_EQ(17u, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(MESSAGEBOX, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(MESSAGEBOX, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(5), TaskItem(RECURSIVE, 3, false));
  // When EndDialogFunc is processed, the window is already dismissed, hence no
  // "end" entry.
  EXPECT_EQ(order.Get(6), TaskItem(ENDDIALOG, 4, true));
  EXPECT_EQ(order.Get(7), TaskItem(QUITMESSAGELOOP, 5, true));
  EXPECT_EQ(order.Get(8), TaskItem(QUITMESSAGELOOP, 5, false));
  EXPECT_EQ(order.Get(9), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(10), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(11), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 3, false));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(14), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(15), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(16), TaskItem(RECURSIVE, 3, false));
}

}  // namespace

// This test occasionally hangs, would need to be turned into an
// interactive_ui_test, see http://crbug.com/44567.
TEST(SingleThreadTaskExecutorTest, DISABLED_NestingDenial2) {
  RunTest_NestingDenial2(MessagePumpType::DEFAULT);
  RunTest_NestingDenial2(MessagePumpType::UI);
  RunTest_NestingDenial2(MessagePumpType::IO);
}

// A side effect of this test is the generation a beep. Sorry.  This test also
// needs to process windows messages on the current thread.
TEST(SingleThreadTaskExecutorTest, NestingSupport2) {
  SingleThreadTaskExecutor executor(MessagePumpType::UI);

  Thread worker("NestingSupport2_worker");
  Thread::Options options;
  options.message_pump_type = MessagePumpType::UI;
  ASSERT_EQ(true, worker.StartWithOptions(std::move(options)));
  TaskList order;
  win::ScopedHandle event(CreateEvent(NULL, FALSE, FALSE, NULL));
  worker.task_runner()->PostTask(
      FROM_HERE, BindOnce(&RecursiveFuncWin, ThreadTaskRunnerHandle::Get(),
                          event.Get(), false, &order, true));
  // Let the other thread execute.
  WaitForSingleObject(event.Get(), INFINITE);
  RunLoop().Run();

  ASSERT_EQ(18u, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(MESSAGEBOX, 2, true));
  // Note that this executes in the MessageBox modal loop.
  EXPECT_EQ(order.Get(3), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(4), TaskItem(RECURSIVE, 3, false));
  EXPECT_EQ(order.Get(5), TaskItem(ENDDIALOG, 4, true));
  EXPECT_EQ(order.Get(6), TaskItem(ENDDIALOG, 4, false));
  EXPECT_EQ(order.Get(7), TaskItem(MESSAGEBOX, 2, false));
  /* The order can subtly change here. The reason is that when RecursiveFunc(1)
     is called in the main thread, if it is faster than getting to the
     PostTask(FROM_HERE, BindOnce(&QuitFunc) execution, the order of task
     execution can change. We don't care anyway that the order isn't correct.
  EXPECT_EQ(order.Get(8), TaskItem(QUITMESSAGELOOP, 5, true));
  EXPECT_EQ(order.Get(9), TaskItem(QUITMESSAGELOOP, 5, false));
  EXPECT_EQ(order.Get(10), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(11), TaskItem(RECURSIVE, 1, false));
  */
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 3, false));
  EXPECT_EQ(order.Get(14), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(15), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(16), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(17), TaskItem(RECURSIVE, 3, false));
}

#endif  // defined(OS_WIN)

#if defined(OS_WIN)
TEST(SingleThreadTaskExecutorTest, IOHandler) {
  RunTest_IOHandler();
}

TEST(SingleThreadTaskExecutorTest, HighResolutionTimer) {
  SingleThreadTaskExecutor executor;
  Time::EnableHighResolutionTimer(true);

  constexpr TimeDelta kFastTimer = TimeDelta::FromMilliseconds(5);
  constexpr TimeDelta kSlowTimer = TimeDelta::FromMilliseconds(100);

  {
    // Post a fast task to enable the high resolution timers.
    RunLoop run_loop;
    executor.task_runner()->PostDelayedTask(
        FROM_HERE,
        BindOnce(
            [](RunLoop* run_loop) {
              EXPECT_TRUE(Time::IsHighResolutionTimerInUse());
              run_loop->QuitWhenIdle();
            },
            &run_loop),
        kFastTimer);
    run_loop.Run();
  }
  EXPECT_FALSE(Time::IsHighResolutionTimerInUse());
  {
    // Check that a slow task does not trigger the high resolution logic.
    RunLoop run_loop;
    executor.task_runner()->PostDelayedTask(
        FROM_HERE,
        BindOnce(
            [](RunLoop* run_loop) {
              EXPECT_FALSE(Time::IsHighResolutionTimerInUse());
              run_loop->QuitWhenIdle();
            },
            &run_loop),
        kSlowTimer);
    run_loop.Run();
  }
  Time::EnableHighResolutionTimer(false);
  Time::ResetHighResolutionTimerUsage();
}

#endif  // defined(OS_WIN)

namespace {
// Inject a test point for recording the destructor calls for Closure objects
// send to MessageLoop::PostTask(). It is awkward usage since we are trying to
// hook the actual destruction, which is not a common operation.
class DestructionObserverProbe : public RefCounted<DestructionObserverProbe> {
 public:
  DestructionObserverProbe(bool* task_destroyed,
                           bool* destruction_observer_called)
      : task_destroyed_(task_destroyed),
        destruction_observer_called_(destruction_observer_called) {}
  virtual void Run() {
    // This task should never run.
    ADD_FAILURE();
  }

 private:
  friend class RefCounted<DestructionObserverProbe>;

  virtual ~DestructionObserverProbe() {
    EXPECT_FALSE(*destruction_observer_called_);
    *task_destroyed_ = true;
  }

  bool* task_destroyed_;
  bool* destruction_observer_called_;
};

class MLDestructionObserver : public CurrentThread::DestructionObserver {
 public:
  MLDestructionObserver(bool* task_destroyed, bool* destruction_observer_called)
      : task_destroyed_(task_destroyed),
        destruction_observer_called_(destruction_observer_called),
        task_destroyed_before_message_loop_(false) {}
  void WillDestroyCurrentMessageLoop() override {
    task_destroyed_before_message_loop_ = *task_destroyed_;
    *destruction_observer_called_ = true;
  }
  bool task_destroyed_before_message_loop() const {
    return task_destroyed_before_message_loop_;
  }

 private:
  bool* task_destroyed_;
  bool* destruction_observer_called_;
  bool task_destroyed_before_message_loop_;
};

}  // namespace

TEST(SingleThreadTaskExecutorTest, DestructionObserverTest) {
  // Verify that the destruction observer gets called at the very end (after
  // all the pending tasks have been destroyed).
  auto executor = std::make_unique<SingleThreadTaskExecutor>();
  const TimeDelta kDelay = TimeDelta::FromMilliseconds(100);

  bool task_destroyed = false;
  bool destruction_observer_called = false;

  MLDestructionObserver observer(&task_destroyed, &destruction_observer_called);
  CurrentThread::Get()->AddDestructionObserver(&observer);
  executor->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&DestructionObserverProbe::Run,
               base::MakeRefCounted<DestructionObserverProbe>(
                   &task_destroyed, &destruction_observer_called)),
      kDelay);
  executor.reset();
  EXPECT_TRUE(observer.task_destroyed_before_message_loop());
  // The task should have been destroyed when we deleted the loop.
  EXPECT_TRUE(task_destroyed);
  EXPECT_TRUE(destruction_observer_called);
}

// Verify that SingleThreadTaskExecutor sets ThreadMainTaskRunner::current() and
// it posts tasks on that message loop.
TEST(SingleThreadTaskExecutorTest, ThreadMainTaskRunner) {
  SingleThreadTaskExecutor executor;

  scoped_refptr<Foo> foo(new Foo());
  std::string a("a");
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test1ConstRef, foo, a));

  // Post quit task;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&RunLoop::QuitCurrentWhenIdleDeprecated));

  // Now kick things off
  RunLoop().Run();

  EXPECT_EQ(foo->test_count(), 1);
  EXPECT_EQ(foo->result(), "a");
}

TEST(SingleThreadTaskExecutorTest, type) {
  SingleThreadTaskExecutor executor(MessagePumpType::UI);
  EXPECT_EQ(executor.type(), MessagePumpType::UI);
}

#if defined(OS_WIN)
void EmptyFunction() {}

void PostMultipleTasks() {
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          base::BindOnce(&EmptyFunction));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          base::BindOnce(&EmptyFunction));
}

static const int kSignalMsg = WM_USER + 2;

void PostWindowsMessage(HWND message_hwnd) {
  PostMessage(message_hwnd, kSignalMsg, 0, 2);
}

void EndTest(bool* did_run, HWND hwnd) {
  *did_run = true;
  PostMessage(hwnd, WM_CLOSE, 0, 0);
}

int kMyMessageFilterCode = 0x5002;

LRESULT CALLBACK TestWndProcThunk(HWND hwnd,
                                  UINT message,
                                  WPARAM wparam,
                                  LPARAM lparam) {
  if (message == WM_CLOSE)
    EXPECT_TRUE(DestroyWindow(hwnd));
  if (message != kSignalMsg)
    return DefWindowProc(hwnd, message, wparam, lparam);

  switch (lparam) {
    case 1:
      // First, we post a task that will post multiple no-op tasks to make sure
      // that the pump's incoming task queue does not become empty during the
      // test.
      ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&PostMultipleTasks));
      // Next, we post a task that posts a windows message to trigger the second
      // stage of the test.
      ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&PostWindowsMessage, hwnd));
      break;
    case 2:
      // Since we're about to enter a modal loop, tell the message loop that we
      // intend to nest tasks.
      CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop
          allow_nestable_tasks;
      bool did_run = false;
      ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&EndTest, &did_run, hwnd));
      // Run a nested windows-style message loop and verify that our task runs.
      // If it doesn't, then we'll loop here until the test times out.
      MSG msg;
      while (GetMessage(&msg, 0, 0, 0)) {
        if (!CallMsgFilter(&msg, kMyMessageFilterCode))
          DispatchMessage(&msg);
        // If this message is a WM_CLOSE, explicitly exit the modal loop.
        // Posting a WM_QUIT should handle this, but unfortunately
        // MessagePumpWin eats WM_QUIT messages even when running inside a modal
        // loop.
        if (msg.message == WM_CLOSE)
          break;
      }
      EXPECT_TRUE(did_run);
      RunLoop::QuitCurrentWhenIdleDeprecated();
      break;
  }
  return 0;
}

TEST(SingleThreadTaskExecutorTest, AlwaysHaveUserMessageWhenNesting) {
  SingleThreadTaskExecutor executor(MessagePumpType::UI);
  HINSTANCE instance = CURRENT_MODULE();
  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = TestWndProcThunk;
  wc.hInstance = instance;
  wc.lpszClassName = L"SingleThreadTaskExecutorTest_HWND";
  ATOM atom = RegisterClassEx(&wc);
  ASSERT_TRUE(atom);

  HWND message_hwnd = CreateWindow(MAKEINTATOM(atom), 0, 0, 0, 0, 0, 0,
                                   HWND_MESSAGE, 0, instance, 0);
  ASSERT_TRUE(message_hwnd) << GetLastError();

  ASSERT_TRUE(PostMessage(message_hwnd, kSignalMsg, 0, 1));

  RunLoop().Run();

  ASSERT_TRUE(UnregisterClass(MAKEINTATOM(atom), instance));
}
#endif  // defined(OS_WIN)

// Verify that tasks posted to and code running in the scope of the same
// SingleThreadTaskExecutor access the same SequenceLocalStorage values.
TEST(SingleThreadTaskExecutorTest, SequenceLocalStorageSetGet) {
  SingleThreadTaskExecutor executor;

  SequenceLocalStorageSlot<int> slot;

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() { slot.emplace(11); }));

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() { EXPECT_EQ(*slot, 11); }));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(*slot, 11);
}

// Verify that tasks posted to and code running in different MessageLoops access
// different SequenceLocalStorage values.
TEST(SingleThreadTaskExecutorTest, SequenceLocalStorageDifferentMessageLoops) {
  SequenceLocalStorageSlot<int> slot;

  {
    SingleThreadTaskExecutor executor;
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, BindLambdaForTesting([&]() { slot.emplace(11); }));

    RunLoop().RunUntilIdle();
    EXPECT_EQ(*slot, 11);
  }

  SingleThreadTaskExecutor executor;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() { EXPECT_FALSE(slot); }));

  RunLoop().RunUntilIdle();
  EXPECT_NE(slot.GetOrCreateValue(), 11);
}

namespace {

class PostTaskOnDestroy {
 public:
  PostTaskOnDestroy(int times) : times_remaining_(times) {}
  ~PostTaskOnDestroy() { PostTaskWithPostingDestructor(times_remaining_); }

  // Post a task that will repost itself on destruction |times| times.
  static void PostTaskWithPostingDestructor(int times) {
    if (times > 0) {
      ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, BindOnce([](std::unique_ptr<PostTaskOnDestroy>) {},
                              std::make_unique<PostTaskOnDestroy>(times - 1)));
    }
  }

 private:
  const int times_remaining_;

  DISALLOW_COPY_AND_ASSIGN(PostTaskOnDestroy);
};

}  // namespace

// Test that SingleThreadTaskExecutor destruction handles a task's destructor
// posting another task.
TEST(SingleThreadTaskExecutorDestructionTest,
     DestroysFineWithPostTaskOnDestroy) {
  SingleThreadTaskExecutor executor;

  PostTaskOnDestroy::PostTaskWithPostingDestructor(10);
}

}  // namespace base

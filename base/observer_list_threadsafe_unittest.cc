// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list_threadsafe.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

constexpr int kThreadRunTime = 2000;  // ms to run the multi-threaded test.

class Foo {
 public:
  virtual void Observe(int x) = 0;
  virtual ~Foo() = default;
  virtual int GetValue() const { return 0; }
};

class Adder : public Foo {
 public:
  explicit Adder(int scaler) : total(0), scaler_(scaler) {}
  ~Adder() override = default;

  void Observe(int x) override { total += x * scaler_; }
  int GetValue() const override { return total; }

  int total;

 private:
  int scaler_;
};

class AddInObserve : public Foo {
 public:
  explicit AddInObserve(ObserverListThreadSafe<Foo>* observer_list)
      : observer_list(observer_list), to_add_() {}

  void SetToAdd(Foo* to_add) { to_add_ = to_add; }

  void Observe(int x) override {
    if (to_add_) {
      observer_list->AddObserver(to_add_);
      to_add_ = nullptr;
    }
  }

  ObserverListThreadSafe<Foo>* observer_list;
  Foo* to_add_;
};

// A thread for use in the ThreadSafeObserver test which will add and remove
// itself from the notification list repeatedly.
class AddRemoveThread : public PlatformThread::Delegate, public Foo {
 public:
  AddRemoveThread(ObserverListThreadSafe<Foo>* list,
                  bool notify,
                  WaitableEvent* ready)
      : list_(list),
        loop_(nullptr),
        in_list_(false),
        start_(Time::Now()),
        count_observes_(0),
        count_addtask_(0),
        do_notifies_(notify),
        ready_(ready),
        weak_factory_(this) {}

  ~AddRemoveThread() override = default;

  void ThreadMain() override {
    loop_ = new MessageLoop();  // Fire up a message loop.
    loop_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AddRemoveThread::AddTask, weak_factory_.GetWeakPtr()));
    ready_->Signal();
    // After ready_ is signaled, loop_ is only accessed by the main test thread
    // (i.e. not this thread) in particular by Quit() which causes Run() to
    // return, and we "control" loop_ again.
    RunLoop run_loop;
    quit_loop_ = run_loop.QuitClosure();
    run_loop.Run();
    delete loop_;
    loop_ = reinterpret_cast<MessageLoop*>(0xdeadbeef);
    delete this;
  }

  // This task just keeps posting to itself in an attempt to race with the
  // notifier.
  void AddTask() {
    count_addtask_++;

    if ((Time::Now() - start_).InMilliseconds() > kThreadRunTime) {
      VLOG(1) << "DONE!";
      return;
    }

    if (!in_list_) {
      list_->AddObserver(this);
      in_list_ = true;
    }

    if (do_notifies_) {
      list_->Notify(FROM_HERE, &Foo::Observe, 10);
    }

    loop_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AddRemoveThread::AddTask, weak_factory_.GetWeakPtr()));
  }

  // This function is only callable from the main thread.
  void Quit() { std::move(quit_loop_).Run(); }

  void Observe(int x) override {
    count_observes_++;

    // If we're getting called after we removed ourselves from the list, that is
    // very bad!
    EXPECT_TRUE(in_list_);

    // This callback should fire on the appropriate thread
    EXPECT_TRUE(loop_->IsBoundToCurrentThread());

    list_->RemoveObserver(this);
    in_list_ = false;
  }

 private:
  ObserverListThreadSafe<Foo>* list_;
  MessageLoop* loop_;
  bool in_list_;  // Are we currently registered for notifications.
                  // in_list_ is only used on |this| thread.
  Time start_;    // The time we started the test.

  int count_observes_;  // Number of times we observed.
  int count_addtask_;   // Number of times thread AddTask was called
  bool do_notifies_;    // Whether these threads should do notifications.
  WaitableEvent* ready_;

  base::OnceClosure quit_loop_;

  base::WeakPtrFactory<AddRemoveThread> weak_factory_;
};

}  // namespace

TEST(ObserverListThreadSafeTest, BasicTest) {
  MessageLoop loop;

  scoped_refptr<ObserverListThreadSafe<Foo>> observer_list(
      new ObserverListThreadSafe<Foo>);
  Adder a(1);
  Adder b(-1);
  Adder c(1);
  Adder d(-1);

  observer_list->AddObserver(&a);
  observer_list->AddObserver(&b);

  observer_list->Notify(FROM_HERE, &Foo::Observe, 10);
  RunLoop().RunUntilIdle();

  observer_list->AddObserver(&c);
  observer_list->AddObserver(&d);

  observer_list->Notify(FROM_HERE, &Foo::Observe, 10);
  observer_list->RemoveObserver(&c);
  RunLoop().RunUntilIdle();

  EXPECT_EQ(20, a.total);
  EXPECT_EQ(-20, b.total);
  EXPECT_EQ(0, c.total);
  EXPECT_EQ(-10, d.total);
}

TEST(ObserverListThreadSafeTest, RemoveObserver) {
  MessageLoop loop;

  scoped_refptr<ObserverListThreadSafe<Foo>> observer_list(
      new ObserverListThreadSafe<Foo>);
  Adder a(1), b(1);

  // A workaround for the compiler bug. See http://crbug.com/121960.
  EXPECT_NE(&a, &b);

  // Should do nothing.
  observer_list->RemoveObserver(&a);
  observer_list->RemoveObserver(&b);

  observer_list->Notify(FROM_HERE, &Foo::Observe, 10);
  RunLoop().RunUntilIdle();

  EXPECT_EQ(0, a.total);
  EXPECT_EQ(0, b.total);

  observer_list->AddObserver(&a);

  // Should also do nothing.
  observer_list->RemoveObserver(&b);

  observer_list->Notify(FROM_HERE, &Foo::Observe, 10);
  RunLoop().RunUntilIdle();

  EXPECT_EQ(10, a.total);
  EXPECT_EQ(0, b.total);
}

TEST(ObserverListThreadSafeTest, WithoutSequence) {
  scoped_refptr<ObserverListThreadSafe<Foo>> observer_list(
      new ObserverListThreadSafe<Foo>);

  Adder a(1), b(1), c(1);

  // No sequence, so these should not be added.
  observer_list->AddObserver(&a);
  observer_list->AddObserver(&b);

  {
    // Add c when there's a sequence.
    MessageLoop loop;
    observer_list->AddObserver(&c);

    observer_list->Notify(FROM_HERE, &Foo::Observe, 10);
    RunLoop().RunUntilIdle();

    EXPECT_EQ(0, a.total);
    EXPECT_EQ(0, b.total);
    EXPECT_EQ(10, c.total);

    // Now add a when there's a sequence.
    observer_list->AddObserver(&a);

    // Remove c when there's a sequence.
    observer_list->RemoveObserver(&c);

    // Notify again.
    observer_list->Notify(FROM_HERE, &Foo::Observe, 20);
    RunLoop().RunUntilIdle();

    EXPECT_EQ(20, a.total);
    EXPECT_EQ(0, b.total);
    EXPECT_EQ(10, c.total);
  }

  // Removing should always succeed with or without a sequence.
  observer_list->RemoveObserver(&a);

  // Notifying should not fail but should also be a no-op.
  MessageLoop loop;
  observer_list->AddObserver(&b);
  observer_list->Notify(FROM_HERE, &Foo::Observe, 30);
  RunLoop().RunUntilIdle();

  EXPECT_EQ(20, a.total);
  EXPECT_EQ(30, b.total);
  EXPECT_EQ(10, c.total);
}

class FooRemover : public Foo {
 public:
  explicit FooRemover(ObserverListThreadSafe<Foo>* list) : list_(list) {}
  ~FooRemover() override = default;

  void AddFooToRemove(Foo* foo) { foos_.push_back(foo); }

  void Observe(int x) override {
    std::vector<Foo*> tmp;
    tmp.swap(foos_);
    for (auto* it : tmp) {
      list_->RemoveObserver(it);
    }
  }

 private:
  const scoped_refptr<ObserverListThreadSafe<Foo>> list_;
  std::vector<Foo*> foos_;
};

TEST(ObserverListThreadSafeTest, RemoveMultipleObservers) {
  MessageLoop loop;
  scoped_refptr<ObserverListThreadSafe<Foo>> observer_list(
      new ObserverListThreadSafe<Foo>);

  FooRemover a(observer_list.get());
  Adder b(1);

  observer_list->AddObserver(&a);
  observer_list->AddObserver(&b);

  a.AddFooToRemove(&a);
  a.AddFooToRemove(&b);

  observer_list->Notify(FROM_HERE, &Foo::Observe, 1);
  RunLoop().RunUntilIdle();
}

// A test driver for a multi-threaded notification loop.  Runs a number of
// observer threads, each of which constantly adds/removes itself from the
// observer list.  Optionally, if cross_thread_notifies is set to true, the
// observer threads will also trigger notifications to all observers.
static void ThreadSafeObserverHarness(int num_threads,
                                      bool cross_thread_notifies) {
  MessageLoop loop;

  scoped_refptr<ObserverListThreadSafe<Foo>> observer_list(
      new ObserverListThreadSafe<Foo>);
  Adder a(1);
  Adder b(-1);

  observer_list->AddObserver(&a);
  observer_list->AddObserver(&b);

  std::vector<AddRemoveThread*> threaded_observer;
  std::vector<base::PlatformThreadHandle> threads(num_threads);
  std::vector<std::unique_ptr<base::WaitableEvent>> ready;
  threaded_observer.reserve(num_threads);
  ready.reserve(num_threads);
  for (int index = 0; index < num_threads; index++) {
    ready.push_back(std::make_unique<WaitableEvent>(
        WaitableEvent::ResetPolicy::MANUAL,
        WaitableEvent::InitialState::NOT_SIGNALED));
    threaded_observer.push_back(new AddRemoveThread(
        observer_list.get(), cross_thread_notifies, ready.back().get()));
    EXPECT_TRUE(
        PlatformThread::Create(0, threaded_observer.back(), &threads[index]));
  }
  ASSERT_EQ(static_cast<size_t>(num_threads), threaded_observer.size());
  ASSERT_EQ(static_cast<size_t>(num_threads), ready.size());

  // This makes sure that threaded_observer has gotten to set loop_, so that we
  // can call Quit() below safe-ish-ly.
  for (int i = 0; i < num_threads; ++i)
    ready[i]->Wait();

  Time start = Time::Now();
  while (true) {
    if ((Time::Now() - start).InMilliseconds() > kThreadRunTime)
      break;

    observer_list->Notify(FROM_HERE, &Foo::Observe, 10);

    RunLoop().RunUntilIdle();
  }

  for (int index = 0; index < num_threads; index++) {
    threaded_observer[index]->Quit();
    PlatformThread::Join(threads[index]);
  }
}

#if defined(OS_FUCHSIA)
// TODO(crbug.com/738275): This is flaky on Fuchsia.
#define MAYBE_CrossThreadObserver DISABLED_CrossThreadObserver
#else
#define MAYBE_CrossThreadObserver CrossThreadObserver
#endif
TEST(ObserverListThreadSafeTest, MAYBE_CrossThreadObserver) {
  // Use 7 observer threads.  Notifications only come from the main thread.
  ThreadSafeObserverHarness(7, false);
}

TEST(ObserverListThreadSafeTest, CrossThreadNotifications) {
  // Use 3 observer threads.  Notifications will fire from the main thread and
  // all 3 observer threads.
  ThreadSafeObserverHarness(3, true);
}

TEST(ObserverListThreadSafeTest, OutlivesMessageLoop) {
  MessageLoop* loop = new MessageLoop;
  scoped_refptr<ObserverListThreadSafe<Foo>> observer_list(
      new ObserverListThreadSafe<Foo>);

  Adder a(1);
  observer_list->AddObserver(&a);
  delete loop;
  // Test passes if we don't crash here.
  observer_list->Notify(FROM_HERE, &Foo::Observe, 1);
}

namespace {

class SequenceVerificationObserver : public Foo {
 public:
  explicit SequenceVerificationObserver(
      scoped_refptr<SequencedTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}
  ~SequenceVerificationObserver() override = default;

  void Observe(int x) override {
    called_on_valid_sequence_ = task_runner_->RunsTasksInCurrentSequence();
  }

  bool called_on_valid_sequence() const { return called_on_valid_sequence_; }

 private:
  const scoped_refptr<SequencedTaskRunner> task_runner_;
  bool called_on_valid_sequence_ = false;

  DISALLOW_COPY_AND_ASSIGN(SequenceVerificationObserver);
};

}  // namespace

// Verify that observers are notified on the correct sequence.
TEST(ObserverListThreadSafeTest, NotificationOnValidSequence) {
  test::ScopedTaskEnvironment scoped_task_environment;

  auto task_runner_1 = CreateSequencedTaskRunnerWithTraits(TaskTraits());
  auto task_runner_2 = CreateSequencedTaskRunnerWithTraits(TaskTraits());

  auto observer_list = MakeRefCounted<ObserverListThreadSafe<Foo>>();

  SequenceVerificationObserver observer_1(task_runner_1);
  SequenceVerificationObserver observer_2(task_runner_2);

  task_runner_1->PostTask(FROM_HERE,
                          BindOnce(&ObserverListThreadSafe<Foo>::AddObserver,
                                   observer_list, Unretained(&observer_1)));
  task_runner_2->PostTask(FROM_HERE,
                          BindOnce(&ObserverListThreadSafe<Foo>::AddObserver,
                                   observer_list, Unretained(&observer_2)));

  TaskScheduler::GetInstance()->FlushForTesting();

  observer_list->Notify(FROM_HERE, &Foo::Observe, 1);

  TaskScheduler::GetInstance()->FlushForTesting();

  EXPECT_TRUE(observer_1.called_on_valid_sequence());
  EXPECT_TRUE(observer_2.called_on_valid_sequence());
}

// Verify that when an observer is added to a NOTIFY_ALL ObserverListThreadSafe
// from a notification, it is itself notified.
TEST(ObserverListThreadSafeTest, AddObserverFromNotificationNotifyAll) {
  test::ScopedTaskEnvironment scoped_task_environment;
  auto observer_list = MakeRefCounted<ObserverListThreadSafe<Foo>>();

  Adder observer_added_from_notification(1);

  AddInObserve initial_observer(observer_list.get());
  initial_observer.SetToAdd(&observer_added_from_notification);
  observer_list->AddObserver(&initial_observer);

  observer_list->Notify(FROM_HERE, &Foo::Observe, 1);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, observer_added_from_notification.GetValue());
}

namespace {

class RemoveWhileNotificationIsRunningObserver : public Foo {
 public:
  RemoveWhileNotificationIsRunningObserver()
      : notification_running_(WaitableEvent::ResetPolicy::AUTOMATIC,
                              WaitableEvent::InitialState::NOT_SIGNALED),
        barrier_(WaitableEvent::ResetPolicy::AUTOMATIC,
                 WaitableEvent::InitialState::NOT_SIGNALED) {}
  ~RemoveWhileNotificationIsRunningObserver() override = default;

  void Observe(int x) override {
    notification_running_.Signal();
    ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
    barrier_.Wait();
  }

  void WaitForNotificationRunning() { notification_running_.Wait(); }
  void Unblock() { barrier_.Signal(); }

 private:
  WaitableEvent notification_running_;
  WaitableEvent barrier_;

  DISALLOW_COPY_AND_ASSIGN(RemoveWhileNotificationIsRunningObserver);
};

}  // namespace

// Verify that there is no crash when an observer is removed while it is being
// notified.
TEST(ObserverListThreadSafeTest, RemoveWhileNotificationIsRunning) {
  auto observer_list = MakeRefCounted<ObserverListThreadSafe<Foo>>();
  RemoveWhileNotificationIsRunningObserver observer;

  WaitableEvent task_running(WaitableEvent::ResetPolicy::AUTOMATIC,
                             WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent barrier(WaitableEvent::ResetPolicy::AUTOMATIC,
                        WaitableEvent::InitialState::NOT_SIGNALED);

  // This must be after the declaration of |barrier| so that tasks posted to
  // TaskScheduler can safely use |barrier|.
  test::ScopedTaskEnvironment scoped_task_environment;

  CreateSequencedTaskRunnerWithTraits({MayBlock()})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&ObserverListThreadSafe<Foo>::AddObserver,
                                observer_list, Unretained(&observer)));
  TaskScheduler::GetInstance()->FlushForTesting();

  observer_list->Notify(FROM_HERE, &Foo::Observe, 1);
  observer.WaitForNotificationRunning();
  observer_list->RemoveObserver(&observer);

  observer.Unblock();
}

// Same as ObserverListTest.Existing, but for ObserverListThreadSafe
TEST(ObserverListThreadSafeTest, Existing) {
  MessageLoop loop;
  scoped_refptr<ObserverListThreadSafe<Foo>> observer_list(
      new ObserverListThreadSafe<Foo>(ObserverListPolicy::EXISTING_ONLY));
  Adder a(1);
  AddInObserve b(observer_list.get());
  Adder c(1);
  b.SetToAdd(&c);

  observer_list->AddObserver(&a);
  observer_list->AddObserver(&b);

  observer_list->Notify(FROM_HERE, &Foo::Observe, 1);
  RunLoop().RunUntilIdle();

  EXPECT_FALSE(b.to_add_);
  // B's adder should not have been notified because it was added during
  // notification.
  EXPECT_EQ(0, c.total);

  // Notify again to make sure b's adder is notified.
  observer_list->Notify(FROM_HERE, &Foo::Observe, 1);
  RunLoop().RunUntilIdle();
  EXPECT_EQ(1, c.total);
}

}  // namespace base

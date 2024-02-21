// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list_threadsafe.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

constexpr int kThreadRunTime = 1000;  // ms to run the multi-threaded test.

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
      observer_list->AddObserver(to_add_.get());
      to_add_ = nullptr;
    }
  }

  raw_ptr<ObserverListThreadSafe<Foo>> observer_list;
  raw_ptr<Foo> to_add_;
};

// A task for use in the ThreadSafeObserver test which will add and remove
// itself from the notification list repeatedly.
template <RemoveObserverPolicy RemovePolicy =
              RemoveObserverPolicy::kAnySequence>
class AddRemoveThread : public Foo {
  using Self = AddRemoveThread<RemovePolicy>;
  using ObserverList = ObserverListThreadSafe<Foo, RemovePolicy>;

 public:
  AddRemoveThread(ObserverList* list,
                  bool notify,
                  scoped_refptr<SingleThreadTaskRunner> removal_task_runner)
      : list_(list),
        task_runner_(ThreadPool::CreateSingleThreadTaskRunner(
            {},
            SingleThreadTaskRunnerThreadMode::DEDICATED)),
        removal_task_runner_(std::move(removal_task_runner)),
        in_list_(false),
        start_(Time::Now()),
        do_notifies_(notify) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Self::AddTask, weak_factory_.GetWeakPtr()));
  }

  ~AddRemoveThread() override = default;

  // This task just keeps posting to itself in an attempt to race with the
  // notifier.
  void AddTask() {
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

    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&Self::AddTask, weak_factory_.GetWeakPtr()));
  }

  void RemoveTask() {
    list_->RemoveObserver(this);
    in_list_ = false;
  }

  void Observe(int x) override {
    // If we're getting called after we removed ourselves from the list, that is
    // very bad!
    EXPECT_TRUE(in_list_);

    // This callback should fire on the appropriate thread
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());

    if (removal_task_runner_) {
      // Remove the observer on a different thread, blocking the current thread
      // until it's removed. Unretained is safe since the pointers are valid
      // until the thread is unblocked.
      base::TestWaitableEvent event;
      removal_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&Self::RemoveTask, base::Unretained(this))
                         .Then(base::BindOnce(&base::TestWaitableEvent::Signal,
                                              base::Unretained(&event))));
      event.Wait();
    } else {
      // Remove the observer on the same thread.
      RemoveTask();
    }
  }

  scoped_refptr<SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

 private:
  raw_ptr<ObserverList> list_;
  scoped_refptr<SingleThreadTaskRunner> task_runner_;
  // Optional task runner used to remove observers. This will be the main task
  // runner of a different AddRemoveThread.
  scoped_refptr<SingleThreadTaskRunner> removal_task_runner_;
  bool in_list_;  // Are we currently registered for notifications.
                  // in_list_ is only used on |this| thread.
  Time start_;    // The time we started the test.

  bool do_notifies_;    // Whether these threads should do notifications.

  base::WeakPtrFactory<Self> weak_factory_{this};
};

}  // namespace

TEST(ObserverListThreadSafeTest, BasicTest) {
  using List = ObserverListThreadSafe<Foo>;
  test::TaskEnvironment task_environment;

  scoped_refptr<List> observer_list(new List);
  Adder a(1);
  Adder b(-1);
  Adder c(1);
  Adder d(-1);

  List::AddObserverResult result;

  result = observer_list->AddObserver(&a);
  EXPECT_EQ(result, List::AddObserverResult::kBecameNonEmpty);
  result = observer_list->AddObserver(&b);
  EXPECT_EQ(result, List::AddObserverResult::kWasAlreadyNonEmpty);

  observer_list->Notify(FROM_HERE, &Foo::Observe, 10);
  RunLoop().RunUntilIdle();

  result = observer_list->AddObserver(&c);
  EXPECT_EQ(result, List::AddObserverResult::kWasAlreadyNonEmpty);
  result = observer_list->AddObserver(&d);
  EXPECT_EQ(result, List::AddObserverResult::kWasAlreadyNonEmpty);

  observer_list->Notify(FROM_HERE, &Foo::Observe, 10);
  observer_list->RemoveObserver(&c);
  RunLoop().RunUntilIdle();

  EXPECT_EQ(20, a.total);
  EXPECT_EQ(-20, b.total);
  EXPECT_EQ(0, c.total);
  EXPECT_EQ(-10, d.total);
}

TEST(ObserverListThreadSafeTest, RemoveObserver) {
  using List = ObserverListThreadSafe<Foo>;
  test::TaskEnvironment task_environment;

  scoped_refptr<List> observer_list(new List);
  Adder a(1), b(1);

  // A workaround for the compiler bug. See http://crbug.com/121960.
  EXPECT_NE(&a, &b);

  List::RemoveObserverResult result;

  // Should do nothing.
  result = observer_list->RemoveObserver(&a);
  EXPECT_EQ(result, List::RemoveObserverResult::kWasOrBecameEmpty);
  result = observer_list->RemoveObserver(&b);
  EXPECT_EQ(result, List::RemoveObserverResult::kWasOrBecameEmpty);

  observer_list->Notify(FROM_HERE, &Foo::Observe, 10);
  RunLoop().RunUntilIdle();

  EXPECT_EQ(0, a.total);
  EXPECT_EQ(0, b.total);

  observer_list->AddObserver(&a);

  // Should also do nothing.
  result = observer_list->RemoveObserver(&b);
  EXPECT_EQ(result, List::RemoveObserverResult::kRemainsNonEmpty);

  observer_list->Notify(FROM_HERE, &Foo::Observe, 10);
  RunLoop().RunUntilIdle();

  EXPECT_EQ(10, a.total);
  EXPECT_EQ(0, b.total);

  result = observer_list->RemoveObserver(&a);
  EXPECT_EQ(result, List::RemoveObserverResult::kWasOrBecameEmpty);
}

class FooRemover : public Foo {
 public:
  explicit FooRemover(ObserverListThreadSafe<Foo>* list) : list_(list) {}
  ~FooRemover() override = default;

  void AddFooToRemove(Foo* foo) { foos_.push_back(foo); }

  void Observe(int x) override {
    std::vector<raw_ptr<Foo, VectorExperimental>> tmp;
    tmp.swap(foos_);
    for (Foo* it : tmp) {
      list_->RemoveObserver(it);
    }
  }

 private:
  const scoped_refptr<ObserverListThreadSafe<Foo>> list_;
  std::vector<raw_ptr<Foo, VectorExperimental>> foos_;
};

TEST(ObserverListThreadSafeTest, RemoveMultipleObservers) {
  test::TaskEnvironment task_environment;
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
// observer list.  Optionally, if `cross_thread_notifies` is set to true, the
// observer threads will also trigger notifications to all observers, and if
// `cross_thread_removes` is set to true, the observer threads will also remove
// observers added by other threads.
template <
    RemoveObserverPolicy RemovePolicy = RemoveObserverPolicy::kAnySequence>
static void ThreadSafeObserverHarness(int num_threads,
                                      bool cross_thread_notifies = false,
                                      bool cross_thread_removes = false) {
  test::TaskEnvironment task_environment;

  auto observer_list =
      base::MakeRefCounted<ObserverListThreadSafe<Foo, RemovePolicy>>();

  Adder a(1);
  Adder b(-1);

  observer_list->AddObserver(&a);
  observer_list->AddObserver(&b);

  using TestThread = AddRemoveThread<RemovePolicy>;
  std::vector<std::unique_ptr<TestThread>> threaded_observers;
  threaded_observers.reserve(num_threads);
  scoped_refptr<SingleThreadTaskRunner> removal_task_runner;
  for (int index = 0; index < num_threads; index++) {
    auto add_remove_thread =
        std::make_unique<TestThread>(observer_list.get(), cross_thread_notifies,
                                     std::move(removal_task_runner));
    if (cross_thread_removes) {
      // Save the task runner to pass to the next thread.
      removal_task_runner = add_remove_thread->task_runner();
    }
    threaded_observers.push_back(std::move(add_remove_thread));
  }
  ASSERT_EQ(static_cast<size_t>(num_threads), threaded_observers.size());

  Time start = Time::Now();
  while (true) {
    if ((Time::Now() - start).InMilliseconds() > kThreadRunTime)
      break;

    observer_list->Notify(FROM_HERE, &Foo::Observe, 10);

    RunLoop().RunUntilIdle();
  }

  task_environment.RunUntilIdle();
}

TEST(ObserverListThreadSafeTest, CrossThreadObserver) {
  // Use 7 observer threads.  Notifications only come from the main thread.
  ThreadSafeObserverHarness(7);
}

TEST(ObserverListThreadSafeTest, CrossThreadNotifications) {
  // Use 3 observer threads.  Notifications will fire from the main thread and
  // all 3 observer threads.
  ThreadSafeObserverHarness(3, /*cross_thread_notifies=*/true);
}

TEST(ObserverListThreadSafeTest, CrossThreadRemoval) {
  // Use 3 observer threads. Observers can be removed from any thread.
  ThreadSafeObserverHarness(3, /*cross_thread_notifies=*/true,
                            /*cross_thread_removes=*/true);
}

TEST(ObserverListThreadSafeTest, CrossThreadRemovalRestricted) {
  // Use 3 observer threads. Observers must be removed from the thread that
  // added them. This should succeed because the test doesn't break that
  // restriction.
  ThreadSafeObserverHarness<RemoveObserverPolicy::kAddingSequenceOnly>(
      3, /*cross_thread_notifies=*/true, /*cross_thread_removes=*/false);
}

TEST(ObserverListThreadSafeDeathTest, CrossThreadRemovalRestricted) {
  // Use 3 observer threads. Observers must be removed from the thread that
  // added them. This should CHECK because the test breaks that restriction.
  EXPECT_CHECK_DEATH(
      ThreadSafeObserverHarness<RemoveObserverPolicy::kAddingSequenceOnly>(
          3, /*cross_thread_notifies=*/true, /*cross_thread_removes=*/true));
}

TEST(ObserverListThreadSafeTest, OutlivesTaskEnvironment) {
  std::optional<test::TaskEnvironment> task_environment(std::in_place);
  auto observer_list = base::MakeRefCounted<ObserverListThreadSafe<Foo>>();

  Adder a(1);
  observer_list->AddObserver(&a);
  task_environment.reset();
  // Test passes if we don't crash here.
  observer_list->Notify(FROM_HERE, &Foo::Observe, 1);
  observer_list->RemoveObserver(&a);
}

TEST(ObserverListThreadSafeTest, OutlivesTaskEnvironmentRemovalRestricted) {
  std::optional<test::TaskEnvironment> task_environment(std::in_place);
  auto observer_list = base::MakeRefCounted<
      ObserverListThreadSafe<Foo, RemoveObserverPolicy::kAddingSequenceOnly>>();

  Adder a(1);
  observer_list->AddObserver(&a);
  task_environment.reset();
  // Test passes if we don't crash here.
  observer_list->Notify(FROM_HERE, &Foo::Observe, 1);
  observer_list->RemoveObserver(&a);
}

namespace {

class SequenceVerificationObserver : public Foo {
 public:
  explicit SequenceVerificationObserver(
      scoped_refptr<SequencedTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}
  SequenceVerificationObserver(const SequenceVerificationObserver&) = delete;
  SequenceVerificationObserver& operator=(const SequenceVerificationObserver&) =
      delete;
  ~SequenceVerificationObserver() override = default;

  void Observe(int x) override {
    called_on_valid_sequence_ = task_runner_->RunsTasksInCurrentSequence();
  }

  bool called_on_valid_sequence() const { return called_on_valid_sequence_; }

 private:
  const scoped_refptr<SequencedTaskRunner> task_runner_;
  bool called_on_valid_sequence_ = false;
};

}  // namespace

// Verify that observers are notified on the correct sequence.
TEST(ObserverListThreadSafeTest, NotificationOnValidSequence) {
  test::TaskEnvironment task_environment;

  auto task_runner_1 = ThreadPool::CreateSequencedTaskRunner({});
  auto task_runner_2 = ThreadPool::ThreadPool::CreateSequencedTaskRunner({});

  auto observer_list = MakeRefCounted<ObserverListThreadSafe<Foo>>();

  SequenceVerificationObserver observer_1(task_runner_1);
  SequenceVerificationObserver observer_2(task_runner_2);

  task_runner_1->PostTask(
      FROM_HERE,
      BindOnce(base::IgnoreResult(&ObserverListThreadSafe<Foo>::AddObserver),
               observer_list, Unretained(&observer_1)));
  task_runner_2->PostTask(
      FROM_HERE,
      BindOnce(base::IgnoreResult(&ObserverListThreadSafe<Foo>::AddObserver),
               observer_list, Unretained(&observer_2)));

  ThreadPoolInstance::Get()->FlushForTesting();

  observer_list->Notify(FROM_HERE, &Foo::Observe, 1);

  ThreadPoolInstance::Get()->FlushForTesting();

  EXPECT_TRUE(observer_1.called_on_valid_sequence());
  EXPECT_TRUE(observer_2.called_on_valid_sequence());
}

// Verify that when an observer is added to a NOTIFY_ALL ObserverListThreadSafe
// from a notification, it is itself notified.
TEST(ObserverListThreadSafeTest, AddObserverFromNotificationNotifyAll) {
  test::TaskEnvironment task_environment;
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
  RemoveWhileNotificationIsRunningObserver(
      const RemoveWhileNotificationIsRunningObserver&) = delete;
  RemoveWhileNotificationIsRunningObserver& operator=(
      const RemoveWhileNotificationIsRunningObserver&) = delete;
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
  // ThreadPool can safely use |barrier|.
  test::TaskEnvironment task_environment;

  ThreadPool::CreateSequencedTaskRunner({MayBlock()})
      ->PostTask(FROM_HERE,
                 base::BindOnce(base::IgnoreResult(
                                    &ObserverListThreadSafe<Foo>::AddObserver),
                                observer_list, Unretained(&observer)));
  ThreadPoolInstance::Get()->FlushForTesting();

  observer_list->Notify(FROM_HERE, &Foo::Observe, 1);
  observer.WaitForNotificationRunning();
  observer_list->RemoveObserver(&observer);

  observer.Unblock();
}

TEST(ObserverListThreadSafeTest, AddRemoveWithPendingNotifications) {
  test::TaskEnvironment task_environment;

  scoped_refptr<ObserverListThreadSafe<Foo>> observer_list(
      new ObserverListThreadSafe<Foo>);
  Adder a(1);
  Adder b(1);

  observer_list->AddObserver(&a);
  observer_list->AddObserver(&b);

  // Remove observer `a` while there is a pending notification for observer `a`.
  observer_list->Notify(FROM_HERE, &Foo::Observe, 10);
  observer_list->RemoveObserver(&a);
  RunLoop().RunUntilIdle();
  observer_list->AddObserver(&a);

  EXPECT_EQ(0, a.total);
  EXPECT_EQ(10, b.total);

  // Remove and re-adding observer `a` while there is a pending notification for
  // observer `a`. The notification to `a` must not be executed since it was
  // sent before the removal of `a`.
  observer_list->Notify(FROM_HERE, &Foo::Observe, 10);
  observer_list->RemoveObserver(&a);
  observer_list->AddObserver(&a);
  RunLoop().RunUntilIdle();

  EXPECT_EQ(0, a.total);
  EXPECT_EQ(20, b.total);

  // Observer `a` and `b` are present and should both receive a notification.
  observer_list->RemoveObserver(&a);
  observer_list->AddObserver(&a);
  observer_list->Notify(FROM_HERE, &Foo::Observe, 10);
  RunLoop().RunUntilIdle();

  EXPECT_EQ(10, a.total);
  EXPECT_EQ(30, b.total);
}

// Same as ObserverListTest.Existing, but for ObserverListThreadSafe
TEST(ObserverListThreadSafeTest, Existing) {
  test::TaskEnvironment task_environment;
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

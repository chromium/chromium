// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/promise/promise.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/do_nothing_promise.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace base {
namespace {

void RecordOrder(std::vector<int>* run_order, int order) {
  run_order->push_back(order);
}

class ObjectToDelete : public RefCounted<ObjectToDelete> {
 public:
  // |delete_flag| is set to true when this object is deleted
  ObjectToDelete(bool* delete_flag) : delete_flag_(delete_flag) {
    EXPECT_FALSE(*delete_flag_);
  }

 private:
  friend class RefCounted<ObjectToDelete>;
  ~ObjectToDelete() { *delete_flag_ = true; }

  bool* const delete_flag_;

  DISALLOW_COPY_AND_ASSIGN(ObjectToDelete);
};

class MockObject {
 public:
  MockObject() = default;

  void Task(scoped_refptr<ObjectToDelete>) {}
  void Reply(scoped_refptr<ObjectToDelete>) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockObject);
};

struct DummyError {};

struct Cancelable {
  Cancelable() {}

  void LogTask(std::vector<std::string>* log, std::string value) {
    log->push_back(value);
  }

  void NopTask() {}

  WeakPtrFactory<Cancelable> weak_ptr_factory{this};
};

}  // namespace

class PromiseTest : public testing::Test {
 public:
  test::TaskEnvironment task_environment_;
};

TEST(PromiseMemoryLeakTest, TargetTaskRunnerClearsTasks) {
  scoped_refptr<TestMockTimeTaskRunner> post_runner =
      MakeRefCounted<TestMockTimeTaskRunner>();
  scoped_refptr<TestMockTimeTaskRunner> reply_runner =
      MakeRefCounted<TestMockTimeTaskRunner>(
          TestMockTimeTaskRunner::Type::kBoundToThread);
  MockObject mock_object;
  bool delete_task_flag = false;
  bool delete_reply_flag = false;

  Promise<int>::CreateResolved(FROM_HERE, 42)
      .ThenOn(post_runner, FROM_HERE,
              BindOnce(&MockObject::Task, Unretained(&mock_object),
                       MakeRefCounted<ObjectToDelete>(&delete_task_flag)))
      .ThenHere(FROM_HERE,
                BindOnce(&MockObject::Reply, Unretained(&mock_object),
                         MakeRefCounted<ObjectToDelete>(&delete_reply_flag)));

  post_runner->ClearPendingTasks();

  post_runner = nullptr;
  reply_runner = nullptr;

  EXPECT_TRUE(delete_task_flag);
  EXPECT_TRUE(delete_reply_flag);
}

TEST(PromiseMemoryLeakTest, GetResolveCallbackNeverRun) {
  test::TaskEnvironment task_environment_;
  OnceCallback<void(int)> cb;
  MockObject mock_object;
  bool delete_task_flag = false;

  {
    ManualPromiseResolver<int> p(FROM_HERE);
    cb = p.GetResolveCallback();

    p.promise().ThenHere(
        FROM_HERE, BindOnce(&MockObject::Task, Unretained(&mock_object),
                            MakeRefCounted<ObjectToDelete>(&delete_task_flag)));
  }

  EXPECT_FALSE(delete_task_flag);
  cb = OnceCallback<void(int)>();
  EXPECT_TRUE(delete_task_flag);
}

TEST(PromiseMemoryLeakTest, GetRepeatingResolveCallbackNeverRun) {
  test::TaskEnvironment task_environment_;
  RepeatingCallback<void(int)> cb;
  MockObject mock_object;
  bool delete_task_flag = false;

  {
    ManualPromiseResolver<int> p(FROM_HERE);
    cb = p.GetRepeatingResolveCallback();

    p.promise().ThenHere(
        FROM_HERE, BindOnce(&MockObject::Task, Unretained(&mock_object),
                            MakeRefCounted<ObjectToDelete>(&delete_task_flag)));
  }

  EXPECT_FALSE(delete_task_flag);
  cb = RepeatingCallback<void(int)>();
  EXPECT_TRUE(delete_task_flag);
}

TEST(PromiseMemoryLeakTest, GetRejectCallbackNeverRun) {
  test::TaskEnvironment task_environment_;
  OnceCallback<void(int)> cb;
  MockObject mock_object;
  bool delete_task_flag = false;

  {
    ManualPromiseResolver<void, int> p(FROM_HERE);
    cb = p.GetRejectCallback();

    p.promise().CatchHere(
        FROM_HERE, BindOnce(&MockObject::Task, Unretained(&mock_object),
                            MakeRefCounted<ObjectToDelete>(&delete_task_flag)));
  }

  EXPECT_FALSE(delete_task_flag);
  cb = OnceCallback<void(int)>();
  EXPECT_TRUE(delete_task_flag);
}

TEST(PromiseMemoryLeakTest, GetRepeatingRejectCallbackNeverRun) {
  test::TaskEnvironment task_environment_;
  RepeatingCallback<void(int)> cb;
  MockObject mock_object;
  bool delete_task_flag = false;

  {
    ManualPromiseResolver<void, int> p(FROM_HERE);
    cb = p.GetRepeatingRejectCallback();

    p.promise().CatchHere(
        FROM_HERE, BindOnce(&MockObject::Task, Unretained(&mock_object),
                            MakeRefCounted<ObjectToDelete>(&delete_task_flag)));
  }

  EXPECT_FALSE(delete_task_flag);
  cb = RepeatingCallback<void(int)>();
  EXPECT_TRUE(delete_task_flag);
}

TEST_F(PromiseTest, GetResolveCallbackThen) {
  ManualPromiseResolver<int> p(FROM_HERE);
  p.GetResolveCallback().Run(123);

  RunLoop run_loop;
  p.promise().ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                         EXPECT_EQ(123, result);
                         run_loop.Quit();
                       }));

  run_loop.Run();
}

TEST_F(PromiseTest, GetResolveCallbackThenWithConstInt) {
  ManualPromiseResolver<int> p(FROM_HERE);
  p.GetResolveCallback().Run(123);

  RunLoop run_loop;
  p.promise().ThenHere(FROM_HERE, BindLambdaForTesting([&](const int result) {
                         EXPECT_EQ(123, result);
                         run_loop.Quit();
                       }));

  run_loop.Run();
}

TEST_F(PromiseTest, GetResolveCallbackMultipleArgs) {
  ManualPromiseResolver<std::tuple<int, bool, float>> p(FROM_HERE);
  static_assert(
      std::is_same<OnceCallback<void(int, bool, float)>,
                   decltype(p.GetResolveCallback<int, bool, float>())>::value,
      "");
  p.GetResolveCallback<int, bool, float>().Run(123, true, 1.5f);

  RunLoop run_loop;
  p.promise().ThenHere(FROM_HERE,
                       BindLambdaForTesting([&](int a, bool b, float c) {
                         EXPECT_EQ(123, a);
                         EXPECT_TRUE(b);
                         EXPECT_EQ(1.5f, c);
                         run_loop.Quit();
                       }));

  run_loop.Run();
}

TEST_F(PromiseTest, ManualPromiseResolverCallbackLifetimeCanOutliveParent) {
  OnceCallback<void(int)> resolve_cb;

  RunLoop run_loop;
  {
    ManualPromiseResolver<int> p(FROM_HERE);
    resolve_cb = p.GetResolveCallback();

    p.promise().ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                           EXPECT_EQ(123, result);
                           run_loop.Quit();
                         }));
  }

  std::move(resolve_cb).Run(123);
  run_loop.Run();
}

TEST_F(PromiseTest, ResolveWithTuple) {
  ManualPromiseResolver<void> p(FROM_HERE);
  p.Resolve();

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE,
                BindOnce([]() { return std::tuple<int, bool>(123, false); }))
      .ThenHere(FROM_HERE,
                BindLambdaForTesting([&](const std::tuple<int, bool>& tuple) {
                  EXPECT_EQ(123, std::get<0>(tuple));
                  EXPECT_FALSE(std::get<1>(tuple));
                  run_loop.Quit();
                }));

  run_loop.Run();
}

TEST_F(PromiseTest, ResolveWithUnpackedTuple) {
  ManualPromiseResolver<void> p(FROM_HERE);
  p.Resolve();

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE,
                BindOnce([]() { return std::tuple<int, bool>(123, false); }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int a, bool b) {
                  EXPECT_EQ(123, a);
                  EXPECT_FALSE(b);
                  run_loop.Quit();
                }));

  run_loop.Run();
}

TEST_F(PromiseTest, ResolveWithUnpackedTupleMoveOnlyTypes) {
  ManualPromiseResolver<void> p(FROM_HERE);
  p.Resolve();

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE, BindOnce([]() {
                  return std::make_tuple(std::make_unique<int>(42),
                                         std::make_unique<float>(4.2f));
                }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](std::unique_ptr<int> a,
                                                    std::unique_ptr<float> b) {
                  EXPECT_EQ(42, *a);
                  EXPECT_EQ(4.2f, *b);
                  run_loop.Quit();
                }));

  run_loop.Run();
}

TEST_F(PromiseTest, GetRejectCallbackCatch) {
  ManualPromiseResolver<int, std::string> p(FROM_HERE);

  RunLoop run_loop;
  p.promise().ThenHere(
      FROM_HERE, BindLambdaForTesting([&](int result) {
        run_loop.Quit();
        FAIL() << "We shouldn't get here, the promise was rejected!";
      }),
      BindLambdaForTesting([&](const std::string& err) {
        run_loop.Quit();
        EXPECT_EQ("Oh no!", err);
      }));

  p.GetRejectCallback().Run(std::string("Oh no!"));
  run_loop.Run();
}

TEST_F(PromiseTest, GetRepeatingResolveCallbackThen) {
  ManualPromiseResolver<int> p(FROM_HERE);
  p.GetRepeatingResolveCallback().Run(123);

  RunLoop run_loop;
  p.promise().ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                         EXPECT_EQ(123, result);
                         run_loop.Quit();
                       }));

  run_loop.Run();
}

TEST_F(PromiseTest, GetRepeatingRejectCallbackCatch) {
  ManualPromiseResolver<int, std::string> p(FROM_HERE);

  RunLoop run_loop;
  p.promise().ThenHere(
      FROM_HERE, BindLambdaForTesting([&](int result) {
        run_loop.Quit();
        FAIL() << "We shouldn't get here, the promise was rejected!";
      }),
      BindLambdaForTesting([&](const std::string& err) {
        run_loop.Quit();
        EXPECT_EQ("Oh no!", err);
      }));

  p.GetRepeatingRejectCallback().Run(std::string("Oh no!"));
  run_loop.Run();
}

TEST_F(PromiseTest, CreateResolvedThen) {
  RunLoop run_loop;
  Promise<int>::CreateResolved(FROM_HERE, 123)
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                  EXPECT_EQ(123, result);
                  run_loop.Quit();
                }));

  run_loop.Run();
}

TEST_F(PromiseTest, ThenRejectWithTuple) {
  ManualPromiseResolver<void> p(FROM_HERE);
  p.Resolve();

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE, BindOnce([]() {
                  return Rejected<std::tuple<int, bool>>{123, false};
                }))
      .CatchHere(FROM_HERE,
                 BindLambdaForTesting([&](const std::tuple<int, bool>& tuple) {
                   EXPECT_EQ(123, std::get<0>(tuple));
                   EXPECT_FALSE(std::get<1>(tuple));
                   run_loop.Quit();
                 }));

  run_loop.Run();
}

TEST_F(PromiseTest, ManualPromiseResolverMultipleArgs) {
  ManualPromiseResolver<int, std::tuple<bool, std::string>> p(FROM_HERE);
  p.GetRejectCallback<bool, std::string>().Run(false, "Noes!");
  std::tuple<bool, std::string> err = p.promise().TakeRejectValueForTesting();
  EXPECT_FALSE(std::get<0>(err));
  EXPECT_EQ("Noes!", std::get<1>(err));
}

TEST_F(PromiseTest, GetRejectCallbackMultipleArgs) {
  ManualPromiseResolver<int, std::tuple<bool, std::string>> p(FROM_HERE);

  RunLoop run_loop;
  p.promise().ThenHere(
      FROM_HERE, BindLambdaForTesting([&](int result) {
        run_loop.Quit();
        FAIL() << "We shouldn't get here, the promise was rejected!";
      }),
      BindLambdaForTesting([&](const std::tuple<bool, std::string>& err) {
        // NB we don't currently support tuple expansion for reject.
        // Its not hard to add, but it's unclear if it will ever be used.
        run_loop.Quit();
        EXPECT_FALSE(std::get<0>(err));
        EXPECT_EQ("Noes!", std::get<1>(err));
      }));

  p.GetRejectCallback<bool, std::string>().Run(false, "Noes!");
  run_loop.Run();
}

TEST_F(PromiseTest, CatchHereReturnTypes) {
  ManualPromiseResolver<int, void> p1(FROM_HERE);

  // Check CatchHere returns the expected return types for various
  // return types.
  Promise<int> r1 =
      p1.promise().CatchHere(FROM_HERE, BindOnce([]() { return 123; }));
  Promise<int> r2 = p1.promise().CatchHere(
      FROM_HERE, BindOnce([]() { return Resolved<int>(123); }));
  Promise<int, int> r3 = p1.promise().CatchHere(
      FROM_HERE, BindOnce([]() { return Rejected<int>(123); }));

  Promise<int, void> r4 = p1.promise().CatchHere(
      FROM_HERE, BindOnce([]() { return PromiseResult<int, void>(123.0); }));
  Promise<int> r5 = p1.promise().CatchHere(
      FROM_HERE,
      BindOnce([]() { return PromiseResult<int, NoReject>(123.0); }));
  Promise<int, int> r6 = p1.promise().CatchHere(
      FROM_HERE,
      BindOnce([]() { return PromiseResult<NoResolve, int>(123.0); }));

  Promise<int, void> r7 = p1.promise().CatchHere(
      FROM_HERE, BindOnce([]() { return Promise<int, void>(); }));
  Promise<int> r8 = p1.promise().CatchHere(
      FROM_HERE, BindOnce([]() { return Promise<int, NoReject>(); }));
  Promise<int, int> r9 = p1.promise().CatchHere(
      FROM_HERE, BindOnce([]() { return Promise<NoResolve, int>(); }));

  ManualPromiseResolver<NoResolve, void> p2(FROM_HERE);
  Promise<int> r10 =
      p2.promise().CatchHere(FROM_HERE, BindOnce([]() { return 123; }));
  Promise<int> r11 = p2.promise().CatchHere(
      FROM_HERE, BindOnce([]() { return Resolved<int>(123); }));
  Promise<NoResolve, int> r12 = p2.promise().CatchHere(
      FROM_HERE, BindOnce([]() { return Rejected<int>(123); }));
}

TEST_F(PromiseTest, ThenHereReturnTypes) {
  ManualPromiseResolver<std::string, void> p1(FROM_HERE);

  // Check ThenHere returns the expected return types for various
  // return types.
  Promise<int, void> r1 =
      p1.promise().ThenHere(FROM_HERE, BindOnce([]() { return 123; }));
  Promise<int, void> r2 = p1.promise().ThenHere(
      FROM_HERE, BindOnce([]() { return Resolved<int>(123); }));
  Promise<NoResolve, void> r3 = p1.promise().ThenHere(
      FROM_HERE, BindOnce([]() { return Rejected<void>(); }));

  Promise<int, void> r4 = p1.promise().ThenHere(
      FROM_HERE, BindOnce([]() { return PromiseResult<int, void>(123.0); }));
  Promise<int, void> r5 = p1.promise().ThenHere(
      FROM_HERE,
      BindOnce([]() { return PromiseResult<int, NoReject>(123.0); }));
  Promise<NoResolve, void> r6 = p1.promise().ThenHere(
      FROM_HERE, BindOnce([]() { return PromiseResult<NoResolve, void>(); }));

  Promise<int, void> r7 = p1.promise().ThenHere(
      FROM_HERE, BindOnce([]() { return Promise<int, void>(); }));
  Promise<int, void> r8 = p1.promise().ThenHere(
      FROM_HERE, BindOnce([]() { return Promise<int, NoReject>(); }));
  Promise<NoResolve, void> r9 = p1.promise().ThenHere(
      FROM_HERE, BindOnce([]() { return Promise<NoResolve, void>(); }));

  ManualPromiseResolver<std::string> p2(FROM_HERE);
  Promise<int> r10 =
      p2.promise().ThenHere(FROM_HERE, BindOnce([]() { return 123; }));
  Promise<int> r11 = p2.promise().ThenHere(
      FROM_HERE, BindOnce([]() { return Resolved<int>(123); }));
  Promise<NoResolve, int> r12 = p2.promise().ThenHere(
      FROM_HERE, BindOnce([]() { return Rejected<int>(123); }));
}

TEST_F(PromiseTest, ThenAndCatchHereReturnTypes) {
  struct A {};
  struct B {};
  struct C {};
  struct D {};

  Promise<B, NoReject> p1 =
      ManualPromiseResolver<A, NoReject>(FROM_HERE).promise().ThenHere(
          FROM_HERE, BindOnce([]() { return Resolved<B>(); }));
  Promise<NoResolve, B> p2 =
      ManualPromiseResolver<A, NoReject>(FROM_HERE).promise().ThenHere(
          FROM_HERE, BindOnce([]() { return Rejected<B>(); }));
  Promise<B, C> p3 =
      ManualPromiseResolver<A, NoReject>(FROM_HERE).promise().ThenHere(
          FROM_HERE, BindOnce([]() -> PromiseResult<B, C> { return B{}; }));

  Promise<B, NoReject> p4 =
      ManualPromiseResolver<NoResolve, A>(FROM_HERE).promise().CatchHere(
          FROM_HERE, BindOnce([]() { return Resolved<B>(); }));
  Promise<NoResolve, B> p5 =
      ManualPromiseResolver<NoResolve, A>(FROM_HERE).promise().CatchHere(
          FROM_HERE, BindOnce([]() { return Rejected<B>(); }));
  Promise<B, C> p6 =
      ManualPromiseResolver<NoResolve, A>(FROM_HERE).promise().CatchHere(
          FROM_HERE, BindOnce([]() -> PromiseResult<B, C> { return B{}; }));

  Promise<B, C> p7 = ManualPromiseResolver<A, C>(FROM_HERE).promise().ThenHere(
      FROM_HERE, BindOnce([]() { return Resolved<B>(); }));
  Promise<NoResolve, C> p8 =
      ManualPromiseResolver<A, C>(FROM_HERE).promise().ThenHere(
          FROM_HERE, BindOnce([]() { return Rejected<C>(); }));
  Promise<B, C> p9 = ManualPromiseResolver<A, C>(FROM_HERE).promise().ThenHere(
      FROM_HERE, BindOnce([]() -> PromiseResult<B, C> { return B{}; }));

  Promise<A, NoReject> p10 =
      ManualPromiseResolver<A, C>(FROM_HERE).promise().CatchHere(
          FROM_HERE, BindOnce([]() { return Resolved<A>(); }));
  Promise<A, B> p11 =
      ManualPromiseResolver<A, C>(FROM_HERE).promise().CatchHere(
          FROM_HERE, BindOnce([]() { return Rejected<B>(); }));
  Promise<A, B> p12 =
      ManualPromiseResolver<A, C>(FROM_HERE).promise().CatchHere(
          FROM_HERE, BindOnce([]() -> PromiseResult<A, B> { return B{}; }));

  Promise<C, NoReject> p13 =
      ManualPromiseResolver<A, B>(FROM_HERE).promise().ThenHere(
          FROM_HERE, BindOnce([]() { return Resolved<C>(); }),
          BindOnce([]() { return Resolved<C>(); }));
  Promise<NoResolve, D> p14 =
      ManualPromiseResolver<A, B>(FROM_HERE).promise().ThenHere(
          FROM_HERE, BindOnce([]() { return Rejected<D>(); }),
          BindOnce([]() { return Rejected<D>(); }));
  Promise<C, D> p15 = ManualPromiseResolver<A, B>(FROM_HERE).promise().ThenHere(
      FROM_HERE, BindOnce([]() { return Resolved<C>(); }),
      BindOnce([]() { return Rejected<D>(); }));
  Promise<C, D> p16 = ManualPromiseResolver<A, B>(FROM_HERE).promise().ThenHere(
      FROM_HERE, BindOnce([]() { return Rejected<D>(); }),
      BindOnce([]() { return Resolved<C>(); }));

  Promise<C, D> p17 = ManualPromiseResolver<A, B>(FROM_HERE).promise().ThenHere(
      FROM_HERE, BindOnce([]() -> PromiseResult<C, D> { return C{}; }),
      BindOnce([]() { return Resolved<C>(); }));
  Promise<C, D> p18 = ManualPromiseResolver<A, B>(FROM_HERE).promise().ThenHere(
      FROM_HERE, BindOnce([]() -> PromiseResult<C, D> { return C{}; }),
      BindOnce([]() { return Rejected<D>(); }));
  Promise<C, D> p19 = ManualPromiseResolver<A, B>(FROM_HERE).promise().ThenHere(
      FROM_HERE, BindOnce([]() { return Resolved<C>(); }),
      BindOnce([]() -> PromiseResult<C, D> { return C{}; }));
  Promise<C, D> p20 = ManualPromiseResolver<A, B>(FROM_HERE).promise().ThenHere(
      FROM_HERE, BindOnce([]() { return Rejected<D>(); }),
      BindOnce([]() -> PromiseResult<C, D> { return C{}; }));

  Promise<C, D> p21 = ManualPromiseResolver<A, B>(FROM_HERE).promise().ThenHere(
      FROM_HERE, BindOnce([]() -> PromiseResult<C, D> { return C{}; }),
      BindOnce([]() -> PromiseResult<C, D> { return C{}; }));
}

TEST_F(PromiseTest, UnsettledManualPromiseResolverCancelsChain) {
  bool delete_flag = false;
  Promise<void> p2;

  {
    ManualPromiseResolver<int> p1(FROM_HERE);
    p2 = p1.promise().ThenHere(
        FROM_HERE, BindOnce([](scoped_refptr<ObjectToDelete> v) {},
                            MakeRefCounted<ObjectToDelete>(&delete_flag)));
  }

  EXPECT_TRUE(delete_flag);
  EXPECT_TRUE(p2.IsCancelledForTesting());
}

TEST_F(PromiseTest, CancellationSpottedByExecute) {
  bool delete_flag = false;
  Promise<void> p3;

  {
    Cancelable cancelable;
    ManualPromiseResolver<void> p1(FROM_HERE);
    Promise<void> p2 = p1.promise().ThenHere(
        FROM_HERE, BindOnce(&Cancelable::NopTask,
                            cancelable.weak_ptr_factory.GetWeakPtr()));

    p1.Resolve();
    cancelable.weak_ptr_factory.InvalidateWeakPtrs();

    p3 = p2.ThenHere(FROM_HERE,
                     BindOnce([](scoped_refptr<ObjectToDelete> v) {},
                              MakeRefCounted<ObjectToDelete>(&delete_flag)));
  }

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(delete_flag);
  EXPECT_TRUE(p3.IsCancelledForTesting());
}

TEST_F(PromiseTest, RejectAndReReject) {
  ManualPromiseResolver<int, std::string> p(FROM_HERE);
  RunLoop run_loop;

  p.promise()
      .CatchHere(
          FROM_HERE,
          BindOnce([](const std::string& err) -> PromiseResult<int, int> {
            EXPECT_EQ("Oh no!", err);
            // Re-Reject with -1 this time.
            return Rejected<int>(-1);
          }))
      .CatchHere(FROM_HERE, BindLambdaForTesting([&](int err) {
                   EXPECT_EQ(-1, err);
                   run_loop.Quit();
                   return -1;
                 }));

  p.GetRejectCallback().Run("Oh no!");
  run_loop.Run();
}

TEST_F(PromiseTest, RejectAndReRejectThenCatch) {
  ManualPromiseResolver<int, std::string> p(FROM_HERE);
  RunLoop run_loop;

  p.promise()
      .CatchHere(FROM_HERE, BindLambdaForTesting(
                                [](std::string) { return Rejected<int>(-1); }))
      .CatchHere(FROM_HERE,
                 BindLambdaForTesting([&](int) { return Resolved<int>(1000); }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int value) {
                  EXPECT_EQ(1000, value);
                  return Rejected<DummyError>();
                }))
      .CatchHere(FROM_HERE,
                 BindLambdaForTesting([&](DummyError) { run_loop.Quit(); }));

  p.GetRejectCallback().Run("Oh no!");
  run_loop.Run();
}

TEST_F(PromiseTest, ThenWhichAlwayResolves) {
  ManualPromiseResolver<void> p(FROM_HERE);
  RunLoop run_loop;

  p.promise()
      .ThenHere(FROM_HERE, BindOnce([]() -> Resolved<int> {
                  // Resolve
                  return 123;
                }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int value) {
                  EXPECT_EQ(123, value);
                  run_loop.Quit();
                }));

  p.GetResolveCallback().Run();
  run_loop.Run();
}

TEST_F(PromiseTest, ThenWhichAlwayRejects) {
  ManualPromiseResolver<void, int> p(FROM_HERE);
  RunLoop run_loop;

  p.promise()
      .ThenHere(FROM_HERE, BindOnce([]() -> Rejected<int> {
                  // Reject
                  return -1;
                }))
      .CatchHere(FROM_HERE, BindLambdaForTesting([&](int err) {
                   EXPECT_EQ(-1, err);
                   run_loop.Quit();
                 }));

  p.GetResolveCallback().Run();
  run_loop.Run();
}

TEST_F(PromiseTest, ThenWhichAlwayRejectsTypeTwo) {
  ManualPromiseResolver<void> p(FROM_HERE);
  RunLoop run_loop;

  p.promise()
      .ThenHere(FROM_HERE, BindOnce([]() -> Rejected<int> {
                  // Reject
                  return -1;
                }))
      .CatchHere(FROM_HERE, BindLambdaForTesting([&](int err) {
                   EXPECT_EQ(-1, err);
                   run_loop.Quit();
                 }));

  p.GetResolveCallback().Run();
  run_loop.Run();
}

TEST_F(PromiseTest, ThenWhichAlwayRejectsTypeThree) {
  ManualPromiseResolver<int> p(FROM_HERE);

  base::RunLoop run_loop;

  p.promise()
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                  return Rejected<std::string>(std::string("reject"));
                }))
      .CatchHere(FROM_HERE, BindLambdaForTesting(
                                [&](std::string result) { run_loop.Quit(); }));

  p.GetResolveCallback().Run(123);

  run_loop.Run();
}

TEST_F(PromiseTest, ReferenceType) {
  int a = 123;
  int b = 456;
  ManualPromiseResolver<const int&> p(FROM_HERE);
  RunLoop run_loop;

  p.promise()
      .ThenHere(FROM_HERE,
                BindLambdaForTesting([&](const int& value) -> const int& {
                  EXPECT_EQ(123, value);
                  return b;
                }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](const int& value) {
                  EXPECT_EQ(456, value);
                  run_loop.Quit();
                }));

  p.GetResolveCallback().Run(a);
  run_loop.Run();
}

TEST_F(PromiseTest, PromiseResultVoid) {
  ManualPromiseResolver<void> p(FROM_HERE);
  RunLoop run_loop;

  p.promise()
      .ThenHere(FROM_HERE,
                BindLambdaForTesting([&]() { return PromiseResult<void>(); }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&]() { run_loop.Quit(); }));

  p.Resolve();
  run_loop.Run();
}

TEST_F(PromiseTest, RefcountedType) {
  scoped_refptr<internal::AbstractPromise> a =
      DoNothingPromiseBuilder(FROM_HERE);
  scoped_refptr<internal::AbstractPromise> b =
      DoNothingPromiseBuilder(FROM_HERE);
  ManualPromiseResolver<scoped_refptr<internal::AbstractPromise>> p(FROM_HERE);
  RunLoop run_loop;

  p.promise()
      .ThenHere(FROM_HERE,
                BindLambdaForTesting(
                    [&](scoped_refptr<internal::AbstractPromise> value) {
                      EXPECT_EQ(a, value);
                      return b;
                    }))
      .ThenHere(FROM_HERE,
                BindLambdaForTesting(
                    [&](scoped_refptr<internal::AbstractPromise> value) {
                      EXPECT_EQ(b, value);
                      run_loop.Quit();
                    }));

  p.Resolve(a);
  run_loop.Run();
}

TEST_F(PromiseTest, ResolveThenVoidFunction) {
  ManualPromiseResolver<int> p(FROM_HERE);
  p.Resolve(123);

  // You don't have to use the resolve (or reject) arguments from the
  // previous promise.
  RunLoop run_loop;
  p.promise().ThenHere(FROM_HERE,
                       BindLambdaForTesting([&]() { run_loop.Quit(); }));

  run_loop.Run();
}

TEST_F(PromiseTest, ResolveThenStdTupleUnpack) {
  RunLoop run_loop;
  Promise<std::tuple<int, std::string>>::CreateResolved(FROM_HERE, 10,
                                                        std::string("Hi"))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int a, std::string b) {
                  EXPECT_EQ(10, a);
                  EXPECT_EQ("Hi", b);
                  run_loop.Quit();
                }));
  run_loop.Run();
}

TEST_F(PromiseTest, ResolveAfterThen) {
  ManualPromiseResolver<int> p(FROM_HERE);

  RunLoop run_loop;
  p.promise().ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                         EXPECT_EQ(123, result);
                         run_loop.Quit();
                       }));

  p.Resolve(123);
  run_loop.Run();
}

TEST_F(PromiseTest, RejectOutsidePromiseAfterThen) {
  ManualPromiseResolver<int, void> p(FROM_HERE);

  RunLoop run_loop;
  p.promise().ThenHere(
      FROM_HERE, BindLambdaForTesting([&](int result) {
        run_loop.Quit();
        FAIL() << "We shouldn't get here, the promise was rejected!";
      }),
      run_loop.QuitClosure());

  p.Reject();
  run_loop.Run();
}

TEST_F(PromiseTest, ThenChainMoveOnlyType) {
  ManualPromiseResolver<std::unique_ptr<int>> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE,
                BindOnce([](std::unique_ptr<int> result) { return result; }))
      .ThenHere(FROM_HERE,
                BindOnce([](std::unique_ptr<int> result) { return result; }))
      .ThenHere(FROM_HERE,
                BindLambdaForTesting([&](std::unique_ptr<int> result) {
                  EXPECT_THAT(123, *result);
                  run_loop.Quit();
                }));

  p.Resolve(std::make_unique<int>(123));
  run_loop.Run();
}

TEST_F(PromiseTest, MultipleMovesNotAllowed) {
  ManualPromiseResolver<std::unique_ptr<int>> p(FROM_HERE);

  // The executor argument will be called with move semantics.
  p.promise().ThenHere(FROM_HERE, BindOnce([](std::unique_ptr<int> result) {}));

  // It's an error to do that twice.
  EXPECT_DCHECK_DEATH({
    p.promise().ThenHere(FROM_HERE,
                         BindOnce([](std::unique_ptr<int> result) {}));
  });
}

TEST_F(PromiseTest, ThenChain) {
  ManualPromiseResolver<std::vector<size_t>> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE, BindOnce([](std::vector<size_t> result) {
                  result.push_back(1);
                  return result;
                }))
      .ThenHere(FROM_HERE, BindOnce([](std::vector<size_t> result) {
                  result.push_back(2);
                  return result;
                }))
      .ThenHere(FROM_HERE, BindOnce([](std::vector<size_t> result) {
                  result.push_back(3);
                  return result;
                }))
      .ThenHere(FROM_HERE,
                BindLambdaForTesting([&](std::vector<size_t> result) {
                  EXPECT_THAT(result, ElementsAre(0u, 1u, 2u, 3u));
                  run_loop.Quit();
                }));

  p.Resolve(std::vector<size_t>{0});
  run_loop.Run();
}

TEST_F(PromiseTest, RejectionInThenChainDefaultVoid) {
  ManualPromiseResolver<std::vector<size_t>> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE, BindOnce([](std::vector<size_t> result) {
                  result.push_back(result.size());
                  return result;
                }))
      .ThenHere(FROM_HERE, BindOnce([](std::vector<size_t> result) {
                  result.push_back(result.size());
                  return result;
                }))
      .ThenHere(FROM_HERE,
                BindOnce([](std::vector<size_t> result)
                             -> PromiseResult<std::vector<size_t>, void> {
                  return Rejected<void>();
                }))
      .ThenHere(FROM_HERE,
                BindLambdaForTesting([&](std::vector<size_t> result) {
                  FAIL() << "We shouldn't get here, the promise was rejected!";
                }),
                BindLambdaForTesting([&]() { run_loop.Quit(); }));

  p.Resolve(std::vector<size_t>{0});
  run_loop.Run();
}

TEST_F(PromiseTest, RejectPropagation) {
  ManualPromiseResolver<int, bool> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE, BindOnce([](int result) { return result + 1; }))
      .ThenHere(FROM_HERE, BindOnce([](int result) { return result + 1; }))
      .ThenHere(FROM_HERE,
                BindOnce([](int result) -> PromiseResult<int, std::string> {
                  return std::string("Fail shouldn't get here");
                }),
                BindOnce([](bool value) -> PromiseResult<int, std::string> {
                  EXPECT_FALSE(value);
                  return std::string("Oh no!");
                }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                  FAIL() << "We shouldn't get here, the promise was rejected!";
                  run_loop.Quit();
                }),
                BindLambdaForTesting([&](const std::string& err) {
                  EXPECT_EQ("Oh no!", err);
                  run_loop.Quit();
                }));

  p.Reject(false);
  run_loop.Run();
}

TEST_F(PromiseTest, RejectPropagationThensAfterRejectSkipped) {
  ManualPromiseResolver<int, bool> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE, BindOnce([](int result) { return result + 1; }))
      .ThenHere(FROM_HERE, BindOnce([](int result) { return result + 1; }))
      .ThenHere(FROM_HERE,
                BindOnce([](int result) -> PromiseResult<int, std::string> {
                  return std::string("Fail shouldn't get here");
                }),
                BindOnce([](bool value) -> PromiseResult<int, std::string> {
                  EXPECT_FALSE(value);
                  return std::string("Oh no!");  // Reject
                }))
      .ThenHere(FROM_HERE, BindOnce([](int result) {
                  CHECK(false) << "Shouldn't get here";
                  return result + 1;
                }))
      .ThenHere(FROM_HERE, BindOnce([](int result) {
                  CHECK(false) << "Shouldn't get here";
                  return result + 1;
                }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                  FAIL() << "We shouldn't get here, the promise was rejected!";
                  run_loop.Quit();
                }),
                BindLambdaForTesting([&](const std::string& err) {
                  EXPECT_EQ("Oh no!", err);
                  run_loop.Quit();
                }));

  p.Reject(false);
  run_loop.Run();
}

TEST_F(PromiseTest, ThenOnWithHetrogenousButCompatibleReturnTypes) {
  ManualPromiseResolver<void, int> p(FROM_HERE);

  // Make sure ThenHere returns the expected type.
  Promise<int, std::string> p2 = p.promise().ThenHere(
      FROM_HERE,
      BindOnce([]() -> PromiseResult<int, std::string> { return 123; }),
      BindOnce([](int err) -> Resolved<int> { return 123; }));
}

TEST_F(PromiseTest, ThenOnWithHetrogenousButCompatibleReturnTypes2) {
  ManualPromiseResolver<void, int> p(FROM_HERE);

  // Make sure ThenHere returns the expected type.
  Promise<int, std::string> p2 = p.promise().ThenHere(
      FROM_HERE,
      BindOnce([]() -> PromiseResult<int, std::string> { return 123; }),
      BindOnce([](int err) -> Rejected<std::string> { return "123"; }));
}

TEST_F(PromiseTest, ThenOnWithHetrogenousButCompatibleReturnTypes3) {
  ManualPromiseResolver<int, std::string> p(FROM_HERE);

  // Make sure ThenHere returns the expected type.
  Promise<void, bool> p2 = p.promise().ThenHere(
      FROM_HERE, BindOnce([](int value) -> PromiseResult<void, bool> {
        if (value % 2) {
          return Resolved<void>();
        } else {
          return true;
        }
      }),
      BindOnce([](const std::string& err) -> Rejected<bool> { return false; }));
}

TEST_F(PromiseTest, ThenOnAfterNoResolvePromiseResult) {
  ManualPromiseResolver<std::unique_ptr<int>, int> p1(FROM_HERE);

  RunLoop run_loop;
  p1.promise()
      .CatchHere(FROM_HERE, BindLambdaForTesting(
                                [&](int) -> PromiseResult<NoResolve, int> {
                                  return Rejected<int>();
                                }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](std::unique_ptr<int>) {
                  run_loop.Quit();
                  return std::make_unique<int>(42);
                }),
                BindLambdaForTesting([&](int err) {
                  CHECK(false) << "Shouldn't get here";
                  return std::make_unique<int>(42);
                }));

  p1.GetResolveCallback().Run(std::make_unique<int>(42));

  run_loop.Run();
}

TEST_F(PromiseTest, CatchCreatesNoRejectPromise) {
  ManualPromiseResolver<int> p(FROM_HERE);

  // Make sure CatchHere returns the expected type.
  Promise<int> p2 =
      p.promise()
          .ThenHere(FROM_HERE, BindLambdaForTesting([&](int) {
                      return Rejected<std::string>();
                    }))
          .CatchHere(FROM_HERE, BindLambdaForTesting([&](std::string) {
                       return Resolved<int>();
                     }));
}

TEST_F(PromiseTest, ResolveSkipsCatches) {
  ManualPromiseResolver<int, void> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE, BindOnce([](int result) { return result + 1; }))
      .CatchHere(FROM_HERE, BindOnce([]() -> PromiseResult<int, void> {
                   CHECK(false) << "Shouldn't get here";
                   return -1;
                 }))
      .CatchHere(FROM_HERE, BindOnce([]() -> PromiseResult<int, void> {
                   CHECK(false) << "Shouldn't get here";
                   return -1;
                 }))
      .CatchHere(FROM_HERE, BindOnce([]() -> PromiseResult<int, void> {
                   CHECK(false) << "Shouldn't get here";
                   return -1;
                 }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                  EXPECT_EQ(2, result);
                  run_loop.Quit();
                }),
                BindLambdaForTesting([&]() {
                  FAIL() << "We shouldn't get here, the promise was resolved!";
                  run_loop.Quit();
                }));

  p.Resolve(1);
  run_loop.Run();
}

TEST_F(PromiseTest, ThenChainVariousReturnTypes) {
  ManualPromiseResolver<void> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE, BindOnce([]() { return 5; }))
      .ThenHere(FROM_HERE, BindOnce([](int result) {
                  EXPECT_EQ(5, result);
                  return std::string("Hello");
                }))
      .ThenHere(FROM_HERE, BindOnce([](std::string result) {
                  EXPECT_EQ("Hello", result);
                  return true;
                }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](bool result) {
                  EXPECT_TRUE(result);
                  run_loop.Quit();
                }));

  p.GetResolveCallback().Run();
  run_loop.Run();
}

TEST_F(PromiseTest, CurriedVoidPromise) {
  Promise<void> p = Promise<void>::CreateResolved(FROM_HERE);
  ManualPromiseResolver<void> promise_resolver(FROM_HERE);

  RunLoop run_loop;
  p.ThenHere(FROM_HERE, BindOnce(
                            [](ManualPromiseResolver<void>* promise_resolver) {
                              return promise_resolver->promise();
                            },
                            &promise_resolver))
      .ThenHere(FROM_HERE, run_loop.QuitClosure());
  RunLoop().RunUntilIdle();

  promise_resolver.Resolve();
  run_loop.Run();
}

TEST_F(PromiseTest, CurriedIntPromise) {
  Promise<int> p = Promise<int>::CreateResolved(FROM_HERE, 1000);
  ManualPromiseResolver<int> promise_resolver(FROM_HERE);

  RunLoop run_loop;
  p.ThenHere(FROM_HERE,
             BindOnce(
                 [](ManualPromiseResolver<int>* promise_resolver, int result) {
                   EXPECT_EQ(1000, result);
                   return promise_resolver->promise();
                 },
                 &promise_resolver))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                  EXPECT_EQ(123, result);
                  run_loop.Quit();
                }));
  RunLoop().RunUntilIdle();

  promise_resolver.Resolve(123);
  run_loop.Run();
}

TEST_F(PromiseTest, CurriedIntPromiseChain) {
  Promise<int> p = Promise<int>::CreateResolved(FROM_HERE, 1000);

  ManualPromiseResolver<int> promise_resolver_1(FROM_HERE);
  ManualPromiseResolver<int> promise_resolver_2(FROM_HERE);
  promise_resolver_2.Resolve(promise_resolver_1.promise());
  promise_resolver_1.Resolve(123);

  RunLoop run_loop;
  p.ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
               EXPECT_EQ(1000, result);
               return promise_resolver_2.promise();
             }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                  EXPECT_EQ(123, result);
                  run_loop.Quit();
                }));

  run_loop.Run();
}

TEST_F(PromiseTest, CurriedIntPromiseChain2) {
  Promise<int> p1 = Promise<int>::CreateResolved(FROM_HERE, 1000);
  Promise<int> p2 = Promise<int>::CreateResolved(FROM_HERE, 789);
  Promise<int> then2;

  {
    Promise<int> then1 =
        Promise<int>::CreateResolved(FROM_HERE, 789)
            .ThenHere(FROM_HERE, BindLambdaForTesting([&]() { return p2; }));
    then2 = Promise<int>::CreateResolved(FROM_HERE, 789)
                .ThenHere(
                    FROM_HERE,
                    BindOnce([&](Promise<int> then1) { return then1; }, then1));
  }

  RunLoop run_loop;
  p1.ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                EXPECT_EQ(1000, result);
                return then2;
              }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                  EXPECT_EQ(789, result);
                  run_loop.Quit();
                }));

  run_loop.Run();
}

TEST_F(PromiseTest, CurriedIntPromiseChainThenAddedAfterInitialResolve) {
  ManualPromiseResolver<int> promise_resolver_1(FROM_HERE);
  ManualPromiseResolver<int> promise_resolver_2(FROM_HERE);
  ManualPromiseResolver<int> promise_resolver_3(FROM_HERE);
  promise_resolver_2.Resolve(promise_resolver_1.promise());
  promise_resolver_3.Resolve(promise_resolver_2.promise());

  RunLoop run_loop;
  promise_resolver_3.promise().ThenHere(FROM_HERE,
                                        BindLambdaForTesting([&](int result) {
                                          EXPECT_EQ(123, result);
                                          run_loop.Quit();
                                        }));

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindLambdaForTesting([&]() { promise_resolver_1.Resolve(123); }));

  run_loop.Run();
}

TEST_F(PromiseTest, CurriedVoidPromiseModified) {
  for (size_t i = 0; i < 1000; ++i) {
    Promise<void> p = Promise<void>::CreateResolved(FROM_HERE);
    std::unique_ptr<ManualPromiseResolver<int>> promise_resolver =
        std::make_unique<ManualPromiseResolver<int>>(FROM_HERE);
    RunLoop run_loop;
    p.ThenHere(FROM_HERE, BindOnce([](Promise<int> promise) { return promise; },
                                   promise_resolver->promise()))
        .ThenHere(FROM_HERE, base::BindOnce([](int v) { EXPECT_EQ(v, 42); }))
        .ThenHere(FROM_HERE, run_loop.QuitClosure());
    PostTask(FROM_HERE, {ThreadPool()}, BindLambdaForTesting([&]() {
               promise_resolver->Resolve(42);
               promise_resolver.reset();
             }));
    run_loop.Run();
    task_environment_.RunUntilIdle();
  }
}

TEST_F(PromiseTest, PromiseResultReturningAPromise) {
  Promise<int> p = Promise<int>::CreateResolved(FROM_HERE, 1000);
  ManualPromiseResolver<int> promise_resolver(FROM_HERE);

  RunLoop run_loop;
  p.ThenHere(FROM_HERE,
             BindLambdaForTesting([&](int result) -> PromiseResult<int> {
               EXPECT_EQ(1000, result);
               return promise_resolver.promise();
             }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                  EXPECT_EQ(123, result);
                  run_loop.Quit();
                }));
  RunLoop().RunUntilIdle();

  promise_resolver.Resolve(123);
  run_loop.Run();
}

TEST_F(PromiseTest, ResolveToDisambiguateThenReturnValue) {
  ManualPromiseResolver<int> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE, BindOnce([](int i) -> PromiseResult<Value, Value> {
                  if ((i % 2) == 1)
                    return Resolved<Value>("Success it was odd.");
                  return Rejected<Value>("Failure it was even.");
                }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](Value result) {
                  EXPECT_EQ("Success it was odd.", result.GetString());
                  run_loop.Quit();
                }),
                BindLambdaForTesting([&](Value err) {
                  run_loop.Quit();
                  FAIL() << "We shouldn't get here, the promise was resolved!";
                }));

  p.Resolve(1);
  run_loop.Run();
}

TEST_F(PromiseTest, RejectedToDisambiguateThenReturnValue) {
  ManualPromiseResolver<int, int> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE, BindOnce([]() -> PromiseResult<int, int> {
                  return Rejected<int>(123);
                }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                  run_loop.Quit();
                  FAIL() << "We shouldn't get here, the promise was rejected!";
                }),
                BindLambdaForTesting([&](int err) {
                  run_loop.Quit();
                  EXPECT_EQ(123, err);
                }));

  p.Resolve();
  run_loop.Run();
}

TEST_F(PromiseTest, NestedPromises) {
  ManualPromiseResolver<int> p(FROM_HERE);
  p.Resolve(100);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE, BindOnce([](int result) {
                  ManualPromiseResolver<int> p2(FROM_HERE);
                  p2.Resolve(200);
                  return p2.promise().ThenHere(
                      FROM_HERE, BindOnce([](int result) {
                        ManualPromiseResolver<int> p3(FROM_HERE);
                        p3.Resolve(300);
                        return p3.promise().ThenHere(
                            FROM_HERE,
                            BindOnce([](int result) { return result; }));
                      }));
                }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                  EXPECT_EQ(300, result);
                  run_loop.Quit();
                }));

  run_loop.Run();
}

TEST_F(PromiseTest, Catch) {
  ManualPromiseResolver<int, std::string> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE, BindOnce([](int result) { return result; }))
      .ThenHere(FROM_HERE, BindOnce([](int result) { return result; }))
      .ThenHere(FROM_HERE, BindOnce([](int result) { return result; }))
      .CatchHere(FROM_HERE, BindLambdaForTesting([&](const std::string& err) {
                   EXPECT_EQ("Whoops!", err);
                   run_loop.Quit();
                   return -1;
                 }));

  p.Reject("Whoops!");
  run_loop.Run();
}

TEST_F(PromiseTest, BranchedThenChainExecutionOrder) {
  std::vector<int> run_order;

  ManualPromiseResolver<void> promise_a(FROM_HERE);
  Promise<void> promise_b =
      promise_a.promise()
          .ThenHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 0))
          .ThenHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 1));

  Promise<void> promise_c =
      promise_a.promise()
          .ThenHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 2))
          .ThenHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 3));

  Promise<void> promise_d =
      promise_a.promise()
          .ThenHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 4))
          .ThenHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 5));

  promise_a.Resolve();
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(0, 2, 4, 1, 3, 5));
}

TEST_F(PromiseTest, BranchedThenChainWithCatchExecutionOrder) {
  std::vector<int> run_order;

  ManualPromiseResolver<void, void> promise_a(FROM_HERE);
  Promise<void> promise_b =
      promise_a.promise()
          .ThenHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 0))
          .ThenHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 1))
          .CatchHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 2));

  Promise<void> promise_c =
      promise_a.promise()
          .ThenHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 3))
          .ThenHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 4))
          .CatchHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 5));

  Promise<void> promise_d =
      promise_a.promise()
          .ThenHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 6))
          .ThenHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 7))
          .CatchHere(FROM_HERE, BindOnce(&RecordOrder, &run_order, 8));

  promise_a.Reject();
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2, 5, 8));
}

TEST_F(PromiseTest, CatchRejectInThenChain) {
  ManualPromiseResolver<int> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE,
                BindOnce([](int result) -> PromiseResult<int, std::string> {
                  return std::string("Whoops!");
                }))
      .ThenHere(FROM_HERE, BindOnce([](int result) {
                  CHECK(false) << "Shouldn't get here";
                  return result;
                }))
      .ThenHere(FROM_HERE, BindOnce([](int result) {
                  CHECK(false) << "Shouldn't get here";
                  return result;
                }))
      .CatchHere(FROM_HERE, BindLambdaForTesting([&](const std::string& err) {
                   EXPECT_EQ("Whoops!", err);
                   run_loop.Quit();
                   return -1;
                 }));

  p.Resolve(123);
  run_loop.Run();
}

TEST_F(PromiseTest, CatchThenVoid) {
  ManualPromiseResolver<int, void> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .CatchHere(FROM_HERE, BindOnce([]() { return 123; }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                  EXPECT_EQ(123, result);
                  run_loop.Quit();
                }));

  p.Reject();
  run_loop.Run();
}

TEST_F(PromiseTest, CatchThenInt) {
  ManualPromiseResolver<int, int> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .CatchHere(FROM_HERE, BindOnce([](int err) { return err + 1; }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int result) {
                  EXPECT_EQ(124, result);
                  run_loop.Quit();
                }));

  p.Reject(123);
  run_loop.Run();
}

TEST_F(PromiseTest, SettledTaskFinally) {
  int result = 0;
  ManualPromiseResolver<int> p(FROM_HERE);
  p.Resolve(123);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE,
                BindLambdaForTesting([&](int value) { result = value; }))
      .FinallyHere(FROM_HERE, BindLambdaForTesting([&]() {
                     EXPECT_EQ(123, result);
                     run_loop.Quit();
                   }));

  run_loop.Run();
}

TEST_F(PromiseTest, SettledTaskFinallyThen) {
  int result = 0;
  ManualPromiseResolver<int> p(FROM_HERE);
  p.Resolve(123);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE,
                BindLambdaForTesting([&](int value) { result = value; }))
      .FinallyHere(FROM_HERE, BindLambdaForTesting([&]() {
                     EXPECT_EQ(123, result);
                     return std::string("hi");
                   }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](const std::string& value) {
                  EXPECT_EQ("hi", value);
                  run_loop.Quit();
                }));

  run_loop.Run();
}

TEST_F(PromiseTest, SettledTaskFinallyCatch) {
  int result = 0;
  ManualPromiseResolver<int> p(FROM_HERE);
  p.Resolve(123);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE,
                BindLambdaForTesting([&](int value) { result = value; }))
      .FinallyHere(FROM_HERE, BindLambdaForTesting(
                                  [&]() -> PromiseResult<void, std::string> {
                                    EXPECT_EQ(123, result);
                                    return std::string("Oh no");
                                  }))
      .CatchHere(FROM_HERE, BindLambdaForTesting([&](const std::string& value) {
                   EXPECT_EQ("Oh no", value);
                   run_loop.Quit();
                 }));

  run_loop.Run();
}

TEST_F(PromiseTest, ResolveFinally) {
  int result = 0;
  ManualPromiseResolver<int> p(FROM_HERE);

  RunLoop run_loop;
  p.promise().ThenHere(
      FROM_HERE, BindLambdaForTesting([&](int value) { result = value; }));
  p.promise().FinallyHere(FROM_HERE, BindLambdaForTesting([&]() {
                            EXPECT_EQ(123, result);
                            run_loop.Quit();
                          }));
  p.Resolve(123);
  run_loop.Run();
}

TEST_F(PromiseTest, RejectFinally) {
  int result = 0;
  ManualPromiseResolver<int, void> p(FROM_HERE);

  RunLoop run_loop;
  p.promise().ThenHere(FROM_HERE,
                       BindLambdaForTesting([&](int value) { result = value; }),
                       BindLambdaForTesting([&]() { result = -1; }));
  p.promise().FinallyHere(FROM_HERE, BindLambdaForTesting([&]() {
                            EXPECT_EQ(-1, result);
                            run_loop.Quit();
                          }));
  p.Reject();
  run_loop.Run();
}

TEST_F(PromiseTest, RejectFinallySkipsThens) {
  ManualPromiseResolver<void> p(FROM_HERE);

  RunLoop run_loop;
  p.promise()
      .ThenHere(FROM_HERE,
                BindLambdaForTesting([&]() { return Rejected<int>(123); }))
      .ThenHere(FROM_HERE, BindLambdaForTesting(
                               [&]() { FAIL() << "Promise was rejected"; }))
      .ThenHere(FROM_HERE, BindLambdaForTesting(
                               [&]() { FAIL() << "Promise was rejected"; }))
      .FinallyHere(FROM_HERE, run_loop.QuitClosure());
  p.Resolve();
  run_loop.Run();
}

TEST_F(PromiseTest, CancelViaWeakPtr) {
  std::vector<std::string> log;
  ManualPromiseResolver<void, std::string> mpr(FROM_HERE,
                                               RejectPolicy::kCatchNotRequired);
  Promise<void, std::string> p1 = mpr.promise();
  {
    Cancelable cancelable;
    Promise<void, std::string> p2 = p1.ThenHere(
        FROM_HERE,
        BindOnce(&Cancelable::LogTask, cancelable.weak_ptr_factory.GetWeakPtr(),
                 &log, "Then #1"));
    p2.ThenHere(FROM_HERE,
                BindLambdaForTesting([&]() -> PromiseResult<void, std::string> {
                  log.push_back("Then #2 (reject)");
                  return std::string("Whoops!");
                }))
        .ThenHere(FROM_HERE,
                  BindLambdaForTesting([&]() { log.push_back("Then #3"); }))
        .ThenHere(FROM_HERE,
                  BindLambdaForTesting([&]() { log.push_back("Then #4"); }))
        .CatchHere(FROM_HERE, BindLambdaForTesting([&](const std::string& err) {
                     log.push_back("Caught " + err);
                   }));

    p2.FinallyHere(FROM_HERE,
                   BindLambdaForTesting([&]() { log.push_back("Finally"); }));
    p2.ThenHere(FROM_HERE,
                BindLambdaForTesting([&]() { log.push_back("Then #5"); }));
    p2.ThenHere(FROM_HERE,
                BindLambdaForTesting([&]() { log.push_back("Then #6"); }));
  }

  mpr.Resolve();
  RunLoop().RunUntilIdle();

  EXPECT_TRUE(log.empty());
}

TEST_F(PromiseTest, CancelPropagation) {
  ManualPromiseResolver<void> p1(FROM_HERE);
  ManualPromiseResolver<void> p2(FROM_HERE);
  Promise<void> p3;
  Promise<std::tuple<Void, Void>> pAll;

  RunLoop run_loop;
  {
    Cancelable cancelable;

    p3 = p2.promise().ThenHere(
        FROM_HERE, BindOnce(&Cancelable::NopTask,
                            cancelable.weak_ptr_factory.GetWeakPtr()));

    pAll = Promises::All(FROM_HERE, p1.promise(), p3);

    p1.Resolve();
    p2.Resolve();
    EXPECT_FALSE(pAll.IsCancelledForTesting());
  }

  run_loop.RunUntilIdle();
  EXPECT_TRUE(pAll.IsCancelledForTesting());
}

TEST_F(PromiseTest, CancelPropagationLongerChain) {
  ManualPromiseResolver<void> p1(FROM_HERE);
  ManualPromiseResolver<void> p2(FROM_HERE);
  Promise<void> p3;
  Promise<std::tuple<Void, Void>> pAll;

  RunLoop run_loop;
  {
    Cancelable cancelable;

    p3 = p2.promise()
             .ThenHere(FROM_HERE,
                       BindOnce(&Cancelable::NopTask,
                                cancelable.weak_ptr_factory.GetWeakPtr()))
             .ThenHere(FROM_HERE, BindOnce([]() {}))
             .ThenHere(FROM_HERE, BindOnce([]() {}));

    pAll = Promises::All(FROM_HERE, p1.promise(), p3);

    p1.Resolve();
    p2.Resolve();
    EXPECT_FALSE(pAll.IsCancelledForTesting());
  }

  run_loop.RunUntilIdle();
  EXPECT_TRUE(pAll.IsCancelledForTesting());
}

TEST_F(PromiseTest, CatchNotRequired) {
  ManualPromiseResolver<bool, int> p(FROM_HERE,
                                     RejectPolicy::kCatchNotRequired);

  RunLoop run_loop;
  p.promise().ThenHere(FROM_HERE, run_loop.QuitClosure());

  // Note this doesn't DCHECK even though we haven't specified a Catch.
  p.Resolve();
  run_loop.Run();
}

TEST_F(PromiseTest, MoveOnlyTypeMultipleThensNotAllowed) {
#if DCHECK_IS_ON()
  Promise<std::unique_ptr<int>> p =
      Promise<std::unique_ptr<int>>::CreateResolved(FROM_HERE,
                                                    std::make_unique<int>(123));

  p.ThenHere(FROM_HERE,
             BindOnce([](std::unique_ptr<int> i) { EXPECT_EQ(123, *i); }));

  EXPECT_DCHECK_DEATH({
    p.ThenHere(FROM_HERE,
               BindOnce([](std::unique_ptr<int> i) { EXPECT_EQ(123, *i); }));
  });
#endif
}

TEST_F(PromiseTest, MoveOnlyTypeMultipleCatchesNotAllowed) {
#if DCHECK_IS_ON()
  auto p = Promise<void, std::unique_ptr<int>>::CreateRejected(
      FROM_HERE, std::make_unique<int>(123));

  auto r = p.CatchHere(
      FROM_HERE, BindOnce([](std::unique_ptr<int> i) { EXPECT_EQ(123, *i); }));

  EXPECT_DCHECK_DEATH({
    p.CatchHere(FROM_HERE,
                BindOnce([](std::unique_ptr<int> i) { EXPECT_EQ(123, *i); }));
  });

  // TODO(alexclarke): Temporary, remove when SequenceManager handles promises
  // natively.
  r.GetScopedRefptrForTesting()->OnCanceled();
#endif
}

TEST_F(PromiseTest, UnhandledRejection) {
#if DCHECK_IS_ON()
  Promise<void, int> p = Promise<void, int>::CreateRejected(FROM_HERE).ThenHere(
      FROM_HERE, BindOnce([]() {}));

  RunLoop().RunUntilIdle();

  Promise<void, int> null_promise;
  EXPECT_DCHECK_DEATH({ p = null_promise; });

  // EXPECT_DCHECK_DEATH uses fork under the hood so we still have to tidy up.
  p.IgnoreUncaughtCatchForTesting();
#endif
}

TEST_F(PromiseTest, ManualPromiseResolverPotentialUnhandledRejection) {
#if DCHECK_IS_ON()
  ManualPromiseResolver<void, void> promise_resolver(FROM_HERE);

  // |promise_resolver| could reject but there's no catch.
  Promise<void, void> p =
      promise_resolver.promise().ThenHere(FROM_HERE, BindOnce([]() {}));

  promise_resolver.Resolve();
  RunLoop().RunUntilIdle();

  Promise<void, void> null_promise;
  EXPECT_DCHECK_DEATH({ p = null_promise; });

  // EXPECT_DCHECK_DEATH uses fork under the hood so we still have to tidy up.
  p.IgnoreUncaughtCatchForTesting();
#endif
}

TEST_F(PromiseTest, ManualPromiseResolverResolveCalledTwice) {
#if DCHECK_IS_ON()
  ManualPromiseResolver<void> promise_resolver(FROM_HERE);

  promise_resolver.Resolve();

  EXPECT_DCHECK_DEATH({ promise_resolver.Resolve(); });
#endif
}

TEST_F(PromiseTest, ManualPromiseResolverRejectCalledTwice) {
#if DCHECK_IS_ON()
  ManualPromiseResolver<void, void> promise_resolver(
      FROM_HERE, RejectPolicy::kCatchNotRequired);

  promise_resolver.Reject();

  EXPECT_DCHECK_DEATH({ promise_resolver.Reject(); });
#endif
}

TEST_F(PromiseTest, ManualPromiseResolverResolveCalledAfterReject) {
#if DCHECK_IS_ON()
  ManualPromiseResolver<void, void> promise_resolver(
      FROM_HERE, RejectPolicy::kCatchNotRequired);

  promise_resolver.Reject();

  EXPECT_DCHECK_DEATH({ promise_resolver.Resolve(); });
#endif
}

TEST_F(PromiseTest, ManualPromiseResolverRepeatingResolveCallbackCalledTwice) {
#if DCHECK_IS_ON()
  ManualPromiseResolver<void, void> promise_resolver(
      FROM_HERE, RejectPolicy::kCatchNotRequired);
  RepeatingCallback<void()> resolve =
      promise_resolver.GetRepeatingResolveCallback();

  resolve.Run();

  EXPECT_DCHECK_DEATH({ resolve.Run(); });
#endif
}

TEST_F(PromiseTest, ManualPromiseResolverRepeatingRejectCallbackCalledTwice) {
#if DCHECK_IS_ON()
  ManualPromiseResolver<void, void> promise_resolver(
      FROM_HERE, RejectPolicy::kCatchNotRequired);
  RepeatingCallback<void()> resolve =
      promise_resolver.GetRepeatingRejectCallback();

  resolve.Run();

  EXPECT_DCHECK_DEATH({ resolve.Run(); });
#endif
}

class MultiThreadedPromiseTest : public PromiseTest {
 public:
  void SetUp() override {
    thread_a_.reset(new Thread("MultiThreadPromiseTest_Thread_A"));
    thread_b_.reset(new Thread("MultiThreadPromiseTest_Thread_B"));
    thread_c_.reset(new Thread("MultiThreadPromiseTest_Thread_C"));
    thread_a_->Start();
    thread_b_->Start();
    thread_c_->Start();
  }

  void TearDown() override {
    thread_a_->Stop();
    thread_b_->Stop();
    thread_c_->Stop();
  }

  std::unique_ptr<Thread> thread_a_;
  std::unique_ptr<Thread> thread_b_;
  std::unique_ptr<Thread> thread_c_;
};

TEST_F(MultiThreadedPromiseTest, SimpleThreadHopping) {
  ManualPromiseResolver<void> promise_resolver(FROM_HERE);

  RunLoop run_loop;
  promise_resolver.promise()
      .ThenOn(
          thread_a_->task_runner(), FROM_HERE, BindLambdaForTesting([&]() {
            EXPECT_TRUE(thread_a_->task_runner()->RunsTasksInCurrentSequence());
          }))
      .ThenOn(
          thread_b_->task_runner(), FROM_HERE, BindLambdaForTesting([&]() {
            EXPECT_TRUE(thread_b_->task_runner()->RunsTasksInCurrentSequence());
          }))
      .ThenOn(
          thread_c_->task_runner(), FROM_HERE, BindLambdaForTesting([&]() {
            EXPECT_TRUE(thread_c_->task_runner()->RunsTasksInCurrentSequence());
          }))
      .ThenHere(FROM_HERE, BindLambdaForTesting([&]() {
                  EXPECT_FALSE(
                      thread_a_->task_runner()->RunsTasksInCurrentSequence());
                  EXPECT_FALSE(
                      thread_b_->task_runner()->RunsTasksInCurrentSequence());
                  EXPECT_FALSE(
                      thread_c_->task_runner()->RunsTasksInCurrentSequence());
                  run_loop.Quit();
                }));

  promise_resolver.Resolve();
  run_loop.Run();
}

TEST_F(MultiThreadedPromiseTest, CrossThreadThens) {
  ManualPromiseResolver<void> promise_resolver(FROM_HERE);

  auto resolve_task =
      BindLambdaForTesting([&]() { promise_resolver.Resolve(); });

  RunLoop run_loop;

  // Rolling our own thread-unsafe BarrierClosure to ensure atomics aren't
  // necessary for this test to resolve all Thens on |thread_c_|.
  int thens_remaining = 1000;
  auto then_task = BindLambdaForTesting([&]() {
    --thens_remaining;
    if (!thens_remaining)
      run_loop.Quit();
  });

  thread_a_->task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        // Post 500 thens.
        for (int i = 0; i < 500; i++) {
          promise_resolver.promise().ThenOn(thread_c_->task_runner(), FROM_HERE,
                                            then_task);
        }
        // Post a task onto the main thread to resolve |promise_resolver|.
        // This should run at an undefined time yet all the thens should run.
        thread_b_->task_runner()->PostTask(FROM_HERE, resolve_task);

        // Post another 500 thens.
        for (int i = 0; i < 500; i++) {
          promise_resolver.promise().ThenOn(thread_c_->task_runner(), FROM_HERE,
                                            then_task);
        }
      }));

  run_loop.Run();
}

TEST_F(MultiThreadedPromiseTest, CrossThreadThensOrdering) {
  constexpr int kNumThenTasks = 1000;
  constexpr int kNumRepetitions = 25;
  for (int repetition = 0; repetition < kNumRepetitions; ++repetition) {
    RunLoop run_loop;

    std::vector<int> order;
    std::vector<OnceCallback<void()>> then_tasks;

    for (int i = 0; i < kNumThenTasks; ++i) {
      then_tasks.push_back(
          BindOnce(BindLambdaForTesting([&order, &run_loop, i]() {
            order.push_back(i);
            if (i == (kNumThenTasks - 1)) {
              run_loop.Quit();
            }
          })));
    }

    ManualPromiseResolver<void> promise_resolver(FROM_HERE);
    auto resolve_callback = promise_resolver.GetResolveCallback();

    thread_a_->task_runner()->PostTask(
        FROM_HERE, BindLambdaForTesting([&]() {
          // Post 500 thens.
          for (int i = 0; i < kNumThenTasks / 2; ++i) {
            promise_resolver.promise().ThenOn(
                thread_c_->task_runner(), FROM_HERE, std::move(then_tasks[i]));
          }

          // Post a task onto |thread_b| to resolve |promise_resolver|.
          // This should run at an undefined time yet all the thens should run.
          thread_b_->task_runner()->PostTask(FROM_HERE,
                                             std::move(resolve_callback));

          // Post another 500 thens.
          for (int i = kNumThenTasks / 2; i < kNumThenTasks; ++i) {
            promise_resolver.promise().ThenOn(
                thread_c_->task_runner(), FROM_HERE, std::move(then_tasks[i]));
          }
        }));

    run_loop.Run();
    for (int i = 0; i < kNumThenTasks; ++i) {
      EXPECT_EQ(order[i], i);
    }
  }
}

TEST_F(PromiseTest, ThreadPoolThenChain) {
  ManualPromiseResolver<std::vector<size_t>> p(FROM_HERE);
  auto main_sequence = SequencedTaskRunnerHandle::Get();

  RunLoop run_loop;
  p.promise()
      .ThenOn({ThreadPool(), TaskPriority::USER_BLOCKING}, FROM_HERE,
              BindLambdaForTesting([&](std::vector<size_t> result) {
                EXPECT_FALSE(main_sequence->RunsTasksInCurrentSequence());
                result.push_back(1);
                return result;
              }))
      .ThenOn({ThreadPool(), TaskPriority::USER_BLOCKING}, FROM_HERE,
              BindLambdaForTesting([&](std::vector<size_t> result) {
                EXPECT_FALSE(main_sequence->RunsTasksInCurrentSequence());
                result.push_back(2);
                return result;
              }))
      .ThenOn({ThreadPool(), TaskPriority::USER_BLOCKING}, FROM_HERE,
              BindLambdaForTesting([&](std::vector<size_t> result) {
                EXPECT_FALSE(main_sequence->RunsTasksInCurrentSequence());
                result.push_back(3);
                return result;
              }))
      .ThenHere(FROM_HERE,
                BindLambdaForTesting([&](std::vector<size_t> result) {
                  EXPECT_TRUE(main_sequence->RunsTasksInCurrentSequence());
                  EXPECT_THAT(result, ElementsAre(0u, 1u, 2u, 3u));
                  run_loop.Quit();
                }));

  p.Resolve(std::vector<size_t>{0});
  run_loop.Run();
}

TEST_F(PromiseTest, All) {
  ManualPromiseResolver<float> p1(FROM_HERE);
  ManualPromiseResolver<int> p2(FROM_HERE);
  ManualPromiseResolver<bool> p3(FROM_HERE);
  Promise<std::tuple<float, int, bool>> p =
      Promises::All(FROM_HERE, p1.promise(), p2.promise(), p3.promise());

  RunLoop run_loop;
  p.ThenHere(FROM_HERE, BindLambdaForTesting(
                            [&](const std::tuple<float, int, bool>& result) {
                              EXPECT_EQ(1.234f, std::get<0>(result));
                              EXPECT_EQ(1234, std::get<1>(result));
                              EXPECT_TRUE(std::get<2>(result));
                              run_loop.Quit();
                            }));

  p1.Resolve(1.234f);
  p2.Resolve(1234);
  p3.Resolve(true);
  run_loop.Run();
}

TEST_F(PromiseTest, AllWithCurriedPromises) {
  ManualPromiseResolver<float> a1(FROM_HERE);
  ManualPromiseResolver<int> a2(FROM_HERE);
  ManualPromiseResolver<bool> a3(FROM_HERE);
  ManualPromiseResolver<void> p(FROM_HERE);

  Promise<float> p1 = p.promise().ThenHere(
      FROM_HERE, BindLambdaForTesting([&]() { return a1.promise(); }));
  Promise<int> p2 = p.promise().ThenHere(
      FROM_HERE, BindLambdaForTesting([&]() { return a2.promise(); }));
  Promise<bool> p3 = p.promise().ThenHere(
      FROM_HERE, BindLambdaForTesting([&]() { return a3.promise(); }));

  Promise<std::tuple<float, int, bool>> all =
      Promises::All(FROM_HERE, p1, p2, p3);

  RunLoop run_loop;
  all.ThenHere(FROM_HERE, BindLambdaForTesting(
                              [&](const std::tuple<float, int, bool>& result) {
                                EXPECT_EQ(1.234f, std::get<0>(result));
                                EXPECT_EQ(1234, std::get<1>(result));
                                EXPECT_TRUE(std::get<2>(result));
                                run_loop.Quit();
                              }));

  p.Resolve();
  a1.Resolve(1.234f);
  a2.Resolve(1234);
  a3.Resolve(true);

  run_loop.Run();
}

TEST_F(PromiseTest, AllUnpackTuple) {
  ManualPromiseResolver<float> p1(FROM_HERE);
  ManualPromiseResolver<int> p2(FROM_HERE);
  ManualPromiseResolver<bool> p3(FROM_HERE);

  RunLoop run_loop;
  Promises::All(FROM_HERE, p1.promise(), p2.promise(), p3.promise())
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](float a, int b, bool c) {
                  EXPECT_EQ(1.234f, a);
                  EXPECT_EQ(1234, b);
                  EXPECT_TRUE(c);
                  run_loop.Quit();
                }));

  p1.Resolve(1.234f);
  p2.Resolve(1234);
  p3.Resolve(true);
  run_loop.Run();
}

TEST_F(PromiseTest, AllRejectString) {
  ManualPromiseResolver<float, std::string> p1(FROM_HERE);
  ManualPromiseResolver<int, std::string> p2(FROM_HERE);
  ManualPromiseResolver<bool, std::string> p3(FROM_HERE);

  RunLoop run_loop;
  Promises::All(FROM_HERE, p1.promise(), p2.promise(), p3.promise())
      .ThenHere(
          FROM_HERE,
          BindLambdaForTesting([&](const std::tuple<float, int, bool>& result) {
            FAIL() << "We shouldn't get here, the promise was rejected!";
            run_loop.Quit();
          }),
          BindLambdaForTesting([&](const std::string& err) {
            EXPECT_EQ("Whoops!", err);
            run_loop.Quit();
          }));

  p1.Reject("Whoops!");
  run_loop.Run();
}

TEST_F(PromiseTest, AllWithSingleValue) {
  ManualPromiseResolver<int> p1(FROM_HERE);

  RunLoop run_loop;
  Promises::All(FROM_HERE, p1.promise())
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](int value1) {
                  EXPECT_EQ(value1, 1);
                  run_loop.Quit();
                }));

  p1.Resolve(1);
  run_loop.Run();
}

TEST_F(PromiseTest, AllIntVoid) {
  ManualPromiseResolver<int> p1(FROM_HERE);
  ManualPromiseResolver<void> p2(FROM_HERE);

  RunLoop run_loop;
  Promises::All(FROM_HERE, p1.promise(), p2.promise())
      .ThenHere(FROM_HERE,
                BindLambdaForTesting([&](const std::tuple<int, Void>& result) {
                  EXPECT_EQ(1234, std::get<0>(result));
                  run_loop.Quit();
                }));

  p1.Resolve(1234);
  p2.Resolve();
  run_loop.Run();
}

TEST_F(PromiseTest, AllMoveOnlyType) {
  ManualPromiseResolver<std::unique_ptr<float>> p1(FROM_HERE);
  ManualPromiseResolver<std::unique_ptr<int>> p2(FROM_HERE);
  ManualPromiseResolver<std::unique_ptr<bool>> p3(FROM_HERE);

  RunLoop run_loop;
  Promises::All(FROM_HERE, p1.promise(), p2.promise(), p3.promise())
      .ThenHere(FROM_HERE,
                BindLambdaForTesting(
                    [&](std::tuple<std::unique_ptr<float>, std::unique_ptr<int>,
                                   std::unique_ptr<bool>> result) {
                      EXPECT_EQ(1.234f, *std::get<0>(result));
                      EXPECT_EQ(1234, *std::get<1>(result));
                      EXPECT_TRUE(*std::get<2>(result));
                      run_loop.Quit();
                    }));

  p1.Resolve(std::make_unique<float>(1.234f));
  p2.Resolve(std::make_unique<int>(1234));
  p3.Resolve(std::make_unique<bool>(true));
  run_loop.Run();
}

TEST_F(PromiseTest, AllIntWithVoidThen) {
  ManualPromiseResolver<int> p1(FROM_HERE);
  ManualPromiseResolver<int> p2(FROM_HERE);
  ManualPromiseResolver<int> p3(FROM_HERE);

  // You can choose to ignore the result.
  RunLoop run_loop;
  Promises::All(FROM_HERE, p1.promise(), p2.promise(), p3.promise())
      .ThenHere(FROM_HERE, run_loop.QuitClosure());

  p1.Resolve(1);
  p2.Resolve(2);
  p3.Resolve(3);
  run_loop.Run();
}

TEST_F(PromiseTest, AllIntContainer) {
  ManualPromiseResolver<int> mpr1(FROM_HERE);
  ManualPromiseResolver<int> mpr2(FROM_HERE);
  ManualPromiseResolver<int> mpr3(FROM_HERE);
  ManualPromiseResolver<int> mpr4(FROM_HERE);

  std::vector<Promise<int>> promises;
  promises.push_back(mpr1.promise());
  promises.push_back(mpr2.promise());
  promises.push_back(mpr3.promise());
  promises.push_back(mpr4.promise());

  RunLoop run_loop;
  Promises::All(FROM_HERE, promises)
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](std::vector<int> result) {
                  EXPECT_THAT(result, ElementsAre(10, 20, 30, 40));
                  run_loop.Quit();
                }));

  mpr1.Resolve(10);
  mpr2.Resolve(20);
  mpr3.Resolve(30);
  mpr4.Resolve(40);
  run_loop.Run();
}

TEST_F(PromiseTest, AllEmptyIntContainer) {
  std::vector<Promise<int>> promises;

  RunLoop run_loop;
  Promises::All(FROM_HERE, promises)
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](std::vector<int> result) {
                  EXPECT_TRUE(result.empty());
                  run_loop.Quit();
                }));

  run_loop.Run();
}

TEST_F(PromiseTest, AllIntStringContainerReject) {
  ManualPromiseResolver<int, std::string> mpr1(FROM_HERE);
  ManualPromiseResolver<int, std::string> mpr2(FROM_HERE);
  ManualPromiseResolver<int, std::string> mpr3(FROM_HERE);
  ManualPromiseResolver<int, std::string> mpr4(FROM_HERE);

  std::vector<Promise<int, std::string>> promises;
  promises.push_back(mpr1.promise());
  promises.push_back(mpr2.promise());
  promises.push_back(mpr3.promise());
  promises.push_back(mpr4.promise());

  RunLoop run_loop;
  Promises::All(FROM_HERE, promises)
      .ThenHere(FROM_HERE, BindLambdaForTesting([&](std::vector<int> result) {
                  FAIL() << "We shouldn't get here, the promise was rejected!";
                  run_loop.Quit();
                }),
                BindLambdaForTesting([&](const std::string& err) {
                  EXPECT_EQ("Oh dear", err);
                  run_loop.Quit();
                }));

  mpr2.Reject("Oh dear");
  run_loop.Run();
}

TEST_F(PromiseTest, AllVoidContainer) {
  ManualPromiseResolver<void> mpr1(FROM_HERE);
  ManualPromiseResolver<void> mpr2(FROM_HERE);
  ManualPromiseResolver<void> mpr3(FROM_HERE);
  ManualPromiseResolver<void> mpr4(FROM_HERE);

  std::vector<Promise<void>> promises;
  promises.push_back(mpr1.promise());
  promises.push_back(mpr2.promise());
  promises.push_back(mpr3.promise());
  promises.push_back(mpr4.promise());

  RunLoop run_loop;
  Promise<void> result =
      Promises::All(FROM_HERE, promises)
          .ThenHere(FROM_HERE,
                    BindLambdaForTesting([&]() { run_loop.Quit(); }));

  mpr1.Resolve();
  mpr2.Resolve();
  mpr3.Resolve();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(result.IsResolvedForTesting());

  mpr4.Resolve();
  run_loop.Run();
  EXPECT_TRUE(result.IsResolvedForTesting());
}

TEST_F(PromiseTest, AllVoidIntContainerReject) {
  ManualPromiseResolver<void, int> mpr1(FROM_HERE);
  ManualPromiseResolver<void, int> mpr2(FROM_HERE);
  ManualPromiseResolver<void, int> mpr3(FROM_HERE);
  ManualPromiseResolver<void, int> mpr4(FROM_HERE);

  std::vector<Promise<void, int>> promises;
  promises.push_back(mpr1.promise());
  promises.push_back(mpr2.promise());
  promises.push_back(mpr3.promise());
  promises.push_back(mpr4.promise());

  RunLoop run_loop;
  Promises::All(FROM_HERE, promises)
      .ThenHere(FROM_HERE, BindLambdaForTesting([&]() {
                  FAIL() << "We shouldn't get here, the promise was rejected!";
                  run_loop.Quit();
                }),
                BindLambdaForTesting([&](int err) {
                  EXPECT_EQ(-1, err);
                  run_loop.Quit();
                }));

  mpr1.Reject(-1);
  run_loop.Run();
}

TEST_F(PromiseTest, AllVoidContainerReject) {
  ManualPromiseResolver<void, void> mpr1(FROM_HERE);
  ManualPromiseResolver<void, void> mpr2(FROM_HERE);
  ManualPromiseResolver<void, void> mpr3(FROM_HERE);
  ManualPromiseResolver<void, void> mpr4(FROM_HERE);

  std::vector<Promise<void, void>> promises;
  promises.push_back(mpr1.promise());
  promises.push_back(mpr2.promise());
  promises.push_back(mpr3.promise());
  promises.push_back(mpr4.promise());

  RunLoop run_loop;
  Promises::All(FROM_HERE, promises)
      .ThenHere(FROM_HERE, BindLambdaForTesting([&]() {
                  FAIL() << "We shouldn't get here, the promise was rejected!";
                  run_loop.Quit();
                }),
                run_loop.QuitClosure());

  mpr4.Reject();
  run_loop.Run();
}

TEST_F(PromiseTest, AllVoidContainerMultipleRejectsBeforeExecute) {
  ManualPromiseResolver<void, void> mpr1(FROM_HERE);
  ManualPromiseResolver<void, void> mpr2(FROM_HERE);
  ManualPromiseResolver<void, void> mpr3(FROM_HERE);
  ManualPromiseResolver<void, void> mpr4(FROM_HERE);

  std::vector<Promise<void, void>> promises;
  promises.push_back(mpr1.promise());
  promises.push_back(mpr2.promise());
  promises.push_back(mpr3.promise());
  promises.push_back(mpr4.promise());

  RunLoop run_loop;
  Promises::All(FROM_HERE, promises)
      .ThenHere(FROM_HERE, BindLambdaForTesting([&]() {
                  FAIL() << "We shouldn't get here, the promise was rejected!";
                  run_loop.Quit();
                }),
                run_loop.QuitClosure());

  mpr1.Reject();
  mpr2.Reject();
  mpr4.Reject();
  run_loop.Run();
}

TEST_F(PromiseTest, AllVoidContainerMultipleRejectsAfterExecute) {
  ManualPromiseResolver<void, void> mpr1(FROM_HERE);
  ManualPromiseResolver<void, void> mpr2(FROM_HERE);
  ManualPromiseResolver<void, void> mpr3(FROM_HERE);
  ManualPromiseResolver<void, void> mpr4(FROM_HERE);

  std::vector<Promise<void, void>> promises;
  promises.push_back(mpr1.promise());
  promises.push_back(mpr2.promise());
  promises.push_back(mpr3.promise());
  promises.push_back(mpr4.promise());

  RunLoop run_loop;
  Promises::All(FROM_HERE, promises)
      .ThenHere(FROM_HERE, BindLambdaForTesting([&]() {
                  FAIL() << "We shouldn't get here, the promise was rejected!";
                  run_loop.Quit();
                }),
                run_loop.QuitClosure());

  mpr1.Reject();
  run_loop.Run();
  mpr2.Reject();
  mpr4.Reject();
}

TEST_F(PromiseTest, TakeResolveValueForTesting) {
  ManualPromiseResolver<void> p1(FROM_HERE);

  Promise<int> p2 =
      p1.promise().ThenHere(FROM_HERE, BindOnce([]() { return 123; }));

  p1.Resolve();

  EXPECT_EQ(123, p2.TakeResolveValueForTesting());
}

TEST_F(PromiseTest, TakeResolveValueForTestingMoveOnlyType) {
  ManualPromiseResolver<void> p1(FROM_HERE);

  Promise<std::unique_ptr<int>> p2 = p1.promise().ThenHere(
      FROM_HERE, BindOnce([]() { return std::make_unique<int>(123); }));

  p1.Resolve();

  EXPECT_EQ(123, *p2.TakeResolveValueForTesting());
}

TEST_F(PromiseTest, TakeResolveValueForTestingNotResolved) {
  ManualPromiseResolver<int, int> p1(FROM_HERE,
                                     RejectPolicy::kCatchNotRequired);

  p1.Reject(123);

  EXPECT_DCHECK_DEATH({ p1.promise().TakeResolveValueForTesting(); });
}

TEST_F(PromiseTest, TakeRejectedValueForTesting) {
  ManualPromiseResolver<void, void> p1(FROM_HERE);

  Promise<int, int> p2 = p1.promise().ThenHere(
      FROM_HERE, BindOnce([]() { return Resolved<int>(123); }),
      BindOnce([]() { return Rejected<int>(456); }));

  p1.Reject();

  EXPECT_EQ(456, p2.TakeRejectValueForTesting());
}

TEST_F(PromiseTest, TakeRejectedValueForTestingMoveOnlyType) {
  ManualPromiseResolver<void, std::unique_ptr<int>> p1(FROM_HERE);

  p1.Reject(std::make_unique<int>(456));

  EXPECT_EQ(456, *p1.promise().TakeRejectValueForTesting());
}

TEST_F(PromiseTest, TakeRejectedValueForTestingNotRejected) {
  ManualPromiseResolver<int, int> p1(FROM_HERE,
                                     RejectPolicy::kCatchNotRequired);

  p1.Resolve(123);

  EXPECT_DCHECK_DEATH({ p1.promise().TakeRejectValueForTesting(); });
}

}  // namespace base

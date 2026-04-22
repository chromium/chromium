// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/weak_value_table.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

using subtle::RefCountedWeakValue;
using subtle::WeakValueTable;

class TestValue : public RefCountedWeakValue<std::string, TestValue> {
 public:
  explicit TestValue(std::string key, OnceClosure on_destroy = NullCallback())
      : key_(std::move(key)), on_destroy_(std::move(on_destroy)) {}

  const std::string& GetKey() const { return key_; }

  void set_on_release_race_closure(base::OnceClosure closure) {
    on_release_race_ = std::move(closure);
  }

  void InternalTestHookForReleaseRace() {
    if (on_release_race_) {
      std::move(on_release_race_).Run();
    }
  }

 private:
  friend RefCountedWeakValue;

  ~TestValue() {
    if (on_destroy_) {
      std::move(on_destroy_).Run();
    }
  }

  std::string key_;
  OnceClosure on_release_race_;
  OnceClosure on_destroy_;
};

TEST(WeakValueTableTest, FindOrCreate) {
  WeakValueTable<std::string, TestValue> table;

  bool hello_was_destroyed = false;
  bool world_was_destroyed = false;
  scoped_refptr<TestValue> hello;

  {
    bool called = false;
    hello = table.FindOrCreate("hello", [&] {
      called = true;
      return MakeRefCounted<TestValue>(
          "hello", BindLambdaForTesting([&] { hello_was_destroyed = true; }));
    });
    EXPECT_TRUE(called);
    EXPECT_EQ("hello", hello->GetKey());
  }

  // The callable should not be invoked when trying to insert another value with
  // the same key; the value already in the table should be returned instead.
  {
    bool called = false;
    scoped_refptr<TestValue> hello2 =
        table.FindOrCreate("hello", [&] -> scoped_refptr<TestValue> {
          called = true;
          return nullptr;
        });
    EXPECT_FALSE(called);
    EXPECT_EQ(hello.get(), hello2.get());
  }

  {
    bool called = false;
    scoped_refptr<TestValue> world = table.FindOrCreate("world", [&] {
      called = true;
      return MakeRefCounted<TestValue>(
          "world", BindLambdaForTesting([&] { world_was_destroyed = true; }));
    });
    EXPECT_TRUE(called);
    EXPECT_EQ("world", world->GetKey());

    EXPECT_FALSE(table.empty());
    EXPECT_FALSE(world_was_destroyed);
    world = nullptr;
    EXPECT_FALSE(table.empty());
    EXPECT_TRUE(world_was_destroyed);
  }

  EXPECT_FALSE(table.empty());
  EXPECT_FALSE(hello_was_destroyed);
  hello = nullptr;
  EXPECT_TRUE(table.empty());
  EXPECT_TRUE(hello_was_destroyed);
}

TEST(WeakValueTableTest, FindOrCreateNullptr) {
  WeakValueTable<std::string, TestValue> table;

  {
    bool called = false;
    scoped_refptr<TestValue> value =
        table.FindOrCreate("hello", [&] -> scoped_refptr<TestValue> {
          called = true;
          return nullptr;
        });
    EXPECT_TRUE(called);
    EXPECT_EQ(nullptr, value);
  }

  // The first insert failed since the callable returned nullptr, so the second
  // insert should invoke the callable.
  {
    bool called = false;
    scoped_refptr<TestValue> value = table.FindOrCreate("hello", [&] {
      called = true;
      return MakeRefCounted<TestValue>("hello");
    });
    EXPECT_TRUE(called);
    EXPECT_EQ("hello", value->GetKey());
  }
}

TEST(WeakValueTableTest, DestroyNeverInsertedValue) {
  base::MakeRefCounted<TestValue>("hello");
}

TEST(WeakValueTableTest, RaceToCreate) {
  WeakValueTable<std::string, TestValue> table;

  bool first_destroyed = false;
  bool second_destroyed = false;
  scoped_refptr<TestValue> keep_alive;

  scoped_refptr<TestValue> value = table.FindOrCreate("hello", [&] {
    auto first = MakeRefCounted<TestValue>(
        "hello", BindLambdaForTesting([&] { first_destroyed = true; }));
    // While inserting the first element, reentrantly insert the second one.
    auto second = table.FindOrCreate("hello", [&] {
      return MakeRefCounted<TestValue>(
          "hello", BindLambdaForTesting([&] { second_destroyed = true; }));
    });
    // Both objects are still live and should have distinct addresses.
    EXPECT_NE(first, second);
    // Allow the reference to escape; otherwise, `second` will be destroyed
    // and removed from the table before returning to the first
    // `FindOrCreate()` call.
    keep_alive = std::move(second);
    return first;
  });

  EXPECT_FALSE(table.empty());
  EXPECT_TRUE(first_destroyed);
  EXPECT_FALSE(second_destroyed);
  // At this point, both `keep_alive` and `value` should be the same object.
  EXPECT_EQ(keep_alive, value);
  EXPECT_FALSE(keep_alive->HasOneRef());
  value = nullptr;
  EXPECT_TRUE(keep_alive->HasOneRef());
  keep_alive = nullptr;
  EXPECT_TRUE(second_destroyed);
  EXPECT_TRUE(table.empty());
}

TEST(WeakValueTableTest, RaceRemovalAndFind) {
  WeakValueTable<std::string, TestValue> table;

  bool was_destroyed = false;
  scoped_refptr<TestValue> keep_alive;

  scoped_refptr<TestValue> value = table.FindOrCreate("hello", [&] {
    auto value = base::MakeRefCounted<TestValue>(
        "hello", base::BindLambdaForTesting([&] { was_destroyed = true; }));
    value->set_on_release_race_closure(base::BindLambdaForTesting([&] {
      bool called = false;
      keep_alive = table.FindOrCreate("hello", [&] -> scoped_refptr<TestValue> {
        called = true;
        return nullptr;
      });
      EXPECT_FALSE(called);
    }));
    return value;
  });

  EXPECT_FALSE(table.empty());
  EXPECT_FALSE(was_destroyed);
  EXPECT_TRUE(value->HasOneRef());
  EXPECT_EQ(nullptr, keep_alive);
  // Trigger the race by releasing the last reference. The on release race
  // callback bound earlier will take a reference right before the value tries
  // to release it's last reference, so the end result should be that the value
  // is not destroyed yet.
  value = nullptr;
  EXPECT_FALSE(table.empty());
  EXPECT_FALSE(was_destroyed);
  ASSERT_TRUE(!!keep_alive);
  EXPECT_TRUE(keep_alive->HasOneRef());
  keep_alive = nullptr;
  EXPECT_TRUE(table.empty());
  EXPECT_TRUE(was_destroyed);
}

TEST(WeakValueTableTest, CreateDuringValueDestruction) {
  WeakValueTable<std::string, TestValue> table;

  bool first_destroyed = false;
  bool second_destroyed = false;
  scoped_refptr<TestValue> second;

  scoped_refptr<TestValue> first = table.FindOrCreate("hello", [&] {
    return MakeRefCounted<TestValue>(
        "hello", BindLambdaForTesting([&] {
          first_destroyed = true;
          second = table.FindOrCreate("hello", [&] {
            return MakeRefCounted<TestValue>("hello", BindLambdaForTesting([&] {
                                               second_destroyed = true;
                                             }));
          });
        }));
  });

  EXPECT_FALSE(first_destroyed);
  EXPECT_FALSE(second_destroyed);
  first = nullptr;
  EXPECT_TRUE(first_destroyed);
  EXPECT_FALSE(second_destroyed);
  second = nullptr;
  EXPECT_TRUE(second_destroyed);
}

TEST(WeakValueTableTest, DestroyNonEmptyTable) {
  std::optional<WeakValueTable<std::string, TestValue>> table(std::in_place);
  scoped_refptr<TestValue> value = table->FindOrCreate(
      "hello", [] { return MakeRefCounted<TestValue>("hello"); });

  // `value` is still live
  EXPECT_FALSE(table->empty());
  // Destroying a non-empty table is unsafe so crash instead.
  EXPECT_CHECK_DEATH(table.reset());
}

TEST(WeakValueTableTest, CallableReturnsPointerWithMultipleRefs) {
  WeakValueTable<std::string, TestValue> table;
  EXPECT_CHECK_DEATH({
    scoped_refptr<TestValue> extra_ref;
    (void)table.FindOrCreate("hello", [&] {
      auto value = MakeRefCounted<TestValue>("hello");
      extra_ref = value;
      return value;
    });
  });
}

TEST(WeakValueTableTest, ReentrantCreateDifferentKey) {
  WeakValueTable<std::string, TestValue> table;

  scoped_refptr<TestValue> first = table.FindOrCreate("hello", [&] {
    // Re-entrantly insert a different key while constructing "hello"
    scoped_refptr<TestValue> second = table.FindOrCreate(
        "world", [] { return MakeRefCounted<TestValue>("world"); });
    EXPECT_EQ("world", second->GetKey());
    return MakeRefCounted<TestValue>("hello");
  });

  EXPECT_EQ("hello", first->GetKey());
  EXPECT_FALSE(table.empty());
}

TEST(WeakValueTableTest, MismatchedKey) {
  WeakValueTable<std::string, TestValue> table;
  // This should crash because TestValue::GetKey() will return "world", which
  // does not match with the originally-requested key "hello".
  EXPECT_CHECK_DEATH((void)table.FindOrCreate(
      "hello", [] { return MakeRefCounted<TestValue>("world"); }));
}

namespace {

class WorkerThread : public base::SimpleThread {
 public:
  WorkerThread(base::WaitableEvent* event,
               WeakValueTable<std::string, TestValue>* table)
      : SimpleThread("WorkerThread"), event_(event), table_(table) {}

  void Run() override {
    event_->Wait();
    for (int i = 0; i < 16; ++i) {
      values_.push_back(table_->FindOrCreate(
          "hello", [] { return MakeRefCounted<TestValue>("hello"); }));
      values_.push_back(table_->FindOrCreate(
          "world", [] { return MakeRefCounted<TestValue>("world"); }));
    }
    values_.clear();
  }

 private:
  const raw_ptr<base::WaitableEvent> event_;
  const raw_ptr<WeakValueTable<std::string, TestValue>> table_;
  std::vector<scoped_refptr<TestValue>> values_;
};

}  // namespace

TEST(WeakValueTableTest, MultithreadedFindOrCreate) {
  WeakValueTable<std::string, TestValue> table;
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);

  std::vector<std::unique_ptr<WorkerThread>> threads;
  for (int i = 0; i < 32; ++i) {
    threads.push_back(std::make_unique<WorkerThread>(&event, &table));
    threads.back()->Start();
  }

  event.Signal();

  for (auto& thread : threads) {
    thread->Join();
  }

  EXPECT_TRUE(table.empty());
}

}  // namespace base

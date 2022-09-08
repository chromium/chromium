// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_auto_reset.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

template <class T>
class HasWeakFactory {
 public:
  HasWeakFactory() = default;
  ~HasWeakFactory() = default;

  // Returns a WeakAutoReset that temporarily sets value_ to `value`.
  auto SetValueScoped(T value) {
    return WeakAutoReset(factory_.GetWeakPtr(), &HasWeakFactory::value_,
                         std::move(value));
  }

  void set_value(T value) { value_ = std::move(value); }
  const T& value() const { return value_; }

  WeakPtr<HasWeakFactory> GetWeakPtr() { return factory_.GetWeakPtr(); }

 private:
  T value_ = T();
  WeakPtrFactory<HasWeakFactory> factory_{this};
};

}  // namespace

TEST(WeakAutoResetTest, DefaultConstructor) {
  WeakAutoReset<HasWeakFactory<int>, int> empty;
}

TEST(WeakAutoResetTest, SingleAutoReset) {
  HasWeakFactory<int> hwf;
  {
    WeakAutoReset reset = hwf.SetValueScoped(1);
    EXPECT_EQ(1, hwf.value());
  }
  EXPECT_EQ(0, hwf.value());
}

TEST(WeakAutoResetTest, SingleAutoResetObjectDestroyed) {
  auto hwf = std::make_unique<HasWeakFactory<int>>();
  WeakAutoReset reset = hwf->SetValueScoped(1);
  EXPECT_EQ(1, hwf->value());
  hwf.reset();
  // ASAN will crash here if we don't correctly detect that hwf has gone away.
}

TEST(WeakAutoResetTest, MultipleNested) {
  HasWeakFactory<int> hwf;
  {
    WeakAutoReset reset = hwf.SetValueScoped(1);
    EXPECT_EQ(1, hwf.value());
    {
      WeakAutoReset reset2 = hwf.SetValueScoped(2);
      EXPECT_EQ(2, hwf.value());
    }
    EXPECT_EQ(1, hwf.value());
  }
  EXPECT_EQ(0, hwf.value());
}

TEST(WeakAutoResetTest, MultipleNestedObjectDestroyed) {
  auto hwf = std::make_unique<HasWeakFactory<int>>();
  WeakAutoReset reset = hwf->SetValueScoped(1);
  EXPECT_EQ(1, hwf->value());
  WeakAutoReset reset2 = hwf->SetValueScoped(2);
  EXPECT_EQ(2, hwf->value());
  hwf.reset();
  // ASAN will crash here if we don't correctly detect that hwf has gone away.
}

TEST(WeakAutoResetTest, MoveAssignmentTransfersOwnership) {
  HasWeakFactory<int> hwf;
  // Create an auto-reset outside of a scope.
  WeakAutoReset reset = hwf.SetValueScoped(1);
  {
    WeakAutoReset<HasWeakFactory<int>, int> reset2;
    EXPECT_EQ(1, hwf.value());
    // Move the auto-reset to an instance inside the scope. This should not
    // cause the value to reset.
    reset2 = std::move(reset);
    EXPECT_EQ(1, hwf.value());
  }
  // Because the active auto-reset went away with the scope, the original value
  // should be restored.
  EXPECT_EQ(0, hwf.value());
}

TEST(WeakAutoResetTest, MoveAssignmentResetsOldValue) {
  HasWeakFactory<int> hwf1;
  HasWeakFactory<int> hwf2;
  WeakAutoReset reset = hwf1.SetValueScoped(1);
  WeakAutoReset reset2 = hwf2.SetValueScoped(2);
  EXPECT_EQ(1, hwf1.value());
  EXPECT_EQ(2, hwf2.value());

  // Overwriting the first with the second should reset the first value, but not
  // the second.
  reset = std::move(reset2);
  EXPECT_EQ(0, hwf1.value());
  EXPECT_EQ(2, hwf2.value());

  // Overwriting the moved value with a default value should have no effect.
  reset2 = WeakAutoReset<HasWeakFactory<int>, int>();

  // Overwriting the live auto-reset with a default value should reset the other
  // value.
  reset = WeakAutoReset<HasWeakFactory<int>, int>();
  EXPECT_EQ(0, hwf1.value());
  EXPECT_EQ(0, hwf2.value());
}

TEST(WeakAutoResetTest, MoveAssignmentToSelfIsNoOp) {
  HasWeakFactory<int> hwf;
  {
    WeakAutoReset reset = hwf.SetValueScoped(1);
    EXPECT_EQ(1, hwf.value());

    // Move the auto-reset to itself. This should have no effect. We'll need to
    // create an intermediate so that we don't get a compile error.
    auto* const reset_ref = &reset;
    reset = std::move(*reset_ref);
    EXPECT_EQ(1, hwf.value());
  }
  // The auto-reset goes out of scope, resetting the value.
  EXPECT_EQ(0, hwf.value());
}

TEST(WeakAutoResetTest, DeleteTargetObjectAfterMoveIsSafe) {
  auto hwf = std::make_unique<HasWeakFactory<int>>();
  WeakAutoReset reset = hwf->SetValueScoped(1);
  WeakAutoReset reset2 = std::move(reset);
  hwf.reset();
  // ASAN will crash here if we don't correctly detect that hwf has gone away.
}

using HasWeakFactoryPointer = std::unique_ptr<HasWeakFactory<int>>;

TEST(WeakAutoResetTest, TestSafelyMovesValue) {
  // We'll use an object that owns another object while keeping a weak reference
  // to the inner object to determine its lifetime.
  auto inner = std::make_unique<HasWeakFactory<int>>();
  auto weak_ptr = inner->GetWeakPtr();
  auto outer = std::make_unique<HasWeakFactory<HasWeakFactoryPointer>>();
  outer->set_value(std::move(inner));
  ASSERT_TRUE(weak_ptr);

  {
    // Transfer ownership of the inner object to the auto-reset.
    WeakAutoReset reset = outer->SetValueScoped(HasWeakFactoryPointer());
    EXPECT_TRUE(weak_ptr);
    EXPECT_FALSE(outer->value());
  }

  // Transfer ownership back to the outer object.
  EXPECT_TRUE(weak_ptr);
  EXPECT_TRUE(outer->value());

  // Destroying the outer object destroys the inner object.
  outer.reset();
  EXPECT_FALSE(weak_ptr);
}

TEST(WeakAutoResetTest, TestSafelyMovesValueAndThenDestroysIt) {
  // We'll use an object that owns another object while keeping a weak reference
  // to the inner object to determine its lifetime.
  auto inner = std::make_unique<HasWeakFactory<int>>();
  auto weak_ptr = inner->GetWeakPtr();
  auto outer = std::make_unique<HasWeakFactory<HasWeakFactoryPointer>>();
  outer->set_value(std::move(inner));
  ASSERT_TRUE(weak_ptr);

  {
    // Transfer ownership of the inner object to the auto-reset.
    WeakAutoReset reset = outer->SetValueScoped(HasWeakFactoryPointer());
    EXPECT_TRUE(weak_ptr);
    EXPECT_FALSE(outer->value());

    // Destroy the outer object. The auto-reset still owns the old inner object.
    outer.reset();
    EXPECT_TRUE(weak_ptr);
  }

  // Onwership can't be transferred back so the inner object is destroyed.
  EXPECT_FALSE(weak_ptr);
}

TEST(WeakAutoResetTest, TestMoveConstructorMovesOldValue) {
  // We'll use an object that owns another object while keeping a weak reference
  // to the inner object to determine its lifetime.
  auto inner = std::make_unique<HasWeakFactory<int>>();
  auto weak_ptr = inner->GetWeakPtr();
  auto outer = std::make_unique<HasWeakFactory<HasWeakFactoryPointer>>();
  outer->set_value(std::move(inner));
  ASSERT_TRUE(weak_ptr);

  {
    // Transfer ownership of the inner object to the auto-reset.
    WeakAutoReset reset = outer->SetValueScoped(HasWeakFactoryPointer());
    EXPECT_TRUE(weak_ptr);
    EXPECT_FALSE(outer->value());

    {
      // Move ownership of the old object to a new auto-reset.
      WeakAutoReset reset2(std::move(reset));
      EXPECT_TRUE(weak_ptr);
      EXPECT_FALSE(outer->value());
    }

    // Destroying the second auto-reset transfers ownership back to the outer
    // object.
    EXPECT_TRUE(weak_ptr);
    EXPECT_TRUE(outer->value());
  }
}

TEST(WeakAutoResetTest, TestMoveAssignmentMovesOldValue) {
  // We'll use an object that owns another object while keeping a weak reference
  // to the inner object to determine its lifetime.
  auto inner = std::make_unique<HasWeakFactory<int>>();
  auto weak_ptr = inner->GetWeakPtr();
  auto outer = std::make_unique<HasWeakFactory<HasWeakFactoryPointer>>();
  outer->set_value(std::move(inner));
  ASSERT_TRUE(weak_ptr);

  {
    // Create an auto-reset that will receive ownership later.
    WeakAutoReset<HasWeakFactory<HasWeakFactoryPointer>, HasWeakFactoryPointer>
        reset;

    {
      // Move ownership of the inner object to an auto-reset.
      WeakAutoReset reset2 = outer->SetValueScoped(HasWeakFactoryPointer());
      EXPECT_TRUE(weak_ptr);
      EXPECT_FALSE(outer->value());

      // Transfer ownership to the other auto-reset.
      reset = std::move(reset2);
      EXPECT_TRUE(weak_ptr);
      EXPECT_FALSE(outer->value());
    }

    // The auto-reset that initially received the value is gone, but the one
    // actually holding the value is still in scope.
    EXPECT_TRUE(weak_ptr);
    EXPECT_FALSE(outer->value());
  }

  // Now both have gone out of scope, so the inner object should be returned to
  // the outer one.
  EXPECT_TRUE(weak_ptr);
  EXPECT_TRUE(outer->value());
}

TEST(WeakAutoResetTest, TestOldAndNewValuesAreSwapped) {
  // We'll use an object that owns another object while keeping a weak reference
  // to the inner object to determine its lifetime.
  auto inner = std::make_unique<HasWeakFactory<int>>();
  auto weak_ptr = inner->GetWeakPtr();
  auto outer = std::make_unique<HasWeakFactory<HasWeakFactoryPointer>>();
  outer->set_value(std::move(inner));
  ASSERT_TRUE(weak_ptr);

  // Create a second inner object that we'll swap with the first.
  auto replacement = std::make_unique<HasWeakFactory<int>>();
  auto weak_ptr2 = replacement->GetWeakPtr();

  {
    // Swap the values.
    WeakAutoReset reset = outer->SetValueScoped(std::move(replacement));
    EXPECT_TRUE(weak_ptr);
    EXPECT_TRUE(weak_ptr2);
    EXPECT_EQ(weak_ptr2.get(), outer->value().get());
  }

  // Unswap the values. The replacement is discarded.
  EXPECT_TRUE(weak_ptr);
  EXPECT_FALSE(weak_ptr2);
  EXPECT_EQ(weak_ptr.get(), outer->value().get());
}

}  // namespace base

// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/safe_ref.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace {

struct BaseClass {};

struct WithWeak : BaseClass {
  ~WithWeak() { self = nullptr; }

  int i = 1;
  WithWeak* self{this};
  base::WeakPtrFactory<WithWeak> factory{this};
};

TEST(SafeRefTest, FromWeakPtrFactory) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
}

TEST(SafeRefTest, Operators) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  // operator->.
  EXPECT_EQ(safe->self->i, 1);  // Will crash if not live.
  // operator*.
  EXPECT_EQ((*safe).self->i, 1);  // Will crash if not live.
}

TEST(SafeRefTest, CanCopyAndMove) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  EXPECT_EQ(safe->self->i, 1);                // Will crash if not live.
  SafeRef<WithWeak> safe2 = safe;             // Copy.
  EXPECT_EQ(safe2->self->i, 1);               // Will crash if not live.
  EXPECT_EQ(safe->self->i, 1);                // Will crash if not live.
  SafeRef<WithWeak> safe3 = std::move(safe);  // Move.
  EXPECT_EQ(safe3->self->i, 1);               // Will crash if not live.
}

TEST(SafeRefTest, StillValidAfterMove) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  SafeRef<WithWeak> safe2 = std::move(safe);  // Move.
  EXPECT_EQ(safe->self->i, 1);                // Will crash if not live.
  EXPECT_EQ(safe2->self->i, 1);               // Will crash if not live.
}

TEST(SafeRefTest, AssignCopyAndMove) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());

  WithWeak with2;
  SafeRef<WithWeak> safe2(with2.factory.GetSafeRef());
  EXPECT_NE(safe->self, &with2);
  safe = safe2;
  EXPECT_EQ(safe->self, &with2);

  WithWeak with3;
  SafeRef<WithWeak> safe3(with3.factory.GetSafeRef());
  EXPECT_NE(safe->self, &with3);
  safe = std::move(safe3);
  EXPECT_EQ(safe->self, &with3);
}

TEST(SafeRefTest, AssignCopyAfterInvalidate) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  SafeRef<WithWeak> safe2(with.factory.GetSafeRef());

  {
    WithWeak with2;
    safe = SafeRef<WithWeak>(with2.factory.GetSafeRef());
  }
  // `safe` is now invalidated (oops), but we won't use it in that state!
  safe = safe2;
  // `safe` is valid again, we can use it.
  EXPECT_EQ(safe->self, &with);
}

TEST(SafeRefTest, AssignMoveAfterInvalidate) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  SafeRef<WithWeak> safe2(with.factory.GetSafeRef());

  {
    WithWeak with2;
    safe = SafeRef<WithWeak>(with2.factory.GetSafeRef());
  }
  // `safe` is now invalidated (oops), but we won't use it in that state!
  safe = std::move(safe2);
  // `safe` is valid again, we can use it.
  EXPECT_EQ(safe->self, &with);
}

TEST(SafeRefDeathTest, ArrowOperatorCrashIfBadPointer) {
  absl::optional<WithWeak> with(absl::in_place);
  SafeRef<WithWeak> safe(with->factory.GetSafeRef());
  with.reset();
  EXPECT_CHECK_DEATH(safe.operator->());  // Will crash since not live.
}

TEST(SafeRefDeathTest, StarOperatorCrashIfBadPointer) {
  absl::optional<WithWeak> with(absl::in_place);
  SafeRef<WithWeak> safe(with->factory.GetSafeRef());
  with.reset();
  EXPECT_CHECK_DEATH(safe.operator*());  // Will crash since not live.
}

TEST(SafeRefTest, ConversionToBaseClassFromCopyAssign) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  SafeRef<BaseClass> base_safe = safe;
  EXPECT_EQ(static_cast<WithWeak*>(&*base_safe)->self, &with);
}

TEST(SafeRefTest, ConversionToBaseClassFromMoveAssign) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  SafeRef<BaseClass> base_safe = std::move(safe);
  EXPECT_EQ(static_cast<WithWeak*>(&*base_safe)->self, &with);
}

TEST(SafeRefTest, CanDerefConst) {
  WithWeak with;
  const SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  EXPECT_EQ(safe->self->i, 1);
  EXPECT_EQ((*safe).self->i, 1);
}

}  // namespace
}  // namespace base

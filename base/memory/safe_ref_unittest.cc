// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/safe_ref.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gtest_util.h"
#include "base/test/memory/dangling_ptr_instrumentation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

struct ReallyBaseClass {};
struct BaseClass : ReallyBaseClass {
  virtual ~BaseClass() = default;
  void VirtualMethod() {}
};
struct OtherBaseClass {
  virtual ~OtherBaseClass() = default;
  virtual void VirtualMethod() {}
};

struct WithWeak final : BaseClass, OtherBaseClass {
  ~WithWeak() final { self = nullptr; }

  void Method() {}

  int i = 1;
  raw_ptr<WithWeak> self{this};
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

TEST(SafeRefTest, ThreeWayComparison) {
  WithWeak with1;
  SafeRef<WithWeak> safe1(with1.factory.GetSafeRef());
  WithWeak with2;
  SafeRef<WithWeak> safe2(with2.factory.GetSafeRef());
  EXPECT_EQ(&with1 <=> &with2, safe1 <=> safe2);
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

TEST(SafeRefTest, AssignCopyAndMove) {
  WithWeak with;
  WithWeak with2;
  WithWeak with3;

  // Ensure `with`s outlive `safe`s
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  SafeRef<WithWeak> safe2(with2.factory.GetSafeRef());
  EXPECT_NE(safe->self, &with2);
  safe = safe2;
  EXPECT_EQ(safe->self, &with2);

  SafeRef<WithWeak> safe3(with3.factory.GetSafeRef());
  EXPECT_NE(safe->self, &with3);
  safe = std::move(safe3);
  EXPECT_EQ(safe->self, &with3);
}

TEST(SafeRefDeathTest, ArrowOperatorCrashIfBadPointer) {
  std::optional<WithWeak> with(std::in_place);
  SafeRef<WithWeak> safe(with->factory.GetSafeRef());
  with.reset();
  EXPECT_CHECK_DEATH(safe.operator->());  // Will crash since not live.
}

TEST(SafeRefDeathTest, StarOperatorCrashIfBadPointer) {
  std::optional<WithWeak> with(std::in_place);
  SafeRef<WithWeak> safe(with->factory.GetSafeRef());
  with.reset();
  EXPECT_CHECK_DEATH(safe.operator*());  // Will crash since not live.
}

TEST(SafeRefTest, ConversionToBaseClassFromCopyConstruct) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  SafeRef<OtherBaseClass> base_safe = safe;
  EXPECT_EQ(static_cast<WithWeak*>(&*base_safe), &with);
}

TEST(SafeRefTest, ConversionToBaseClassFromMoveConstruct) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  SafeRef<OtherBaseClass> base_safe = std::move(safe);
  EXPECT_EQ(static_cast<WithWeak*>(&*base_safe), &with);
}

TEST(SafeRefTest, ConversionToBaseClassFromCopyAssign) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  SafeRef<OtherBaseClass> base_safe(with.factory.GetSafeRef());
  base_safe = safe;
  EXPECT_EQ(static_cast<WithWeak*>(&*base_safe), &with);
}

TEST(SafeRefTest, ConversionToBaseClassFromMoveAssign) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  SafeRef<OtherBaseClass> base_safe(with.factory.GetSafeRef());
  base_safe = std::move(safe);
  EXPECT_EQ(static_cast<WithWeak*>(&*base_safe), &with);
}

TEST(SafeRefTest, CanDerefConst) {
  WithWeak with;
  const SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  EXPECT_EQ(safe->self->i, 1);
  EXPECT_EQ((*safe).self->i, 1);
}

TEST(SafeRefTest, InvalidAfterMoveConstruction) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  SafeRef<WithWeak> safe2 = std::move(safe);
  // Will crash if not live.
  EXPECT_EQ(safe2->self->i, 1);
  // `safe` was previously moved-from, so using it in any way should crash now.
  { EXPECT_CHECK_DEATH(SafeRef<WithWeak> safe3(safe)); }
  {
    SafeRef<WithWeak> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = safe);
  }
  { EXPECT_CHECK_DEATH(SafeRef<WithWeak> safe3(std::move(safe))); }
  {
    SafeRef<WithWeak> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = std::move(safe));
  }
  { EXPECT_CHECK_DEATH(SafeRef<BaseClass> safe3(safe)); }
  {
    SafeRef<BaseClass> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = safe);
  }
  { EXPECT_CHECK_DEATH(SafeRef<BaseClass> safe3(std::move(safe))); }
  {
    SafeRef<BaseClass> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = std::move(safe));
  }
  EXPECT_CHECK_DEATH((void)safe->self->i);
}

TEST(SafeRefTest, InvalidAfterMoveAssignment) {
  WithWeak with;
  SafeRef<WithWeak> safe(with.factory.GetSafeRef());
  SafeRef<WithWeak> safe2(with.factory.GetSafeRef());
  safe2 = std::move(safe);
  // Will crash if not live.
  EXPECT_EQ(safe2->self->i, 1);
  // `safe` was previously moved-from, so using it in any way should crash now.
  { EXPECT_CHECK_DEATH(SafeRef<WithWeak> safe3(safe)); }
  {
    SafeRef<WithWeak> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = safe);
  }
  { EXPECT_CHECK_DEATH(SafeRef<WithWeak> safe3(std::move(safe))); }
  {
    SafeRef<WithWeak> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = std::move(safe));
  }
  { EXPECT_CHECK_DEATH(SafeRef<BaseClass> safe3(safe)); }
  {
    SafeRef<BaseClass> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = safe);
  }
  { EXPECT_CHECK_DEATH(SafeRef<BaseClass> safe3(std::move(safe))); }
  {
    SafeRef<BaseClass> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = std::move(safe));
  }
  EXPECT_CHECK_DEATH((void)safe->self->i);
}

TEST(SafeRefTest, InvalidAfterMoveConversionConstruction) {
  WithWeak with;
  SafeRef<BaseClass> safe(with.factory.GetSafeRef());
  SafeRef<BaseClass> safe2 = std::move(safe);
  // Will crash if not live.
  EXPECT_EQ(static_cast<WithWeak*>(&*safe2)->self->i, 1);
  // `safe` was previously moved-from, so using it in any way should crash now.
  { EXPECT_CHECK_DEATH(SafeRef<BaseClass> safe3(safe)); }
  {
    SafeRef<BaseClass> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = safe);
  }
  { EXPECT_CHECK_DEATH(SafeRef<BaseClass> safe3(std::move(safe))); }
  {
    SafeRef<BaseClass> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = std::move(safe));
  }
  { EXPECT_CHECK_DEATH(SafeRef<ReallyBaseClass> safe3(safe)); }
  {
    SafeRef<ReallyBaseClass> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = safe);
  }
  { EXPECT_CHECK_DEATH(SafeRef<ReallyBaseClass> safe3(std::move(safe))); }
  {
    SafeRef<ReallyBaseClass> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = std::move(safe));
  }
  EXPECT_CHECK_DEATH((void)static_cast<WithWeak*>(&*safe)->self->i);
}

TEST(SafeRefTest, InvalidAfterMoveConversionAssignment) {
  WithWeak with;
  SafeRef<BaseClass> safe(with.factory.GetSafeRef());
  SafeRef<BaseClass> safe2(with.factory.GetSafeRef());
  safe2 = std::move(safe);
  //  // Will crash if not live.
  EXPECT_EQ(static_cast<WithWeak*>(&*safe2)->self->i, 1);
  // `safe` was previously moved-from, so using it in any way should crash now.
  { EXPECT_CHECK_DEATH(SafeRef<BaseClass> safe3(safe)); }
  {
    SafeRef<BaseClass> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = safe);
  }
  { EXPECT_CHECK_DEATH(SafeRef<BaseClass> safe3(std::move(safe))); }
  {
    SafeRef<BaseClass> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = std::move(safe));
  }
  { EXPECT_CHECK_DEATH(SafeRef<ReallyBaseClass> safe3(safe)); }
  {
    SafeRef<ReallyBaseClass> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = safe);
  }
  { EXPECT_CHECK_DEATH(SafeRef<ReallyBaseClass> safe3(std::move(safe))); }
  {
    SafeRef<ReallyBaseClass> safe3(with.factory.GetSafeRef());
    EXPECT_CHECK_DEATH(safe3 = std::move(safe));
  }
  EXPECT_CHECK_DEATH((void)static_cast<WithWeak*>(&*safe)->self->i);
}

TEST(SafeRefTest, Bind) {
  WithWeak with;
  BindOnce(&WithWeak::Method, with.factory.GetSafeRef()).Run();
}

TEST(SafeRefTest, DanglingPointerDetector) {
  auto instrumentation = test::DanglingPtrInstrumentation::Create();
  if (!instrumentation.has_value()) {
    GTEST_SKIP() << instrumentation.error();
  }
  {
    auto with = std::make_unique<WithWeak>();
    SafeRef<WithWeak> safe(with->factory.GetSafeRef());
    EXPECT_EQ(instrumentation->dangling_ptr_detected(), 0u);
    EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);

    with.reset();
    EXPECT_EQ(instrumentation->dangling_ptr_detected(), 1u);
    EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);
  }
  EXPECT_EQ(instrumentation->dangling_ptr_detected(), 1u);
  EXPECT_EQ(instrumentation->dangling_ptr_released(), 1u);
}

TEST(SafeRefTest, DanglingUntriaged) {
  auto instrumentation = test::DanglingPtrInstrumentation::Create();
  if (!instrumentation.has_value()) {
    GTEST_SKIP() << instrumentation.error();
  }
  {
    auto with = std::make_unique<WithWeak>();
    SafeRef<WithWeak, SafeRefDanglingUntriaged> safe(
        with->factory.GetSafeRef());
    EXPECT_EQ(instrumentation->dangling_ptr_detected(), 0u);
    EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);

    with.reset();
    EXPECT_EQ(instrumentation->dangling_ptr_detected(), 0u);
    EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);
  }
  EXPECT_EQ(instrumentation->dangling_ptr_detected(), 0u);
  EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);
}

}  // namespace
}  // namespace base

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

#include <sanitizer/asan_interface.h>
#include <thread>

#include "base/debug/asan_service.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_asan_service.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::internal {

struct AsanStruct {
  int x;

  void func() { ++x; }
};

#define ASAN_BRP_PROTECTED(x) "MiraclePtr Status: PROTECTED\\n.*" x
#define ASAN_BRP_MANUAL_ANALYSIS(x) \
  "MiraclePtr Status: MANUAL ANALYSIS REQUIRED\\n.*" x
#define ASAN_BRP_NOT_PROTECTED(x) "MiraclePtr Status: NOT PROTECTED\\n.*" x

const char kAsanBrpProtected_Dereference[] =
    ASAN_BRP_PROTECTED("dangling pointer was being dereferenced");
const char kAsanBrpProtected_Callback[] = ASAN_BRP_PROTECTED(
    "crash occurred inside a callback where a raw_ptr<T> pointing to the same "
    "region");
const char kAsanBrpMaybeProtected_Extraction[] = ASAN_BRP_MANUAL_ANALYSIS(
    "pointer to the same region was extracted from a raw_ptr<T>");
const char kAsanBrpNotProtected_EarlyAllocation[] = ASAN_BRP_NOT_PROTECTED(
    "crash occurred while accessing a region that was allocated before "
    "MiraclePtr was activated");
const char kAsanBrpNotProtected_NoRawPtrAccess[] =
    ASAN_BRP_NOT_PROTECTED("No raw_ptr<T> access to this region was detected");
const char kAsanBrpMaybeProtected_Race[] =
    ASAN_BRP_MANUAL_ANALYSIS("\\nThe \"use\" and \"free\" threads don't match");
const char kAsanBrpMaybeProtected_ThreadPool[] =
    ASAN_BRP_MANUAL_ANALYSIS("\\nThis crash occurred in the thread pool");

// Instantiation failure message format is special.
const char kAsanBrp_Instantiation[] =
    "crash occurred due to an attempt to assign a dangling pointer";

#undef ASAN_BRP_PROTECTED
#undef ASAN_BRP_MANUAL_ANALYSIS
#undef ASAN_BRP_NOT_PROTECTED

class AsanBackupRefPtrTest : public testing::Test {
 protected:
  void SetUp() override {
    base::debug::AsanService::GetInstance()->Initialize();

    if (!RawPtrAsanService::GetInstance().IsEnabled()) {
      base::RawPtrAsanService::GetInstance().Configure(
          base::EnableDereferenceCheck(true), base::EnableExtractionCheck(true),
          base::EnableInstantiationCheck(true));
    } else {
      ASSERT_TRUE(base::RawPtrAsanService::GetInstance()
                      .is_dereference_check_enabled());
      ASSERT_TRUE(
          base::RawPtrAsanService::GetInstance().is_extraction_check_enabled());
      ASSERT_TRUE(base::RawPtrAsanService::GetInstance()
                      .is_instantiation_check_enabled());
    }
  }

  static void SetUpTestSuite() { early_allocation_ptr_ = new AsanStruct; }
  static void TearDownTestSuite() { delete early_allocation_ptr_; }
  static raw_ptr<AsanStruct> early_allocation_ptr_;
};

raw_ptr<AsanStruct> AsanBackupRefPtrTest::early_allocation_ptr_ = nullptr;

TEST_F(AsanBackupRefPtrTest, Dereference) {
  raw_ptr<AsanStruct> protected_ptr = new AsanStruct;

  // The four statements below should succeed.
  (*protected_ptr).x = 1;
  (*protected_ptr).func();
  ++(protected_ptr->x);
  protected_ptr->func();

  delete protected_ptr.get();

  EXPECT_DEATH_IF_SUPPORTED((*protected_ptr).x = 1,
                            kAsanBrpProtected_Dereference);
  EXPECT_DEATH_IF_SUPPORTED((*protected_ptr).func(),
                            kAsanBrpProtected_Dereference);
  EXPECT_DEATH_IF_SUPPORTED(++(protected_ptr->x),
                            kAsanBrpProtected_Dereference);
  EXPECT_DEATH_IF_SUPPORTED(protected_ptr->func(),
                            kAsanBrpProtected_Dereference);

  // The following statement should not trigger a dereference, so it should
  // succeed without crashing even though *protected_ptr is no longer valid.
  [[maybe_unused]] AsanStruct* ptr = protected_ptr;
}

TEST_F(AsanBackupRefPtrTest, Extraction) {
  raw_ptr<AsanStruct> protected_ptr = new AsanStruct;

  AsanStruct* ptr1 = protected_ptr;  // Shouldn't crash.
  ptr1->x = 0;

  delete protected_ptr.get();

  EXPECT_DEATH_IF_SUPPORTED(
      {
        AsanStruct* ptr2 = protected_ptr;
        ptr2->x = 1;
      },
      kAsanBrpMaybeProtected_Extraction);
}

TEST_F(AsanBackupRefPtrTest, Instantiation) {
  AsanStruct* ptr = new AsanStruct;

  raw_ptr<AsanStruct> protected_ptr1 = ptr;  // Shouldn't crash.
  protected_ptr1 = nullptr;

  delete ptr;

  EXPECT_DEATH_IF_SUPPORTED(
      { [[maybe_unused]] raw_ptr<AsanStruct> protected_ptr2 = ptr; },
      kAsanBrp_Instantiation);
}

TEST_F(AsanBackupRefPtrTest, InstantiationInvalidPointer) {
  void* ptr1 = reinterpret_cast<void*>(0xfefefefefefefefe);

  [[maybe_unused]] raw_ptr<void> protected_ptr1 = ptr1;  // Shouldn't crash.

  size_t shadow_scale, shadow_offset;
  __asan_get_shadow_mapping(&shadow_scale, &shadow_offset);
  [[maybe_unused]] raw_ptr<void> protected_ptr2 =
      reinterpret_cast<void*>(shadow_offset);  // Shouldn't crash.
}

TEST_F(AsanBackupRefPtrTest, UserPoisoned) {
  AsanStruct* ptr = new AsanStruct;
  __asan_poison_memory_region(ptr, sizeof(AsanStruct));

  [[maybe_unused]] raw_ptr<AsanStruct> protected_ptr1 =
      ptr;  // Shouldn't crash.

  delete ptr;  // Should crash now.
  EXPECT_DEATH_IF_SUPPORTED(
      { [[maybe_unused]] raw_ptr<AsanStruct> protected_ptr2 = ptr; },
      kAsanBrp_Instantiation);
}

TEST_F(AsanBackupRefPtrTest, EarlyAllocationDetection) {
  raw_ptr<AsanStruct> late_allocation_ptr = new AsanStruct;
  EXPECT_FALSE(RawPtrAsanService::GetInstance().IsSupportedAllocation(
      early_allocation_ptr_.get()));
  EXPECT_TRUE(RawPtrAsanService::GetInstance().IsSupportedAllocation(
      late_allocation_ptr.get()));

  delete late_allocation_ptr.get();
  delete early_allocation_ptr_.get();

  EXPECT_FALSE(RawPtrAsanService::GetInstance().IsSupportedAllocation(
      early_allocation_ptr_.get()));
  EXPECT_TRUE(RawPtrAsanService::GetInstance().IsSupportedAllocation(
      late_allocation_ptr.get()));

  EXPECT_DEATH_IF_SUPPORTED({ early_allocation_ptr_->func(); },
                            kAsanBrpNotProtected_EarlyAllocation);
  EXPECT_DEATH_IF_SUPPORTED({ late_allocation_ptr->func(); },
                            kAsanBrpProtected_Dereference);

  early_allocation_ptr_ = nullptr;
}

TEST_F(AsanBackupRefPtrTest, BoundRawPtr) {
  // This test is for the handling of raw_ptr<T> type objects being passed
  // directly to Bind.

  raw_ptr<AsanStruct> protected_ptr = new AsanStruct;

  // First create our test callbacks while `*protected_ptr` is still valid, and
  // we will then invoke them after deleting `*protected_ptr`.

  // `ptr` is protected in this callback, as raw_ptr<T> will be mapped to an
  // UnretainedWrapper containing a raw_ptr<T> which is guaranteed to outlive
  // the function call.
  auto ptr_callback = base::BindOnce(
      [](AsanStruct* ptr) {
        // This will crash and should be detected as a protected access.
        ptr->func();
      },
      protected_ptr);

  // Now delete `*protected_ptr` and check that the callbacks we created are
  // handled correctly.
  delete protected_ptr.get();
  protected_ptr = nullptr;

  EXPECT_DEATH_IF_SUPPORTED(std::move(ptr_callback).Run(),
                            kAsanBrpProtected_Callback);
}

TEST_F(AsanBackupRefPtrTest, BoundArgumentsProtected) {
  raw_ptr<AsanStruct> protected_ptr = new AsanStruct;
  raw_ptr<AsanStruct> protected_ptr2 = new AsanStruct;

  // First create our test callbacks while `*protected_ptr` is still valid, and
  // we will then invoke them after deleting `*protected_ptr`.

  // `ptr` is protected in this callback even after `*ptr` has been deleted,
  // since the allocation will be kept alive by the internal `raw_ptr<T>` inside
  // base::Unretained().
  auto safe_callback = base::BindOnce(
      [](AsanStruct* ptr) {
        // This will crash and should be detected as a protected access.
        ptr->func();
      },
      base::Unretained(protected_ptr));

  // Both `inner_ptr` and `outer_ptr` are protected in these callbacks, since
  // both are bound before `*ptr` is deleted. This test is making sure that
  // `inner_ptr` is treated as protected.
  auto safe_nested_inner_callback = base::BindOnce(
      [](AsanStruct* outer_ptr, base::OnceClosure inner_callback) {
        std::move(inner_callback).Run();
        // This will never be executed, as we will crash in inner_callback
        ASSERT_TRUE(false);
      },
      base::Unretained(protected_ptr),
      base::BindOnce(
          [](AsanStruct* inner_ptr) {
            // This will crash and should be detected as a protected access.
            inner_ptr->func();
          },
          base::Unretained(protected_ptr2)));

  // Both `inner_ptr` and `outer_ptr` are protected in these callbacks, since
  // both are bound before `*ptr` is deleted. This test is making sure that
  // `outer_ptr` is still treated as protected after `inner_callback` has run.
  auto safe_nested_outer_callback = base::BindOnce(
      [](AsanStruct* outer_ptr, base::OnceClosure inner_callback) {
        std::move(inner_callback).Run();
        // This will crash and should be detected as a protected access.
        outer_ptr->func();
      },
      base::Unretained(protected_ptr),
      base::BindOnce(
          [](AsanStruct* inner_ptr) {
            // Do nothing - we don't want to trip the protection inside the
            // inner callback.
          },
          base::Unretained(protected_ptr2)));

  // Now delete `*protected_ptr` and check that the callbacks we created are
  // handled correctly.
  delete protected_ptr.get();
  delete protected_ptr2.get();
  protected_ptr = nullptr;
  protected_ptr2 = nullptr;

  EXPECT_DEATH_IF_SUPPORTED(std::move(safe_callback).Run(),
                            kAsanBrpProtected_Callback);
  EXPECT_DEATH_IF_SUPPORTED(std::move(safe_nested_inner_callback).Run(),
                            kAsanBrpProtected_Callback);
  EXPECT_DEATH_IF_SUPPORTED(std::move(safe_nested_outer_callback).Run(),
                            kAsanBrpProtected_Callback);
}

TEST_F(AsanBackupRefPtrTest, BoundArgumentsNotProtected) {
  raw_ptr<AsanStruct> protected_ptr = new AsanStruct;

  // First create our test callbacks while `*protected_ptr` is still valid, and
  // we will then invoke them after deleting `*protected_ptr`.

  // `ptr` is not protected in this callback after `*ptr` has been deleted, as
  // integer-type bind arguments do not use an internal `raw_ptr<T>`.
  auto unsafe_callback = base::BindOnce(
      [](uintptr_t address) {
        AsanStruct* ptr = reinterpret_cast<AsanStruct*>(address);
        // This will crash and should not be detected as a protected access.
        ptr->func();
      },
      reinterpret_cast<uintptr_t>(protected_ptr.get()));

  // In this case, `outer_ptr` is protected in these callbacks, since it is
  // bound before `*ptr` is deleted. We want to make sure that the access to
  // `inner_ptr` is not automatically treated as protected (although it actually
  // is) because we're trying to limit the protection scope to be very
  // conservative here.
  auto unsafe_nested_inner_callback = base::BindOnce(
      [](AsanStruct* outer_ptr, base::OnceClosure inner_callback) {
        std::move(inner_callback).Run();
        // This will never be executed, as we will crash in inner_callback
        NOTREACHED();
      },
      base::Unretained(protected_ptr),
      base::BindOnce(
          [](uintptr_t inner_address) {
            AsanStruct* inner_ptr =
                reinterpret_cast<AsanStruct*>(inner_address);
            // This will crash and should be detected as maybe protected, since
            // it follows an extraction operation when the outer callback is
            // invoked
            inner_ptr->func();
          },
          reinterpret_cast<uintptr_t>(protected_ptr.get())));

  // In this case, `inner_ptr` is protected in these callbacks, since it is
  // bound before `*ptr` is deleted. We want to make sure that the access to
  // `outer_ptr` is not automatically treated as protected, since it isn't.
  auto unsafe_nested_outer_callback = base::BindOnce(
      [](uintptr_t outer_address, base::OnceClosure inner_callback) {
        { std::move(inner_callback).Run(); }
        AsanStruct* outer_ptr = reinterpret_cast<AsanStruct*>(outer_address);
        // This will crash and should be detected as maybe protected, since it
        // follows an extraction operation when the inner callback is invoked.
        outer_ptr->func();
      },
      reinterpret_cast<uintptr_t>(protected_ptr.get()),
      base::BindOnce(
          [](AsanStruct* inner_ptr) {
            // Do nothing - we don't want to trip the protection inside the
            // inner callback.
          },
          base::Unretained(protected_ptr)));

  // Now delete `*protected_ptr` and check that the callbacks we created are
  // handled correctly.
  delete protected_ptr.get();
  protected_ptr = nullptr;

  EXPECT_DEATH_IF_SUPPORTED(std::move(unsafe_callback).Run(),
                            kAsanBrpNotProtected_NoRawPtrAccess);
  EXPECT_DEATH_IF_SUPPORTED(std::move(unsafe_nested_inner_callback).Run(),
                            kAsanBrpMaybeProtected_Extraction);
  EXPECT_DEATH_IF_SUPPORTED(std::move(unsafe_nested_outer_callback).Run(),
                            kAsanBrpMaybeProtected_Extraction);
}

TEST_F(AsanBackupRefPtrTest, BoundArgumentsInstantiation) {
  // This test is ensuring that instantiations of `raw_ptr` inside callbacks are
  // handled correctly.

  raw_ptr<AsanStruct> protected_ptr = new AsanStruct;

  // First create our test callback while `*protected_ptr` is still valid.
  auto callback = base::BindRepeating(
      [](AsanStruct* ptr) {
        // This will crash if `*protected_ptr` is not valid.
        [[maybe_unused]] raw_ptr<AsanStruct> copy_ptr = ptr;
      },
      base::Unretained(protected_ptr));

  // It is allowed to create a new `raw_ptr<T>` inside a callback while
  // `*protected_ptr` is still valid.
  callback.Run();

  delete protected_ptr.get();
  protected_ptr = nullptr;

  // It is not allowed to create a new `raw_ptr<T>` inside a callback once
  // `*protected_ptr` is no longer valid.
  EXPECT_DEATH_IF_SUPPORTED(std::move(callback).Run(), kAsanBrp_Instantiation);
}

TEST_F(AsanBackupRefPtrTest, BoundReferences) {
  auto ptr = ::std::make_unique<AsanStruct>();

  // This test is ensuring that reference parameters inside callbacks are
  // handled correctly.

  // We should not crash during unwrapping a reference parameter if the
  // parameter is not accessed inside the callback.
  auto no_crash_callback = base::BindOnce(
      [](AsanStruct& ref) {
        // There should be no crash here as we don't access ref.
      },
      std::reference_wrapper(*ptr));

  // `ref` is protected in this callback even after `*ptr` has been deleted,
  // since the allocation will be kept alive by the internal `raw_ref<T>` inside
  // base::UnretainedRefWrapper().
  auto callback = base::BindOnce(
      [](AsanStruct& ref) {
        // This will crash and should be detected as protected
        ref.func();
      },
      std::reference_wrapper(*ptr));

  ptr.reset();

  std::move(no_crash_callback).Run();

  EXPECT_DEATH_IF_SUPPORTED(std::move(callback).Run(),
                            kAsanBrpProtected_Callback);
}

TEST_F(AsanBackupRefPtrTest, FreeOnAnotherThread) {
  auto ptr = ::std::make_unique<AsanStruct>();
  raw_ptr<AsanStruct> protected_ptr = ptr.get();

  std::thread thread([&ptr] { ptr.reset(); });
  thread.join();

  EXPECT_DEATH_IF_SUPPORTED(protected_ptr->func(), kAsanBrpMaybeProtected_Race);
}

TEST_F(AsanBackupRefPtrTest, AccessOnThreadPoolThread) {
  auto ptr = ::std::make_unique<AsanStruct>();
  raw_ptr<AsanStruct> protected_ptr = ptr.get();

  test::TaskEnvironment env;
  RunLoop run_loop;

  ThreadPool::PostTaskAndReply(
      FROM_HERE, {}, base::BindLambdaForTesting([&ptr, &protected_ptr] {
        ptr.reset();
        EXPECT_DEATH_IF_SUPPORTED(protected_ptr->func(),
                                  kAsanBrpMaybeProtected_ThreadPool);
      }),
      base::BindLambdaForTesting([&run_loop] { run_loop.Quit(); }));
  run_loop.Run();
}

TEST_F(AsanBackupRefPtrTest, DanglingUnretained) {
  // The test should finish without crashing.

  raw_ptr<AsanStruct> protected_ptr = new AsanStruct;
  delete protected_ptr.get();

  auto ptr_callback = base::BindOnce(
      [](AsanStruct* ptr) {
        // Do nothing - we only check the behavior of `BindOnce` in this test.
      },
      protected_ptr);
}

}  // namespace base::internal

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

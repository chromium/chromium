// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/trace_event/category_registry.h"
#include "base/trace_event/trace_category.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

// Static initializers are generally forbidden. However, in the past we ran in
// the case of some test using tracing in a static initializer. This test checks
// That the category registry doesn't rely on static initializers itself and is
// functional even if called from another static initializer.
bool Initializer() {
  return CategoryRegistry::kCategoryMetadata &&
         CategoryRegistry::kCategoryMetadata->is_valid();
}
bool g_initializer_check = Initializer();

class TraceCategoryTest : public testing::Test {
 public:
  void SetUp() override { CategoryRegistry::Initialize(); }

  void TearDown() override { CategoryRegistry::ResetForTesting(); }

  static bool GetOrCreateCategoryByName(const char* name, TraceCategory** cat) {
    static LazyInstance<Lock>::Leaky g_lock = LAZY_INSTANCE_INITIALIZER;
    bool is_new_cat = false;
    *cat = CategoryRegistry::GetCategoryByName(name);
    if (!*cat) {
      AutoLock lock(g_lock.Get());
      is_new_cat = CategoryRegistry::GetOrCreateCategoryLocked(
          name, [](TraceCategory*) {}, cat);
    }
    return is_new_cat;
  }

  static CategoryRegistry::Range GetAllCategories() {
    return CategoryRegistry::GetAllCategories();
  }

  static void TestRaceThreadMain(WaitableEvent* event) {
    TraceCategory* cat = nullptr;
    event->Wait();
    GetOrCreateCategoryByName("__test_race", &cat);
    EXPECT_NE(nullptr, cat);
  }

  static constexpr TraceCategory* GetBuiltinCategoryByName(
      const char* category_group) {
    return CategoryRegistry::GetBuiltinCategoryByName(category_group);
  }
};

TEST_F(TraceCategoryTest, Basic) {
  ASSERT_NE(nullptr, CategoryRegistry::kCategoryMetadata);
  ASSERT_TRUE(CategoryRegistry::kCategoryMetadata->is_valid());
  ASSERT_FALSE(CategoryRegistry::kCategoryMetadata->is_enabled());

  // Metadata category is built-in and should create a new category.
  TraceCategory* cat_meta = nullptr;
  const char* kMetadataName = CategoryRegistry::kCategoryMetadata->name();
  ASSERT_FALSE(GetOrCreateCategoryByName(kMetadataName, &cat_meta));
  ASSERT_EQ(CategoryRegistry::kCategoryMetadata, cat_meta);

  TraceCategory* cat_1 = nullptr;
  ASSERT_TRUE(GetOrCreateCategoryByName("__test_basic_ab", &cat_1));
  ASSERT_FALSE(cat_1->is_enabled());
  ASSERT_EQ(0u, cat_1->enabled_filters());
  cat_1->set_state_flag(TraceCategory::ENABLED_FOR_RECORDING);
  cat_1->set_state_flag(TraceCategory::ENABLED_FOR_FILTERING);
  ASSERT_EQ(TraceCategory::ENABLED_FOR_RECORDING |
                TraceCategory::ENABLED_FOR_FILTERING,
            cat_1->state());

  cat_1->set_enabled_filters(129);
  ASSERT_EQ(129u, cat_1->enabled_filters());
  ASSERT_EQ(cat_1, CategoryRegistry::GetCategoryByStatePtr(cat_1->state_ptr()));

  cat_1->clear_state_flag(TraceCategory::ENABLED_FOR_FILTERING);
  ASSERT_EQ(TraceCategory::ENABLED_FOR_RECORDING, cat_1->state());
  ASSERT_EQ(TraceCategory::ENABLED_FOR_RECORDING, *cat_1->state_ptr());
  ASSERT_TRUE(cat_1->is_enabled());

  TraceCategory* cat_2 = nullptr;
  ASSERT_TRUE(GetOrCreateCategoryByName("__test_basic_a", &cat_2));
  ASSERT_FALSE(cat_2->is_enabled());
  cat_2->set_state_flag(TraceCategory::ENABLED_FOR_RECORDING);

  TraceCategory* cat_2_copy = nullptr;
  ASSERT_FALSE(GetOrCreateCategoryByName("__test_basic_a", &cat_2_copy));
  ASSERT_EQ(cat_2, cat_2_copy);

  TraceCategory* cat_3 = nullptr;
  ASSERT_TRUE(
      GetOrCreateCategoryByName("__test_basic_ab,__test_basic_a", &cat_3));
  ASSERT_FALSE(cat_3->is_enabled());
  ASSERT_EQ(0u, cat_3->enabled_filters());

  int num_test_categories_seen = 0;
  for (const TraceCategory& cat : GetAllCategories()) {
    if (strcmp(cat.name(), kMetadataName) == 0)
      ASSERT_TRUE(CategoryRegistry::IsMetaCategory(&cat));

    if (strncmp(cat.name(), "__test_basic_", 13) == 0) {
      ASSERT_FALSE(CategoryRegistry::IsMetaCategory(&cat));
      num_test_categories_seen++;
    }
  }
  ASSERT_EQ(3, num_test_categories_seen);
  ASSERT_TRUE(g_initializer_check);
}

// Tries to cover the case of multiple threads creating the same category
// simultaneously. Should never end up with distinct entries with the same name.
#if defined(OS_FUCHSIA)
// TODO(crbug.com/738275): This is flaky on Fuchsia.
#define MAYBE_ThreadRaces DISABLED_ThreadRaces
#else
#define MAYBE_ThreadRaces ThreadRaces
#endif
TEST_F(TraceCategoryTest, MAYBE_ThreadRaces) {
  const int kNumThreads = 32;
  std::unique_ptr<Thread> threads[kNumThreads];
  for (int i = 0; i < kNumThreads; i++) {
    threads[i] = std::make_unique<Thread>("test thread");
    threads[i]->Start();
  }
  WaitableEvent sync_event(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  for (int i = 0; i < kNumThreads; i++) {
    threads[i]->task_runner()->PostTask(
        FROM_HERE, BindOnce(&TestRaceThreadMain, Unretained(&sync_event)));
  }
  sync_event.Signal();
  for (int i = 0; i < kNumThreads; i++)
    threads[i]->Stop();

  int num_times_seen = 0;
  for (const TraceCategory& cat : GetAllCategories()) {
    if (strcmp(cat.name(), "__test_race") == 0)
      num_times_seen++;
  }
  ASSERT_EQ(1, num_times_seen);
}

// Tests getting trace categories by name at compile-time.
TEST_F(TraceCategoryTest, GetCategoryAtCompileTime) {
  static_assert(GetBuiltinCategoryByName("nonexistent") == nullptr,
                "nonexistent found");
#if defined(OS_WIN) && defined(COMPONENT_BUILD)
  static_assert(GetBuiltinCategoryByName("toplevel") == nullptr,
                "toplevel found");
#else
  static_assert(GetBuiltinCategoryByName("toplevel") != nullptr,
                "toplevel not found");
#endif
}

}  // namespace trace_event
}  // namespace base

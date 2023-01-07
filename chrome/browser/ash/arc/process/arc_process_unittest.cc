// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/process/arc_process.h"

#include <assert.h>

#include <list>
#include <sstream>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/mojom/process.mojom.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

ArcProcess CreateFromPattern(const ArcProcess& pattern,
                             const std::string process_name) {
  return ArcProcess(pattern.nspid(), pattern.pid(), process_name,
                    pattern.process_state(), pattern.is_focused(),
                    pattern.last_activity_time());
}

// Tests that ArcProcess objects can be sorted by their priority (higher to
// lower). This is critical for the OOM handler to work correctly.
TEST(ArcProcess, TestSorting) {
  constexpr int64_t kNow = 1234567890;

  std::list<ArcProcess> processes;  // use list<> for emplace_front.
  processes.emplace_back(0, 0, "process 0", mojom::ProcessState::PERSISTENT,
                         false /* is_focused */, kNow + 1);
  processes.emplace_front(1, 1, "process 1", mojom::ProcessState::PERSISTENT,
                          false, kNow);
  processes.emplace_back(2, 2, "process 2", mojom::ProcessState::LAST_ACTIVITY,
                         false, kNow);
  processes.emplace_front(3, 3, "process 3", mojom::ProcessState::LAST_ACTIVITY,
                          false, kNow + 1);
  processes.emplace_back(4, 4, "process 4", mojom::ProcessState::CACHED_EMPTY,
                         false, kNow + 1);
  processes.emplace_front(5, 5, "process 5", mojom::ProcessState::CACHED_EMPTY,
                          false, kNow);
  processes.sort();

  static_assert(
      mojom::ProcessState::PERSISTENT < mojom::ProcessState::LAST_ACTIVITY,
      "unexpected enum values");
  static_assert(
      mojom::ProcessState::LAST_ACTIVITY < mojom::ProcessState::CACHED_EMPTY,
      "unexpected enum values");

  std::list<ArcProcess>::const_iterator it = processes.begin();
  // 0 should have higher priority since its last_activity_time is more recent.
  EXPECT_EQ(0, it->pid());
  ++it;
  EXPECT_EQ(1, it->pid());
  ++it;
  // Same, 3 should have higher priority.
  EXPECT_EQ(3, it->pid());
  ++it;
  EXPECT_EQ(2, it->pid());
  ++it;
  // Same, 4 should have higher priority.
  EXPECT_EQ(4, it->pid());
  ++it;
  EXPECT_EQ(5, it->pid());
}

TEST(ArcProcess, TestIsImportant) {
  constexpr bool kIsNotFocused = false;

  // Processes up to IMPORTANT_FOREGROUND are considered important.
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::PERSISTENT,
                         kIsNotFocused, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::PERSISTENT_UI,
                         kIsNotFocused, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::TOP, true, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::TOP, false, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessState::BOUND_FOREGROUND_SERVICE,
                         kIsNotFocused, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessState::FOREGROUND_SERVICE, kIsNotFocused,
                         0)
                  .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::TOP_SLEEPING,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessState::IMPORTANT_FOREGROUND,
                         kIsNotFocused, 0)
                  .IsImportant());

  // Others are not important.
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessState::IMPORTANT_BACKGROUND,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(
      ArcProcess(0, 0, "process", mojom::ProcessState::BACKUP, kIsNotFocused, 0)
          .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::HEAVY_WEIGHT,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::SERVICE,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::RECEIVER,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(
      ArcProcess(0, 0, "process", mojom::ProcessState::HOME, kIsNotFocused, 0)
          .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::LAST_ACTIVITY,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::CACHED_ACTIVITY,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessState::CACHED_ACTIVITY_CLIENT,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::CACHED_EMPTY,
                          kIsNotFocused, 0)
                   .IsImportant());

  // Custom ARC protected processes.
  EXPECT_TRUE(ArcProcess(0, 0, "com.google.android.apps.work.clouddpc.arc",
                         mojom::ProcessState::SERVICE, kIsNotFocused, 0)
                  .IsImportant());
}

TEST(ArcProcess, TestIsPersistent) {
  constexpr bool kIsNotFocused = false;

  // PERSISITENT* processes are persistent and should have lower oom_score_adj.
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::PERSISTENT,
                         kIsNotFocused, 0)
                  .IsPersistent());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::PERSISTENT_UI,
                         kIsNotFocused, 0)
                  .IsPersistent());

  // Both TOP+focused and TOP apps are not persistent.
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::TOP, true, 0)
                   .IsPersistent());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::TOP, false, 0)
                   .IsPersistent());

  // Others are not persistent.
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessState::BOUND_FOREGROUND_SERVICE,
                          kIsNotFocused, 0)
                   .IsPersistent());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessState::FOREGROUND_SERVICE,
                          kIsNotFocused, 0)
                   .IsPersistent());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::TOP_SLEEPING,
                          kIsNotFocused, 0)
                   .IsPersistent());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessState::IMPORTANT_FOREGROUND,
                          kIsNotFocused, 0)
                   .IsPersistent());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessState::IMPORTANT_BACKGROUND,
                          kIsNotFocused, 0)
                   .IsPersistent());
  EXPECT_FALSE(
      ArcProcess(0, 0, "process", mojom::ProcessState::BACKUP, kIsNotFocused, 0)
          .IsPersistent());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::HEAVY_WEIGHT,
                          kIsNotFocused, 0)
                   .IsPersistent());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::SERVICE,
                          kIsNotFocused, 0)
                   .IsPersistent());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::RECEIVER,
                          kIsNotFocused, 0)
                   .IsPersistent());
  EXPECT_FALSE(
      ArcProcess(0, 0, "process", mojom::ProcessState::HOME, kIsNotFocused, 0)
          .IsPersistent());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::LAST_ACTIVITY,
                          kIsNotFocused, 0)
                   .IsPersistent());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::CACHED_ACTIVITY,
                          kIsNotFocused, 0)
                   .IsPersistent());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessState::CACHED_ACTIVITY_CLIENT,
                          kIsNotFocused, 0)
                   .IsPersistent());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::CACHED_EMPTY,
                          kIsNotFocused, 0)
                   .IsPersistent());

  // Set of custom processes that are persistent.
  EXPECT_TRUE(ArcProcess(0, 0, "com.google.android.apps.work.clouddpc.arc",
                         mojom::ProcessState::SERVICE, kIsNotFocused, 0)
                  .IsPersistent());
}

// Tests operator<<() does not crash and returns non-empty result, at least.
TEST(ArcProcess, TestStringification) {
  std::stringstream s;
  s << ArcProcess(0, 0, "p", mojom::ProcessState::PERSISTENT, false, 0);
  EXPECT_FALSE(s.str().empty());
}

TEST(ArcProcess, GmsCoreProtection) {
  const ArcProcess pattern(0 /* nspid */, 0 /* pid */,
                           std::string() /* process_name */,
                           mojom::ProcessState::CACHED_EMPTY,
                           false /* is_focused */, 0 /* last_activity_time */);

  EXPECT_TRUE(CreateFromPattern(pattern, "com.google.process.gservices")
                  .IsPersistent());
  EXPECT_TRUE(
      CreateFromPattern(pattern, "com.google.process.gservices").IsImportant());
  EXPECT_TRUE(
      CreateFromPattern(pattern, "com.google.android.gms").IsPersistent());
  EXPECT_TRUE(
      CreateFromPattern(pattern, "com.google.android.gms").IsImportant());
  EXPECT_TRUE(CreateFromPattern(pattern, "com.google.android.gms.persistent")
                  .IsPersistent());
  EXPECT_TRUE(CreateFromPattern(pattern, "com.google.android.gms.persistent")
                  .IsImportant());
  // GMS UI is not protected.
  EXPECT_FALSE(
      CreateFromPattern(pattern, "com.google.android.gms.ui").IsPersistent());
  EXPECT_FALSE(
      CreateFromPattern(pattern, "com.google.android.gms.ui").IsImportant());
  EXPECT_TRUE(CreateFromPattern(pattern, "com.google.android.gms.unstable")
                  .IsPersistent());
  EXPECT_TRUE(CreateFromPattern(pattern, "com.google.android.gms.unstable")
                  .IsImportant());
}

}  // namespace

}  // namespace arc

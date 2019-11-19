// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/ukm_manager.h"

#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cc {
namespace {

const char kTestUrl[] = "https://example.com/foo";
const int64_t kTestSourceId1 = 100;
const int64_t kTestSourceId2 = 200;

const char kUserInteraction[] = "Compositor.UserInteraction";
const char kRendering[] = "Compositor.Rendering";

const char kCheckerboardArea[] = "CheckerboardedContentArea";
const char kCheckerboardAreaRatio[] = "CheckerboardedContentAreaRatio";
const char kMissingTiles[] = "NumMissingTiles";
const char kCheckerboardedImagesCount[] = "CheckerboardedImagesCount";

class UkmManagerTest : public testing::Test {
 public:
  UkmManagerTest() {
    auto recorder = std::make_unique<ukm::TestUkmRecorder>();
    test_ukm_recorder_ = recorder.get();
    manager_ = std::make_unique<UkmManager>(std::move(recorder));

    // In production, new UKM Source would have been already created, so
    // manager only needs to know the source id.
    test_ukm_recorder_->UpdateSourceURL(kTestSourceId1, GURL(kTestUrl));
    manager_->SetSourceId(kTestSourceId1);
  }

 protected:
  ukm::TestUkmRecorder* test_ukm_recorder_;
  std::unique_ptr<UkmManager> manager_;
};

TEST_F(UkmManagerTest, Basic) {
  manager_->SetUserInteractionInProgress(true);
  manager_->AddCheckerboardStatsForFrame(5, 1, 10);
  manager_->AddCheckerboardStatsForFrame(15, 3, 30);
  manager_->AddCheckerboardedImages(6);
  manager_->SetUserInteractionInProgress(false);

  // We should see a single entry for the interaction above.
  const auto& entries = test_ukm_recorder_->GetEntriesByName(kUserInteraction);
  ukm::SourceId original_id = ukm::kInvalidSourceId;
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    original_id = entry->source_id;
    EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestUrl));
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardArea, 10);
    test_ukm_recorder_->ExpectEntryMetric(entry, kMissingTiles, 2);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardAreaRatio, 50);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardedImagesCount, 6);
  }
  test_ukm_recorder_->Purge();

  // Try pushing some stats while no user interaction is happening. No entries
  // should be pushed.
  manager_->AddCheckerboardStatsForFrame(6, 1, 10);
  manager_->AddCheckerboardStatsForFrame(99, 3, 100);
  EXPECT_EQ(0u, test_ukm_recorder_->entries_count());
  manager_->SetUserInteractionInProgress(true);
  EXPECT_EQ(0u, test_ukm_recorder_->entries_count());

  // Record a few entries and change the source before the interaction ends. The
  // stats collected up till this point should be recorded before the source is
  // swapped.
  manager_->AddCheckerboardStatsForFrame(10, 1, 100);
  manager_->AddCheckerboardStatsForFrame(30, 5, 100);

  manager_->SetSourceId(kTestSourceId2);

  const auto& entries2 = test_ukm_recorder_->GetEntriesByName(kUserInteraction);
  EXPECT_EQ(1u, entries2.size());
  for (const auto* entry : entries2) {
    EXPECT_EQ(original_id, entry->source_id);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardArea, 20);
    test_ukm_recorder_->ExpectEntryMetric(entry, kMissingTiles, 3);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardAreaRatio, 20);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardedImagesCount, 0);
  }

  // An entry for rendering is emitted when the URL changes.
  const auto& entries_rendering =
      test_ukm_recorder_->GetEntriesByName(kRendering);
  EXPECT_EQ(1u, entries_rendering.size());
  for (const auto* entry : entries_rendering) {
    EXPECT_EQ(original_id, entry->source_id);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardedImagesCount, 6);
  }
}

}  // namespace
}  // namespace cc

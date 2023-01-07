// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_UKM_DATA_MANAGER_TEST_UTILS_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_UKM_DATA_MANAGER_TEST_UTILS_H_

#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/ukm/test_ukm_recorder.h"

class GURL;

namespace history {
class HistoryService;
}

namespace segmentation_platform {

// Utility used for testing UKM based engine.
class UkmDataManagerTestUtils {
 public:
  explicit UkmDataManagerTestUtils(ukm::TestUkmRecorder* ukm_recorder);
  ~UkmDataManagerTestUtils();

  UkmDataManagerTestUtils(const UkmDataManagerTestUtils&) = delete;
  UkmDataManagerTestUtils& operator=(const UkmDataManagerTestUtils&) = delete;

  // Must be called before the first profile initialization, sets up default
  // model overrides for the given `default_overrides`
  void PreProfileInit(const std::set<proto::SegmentId>& default_overrides);

  // Waits for platform to initialize and request default model for
  // `segment_id`, and then returns the provided `metadata` to the platform.
  void WaitForModelRequestAndUpdateWith(
      proto::SegmentId segment_id,
      const proto::SegmentationModelMetadata& metadata);

  // Creates a sample page load UKM based model metadata, with a simple SQL
  // feature with `query`.
  proto::SegmentationModelMetadata GetSamplePageLoadMetadata(
      const std::string& query);

  // Records a page load and 2 valid UKM metrics associated with it. May record
  // other UKM metrics that are unrelated to the metadata provided by
  // GetSamplePageLoadMetadata().
  void RecordPageLoadUkm(const GURL& url, base::Time history_timestamp);

  // Returns whether the `url` is part of the UKM database.
  bool IsUrlInDatabase(const GURL& url);

  // Returns the model provider override for the `segment_id`.
  MockModelProvider* GetDefaultOverride(proto::SegmentId segment_id);

  // History service is needed for validating test URLs written to database.
  void set_history_service(history::HistoryService* history_service) {
    history_service_ = history_service;
  }

 private:
  void StoreModelUpdateCallback(
      proto::SegmentId segment_id,
      const ModelProvider::ModelUpdatedCallback& callback);

  const raw_ptr<ukm::TestUkmRecorder> ukm_recorder_;
  int source_id_counter_ = 1;
  raw_ptr<history::HistoryService, DanglingUntriaged> history_service_;

  std::map<proto::SegmentId, MockModelProvider*> default_overrides_;
  std::map<proto::SegmentId, std::vector<ModelProvider::ModelUpdatedCallback>>
      callbacks_;

  base::WeakPtrFactory<UkmDataManagerTestUtils> weak_factory_{this};
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_UKM_DATA_MANAGER_TEST_UTILS_H_

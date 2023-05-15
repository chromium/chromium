// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_UKM_DATABASE_CLIENT_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_UKM_DATABASE_CLIENT_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"

namespace ukm {
class UkmRecorderImpl;
}

namespace segmentation_platform {
class UkmDataManager;
class UkmObserver;

// Provides UKM functionality to the segmentation platform service(s).
class UkmDatabaseClient {
 public:
  static UkmDatabaseClient& GetInstance();

  UkmDatabaseClient(const UkmDatabaseClient&) = delete;
  UkmDatabaseClient& operator=(const UkmDatabaseClient&) = delete;

  void PreProfileInit();

  void PostMessageLoopRun();

  // UkmDataManager will be valid for the lifetime of all the profiles. It is
  // created before profiles are created at startup. It is safe to use this
  // pointer till ProfileManagerDestroying() is called.
  segmentation_platform::UkmDataManager* GetUkmDataManager();

  // UKM observer will use the test recorder to observe metrics.
  void set_ukm_recorder_for_testing(ukm::UkmRecorderImpl* ukm_recorder) {
    DCHECK(!ukm_observer_);
    ukm_recorder_for_testing_ = ukm_recorder;
  }

  UkmObserver* ukm_observer_for_testing() { return ukm_observer_.get(); }

 private:
  friend base::NoDestructor<UkmDatabaseClient>;
  UkmDatabaseClient();
  ~UkmDatabaseClient();

  raw_ptr<ukm::UkmRecorderImpl, DanglingUntriaged> ukm_recorder_for_testing_;
  std::unique_ptr<UkmObserver> ukm_observer_;
  std::unique_ptr<segmentation_platform::UkmDataManager> ukm_data_manager_;
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_UKM_DATABASE_CLIENT_H_

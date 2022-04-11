// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_UKM_DATABASE_CLIENT_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_UKM_DATABASE_CLIENT_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/no_destructor.h"

namespace segmentation_platform {
class UkmDataManager;

// Provides UKM functionality to the segmentation platform service(s).
class UkmDatabaseClient {
 public:
  static UkmDatabaseClient& GetInstance();

  UkmDatabaseClient(UkmDatabaseClient&) = delete;
  UkmDatabaseClient& operator=(UkmDatabaseClient&) = delete;

  void PreProfileInit();

  void PostMessageLoopRun();

  // UkmDataManager will be valid for the lifetime of all the profiles. It is
  // created before profiles are created at startup. It is safe to use this
  // pointer till ProfileManagerDestroying() is called.
  segmentation_platform::UkmDataManager* GetUkmDataManager();

 private:
  friend base::NoDestructor<UkmDatabaseClient>;
  UkmDatabaseClient();
  ~UkmDatabaseClient();

  std::unique_ptr<segmentation_platform::UkmDataManager> ukm_data_manager_;
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_UKM_DATABASE_CLIENT_H_

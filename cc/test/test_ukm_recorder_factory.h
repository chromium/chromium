// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_UKM_RECORDER_FACTORY_H_
#define CC_TEST_TEST_UKM_RECORDER_FACTORY_H_

#include <memory>

#include "cc/metrics/ukm_manager.h"

namespace cc {

class TestUkmRecorderFactory : public UkmRecorderFactory {
 public:
  ~TestUkmRecorderFactory() override;

  std::unique_ptr<ukm::UkmRecorder> CreateRecorder() override;
};

}  // namespace cc

#endif  // CC_TEST_TEST_UKM_RECORDER_FACTORY_H_

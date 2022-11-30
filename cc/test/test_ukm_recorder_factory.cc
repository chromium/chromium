// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_ukm_recorder_factory.h"

#include "components/ukm/test_ukm_recorder.h"

namespace cc {

TestUkmRecorderFactory::~TestUkmRecorderFactory() = default;

std::unique_ptr<ukm::UkmRecorder> TestUkmRecorderFactory::CreateRecorder() {
  return std::make_unique<ukm::TestUkmRecorder>();
}

}  // namespace cc

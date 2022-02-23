// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TEST_ASH_COMPONENTS_TEST_SUITE_H_
#define ASH_COMPONENTS_TEST_ASH_COMPONENTS_TEST_SUITE_H_

#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite.h"
#include "ui/aura/env.h"

namespace ash {

class AshComponentsTestSuite : public base::TestSuite {
 public:
  AshComponentsTestSuite(int argc, char** argv);
  AshComponentsTestSuite(const AshComponentsTestSuite&) = delete;
  AshComponentsTestSuite& operator=(const AshComponentsTestSuite&) = delete;
  ~AshComponentsTestSuite() override;

 private:
  void Initialize() override;
  void Shutdown() override;

  void LoadTestResources();

  std::unique_ptr<aura::Env> env_;
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
};

}  // namespace ash

#endif  // ASH_COMPONENTS_TEST_ASH_COMPONENTS_TEST_SUITE_H_

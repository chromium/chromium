// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ASH_WEBUI_TEST_SUITE_H_
#define ASH_WEBUI_ASH_WEBUI_TEST_SUITE_H_

#include "base/files/scoped_temp_dir.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite.h"

class AshWebUITestSuite : public base::TestSuite {
 public:
  AshWebUITestSuite(int argc, char** argv);

  AshWebUITestSuite(const AshWebUITestSuite&) = delete;
  AshWebUITestSuite& operator=(const AshWebUITestSuite&) = delete;

  ~AshWebUITestSuite() override;

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

 private:
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
  base::ScopedTempDir user_data_dir_;
};

#endif  // ASH_WEBUI_ASH_WEBUI_TEST_SUITE_H_

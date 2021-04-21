// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTENT_ASH_CONTENT_TEST_SUITE_H_
#define ASH_CONTENT_ASH_CONTENT_TEST_SUITE_H_

#include "base/test/test_suite.h"

class AshContentTestSuite : public base::TestSuite {
 public:
  AshContentTestSuite(int argc, char** argv);

  AshContentTestSuite(const AshContentTestSuite&) = delete;
  AshContentTestSuite& operator=(const AshContentTestSuite&) = delete;

  ~AshContentTestSuite() override;

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;
};

#endif  // ASH_CONTENT_ASH_CONTENT_TEST_SUITE_H_

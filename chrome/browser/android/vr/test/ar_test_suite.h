// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_TEST_AR_TEST_SUITE_H_
#define CHROME_BROWSER_ANDROID_VR_TEST_AR_TEST_SUITE_H_

#include "base/test/test_suite.h"

namespace base {
namespace test {
class TaskEnvironment;
}  // namespace test
}  // namespace base

namespace vr {

class ArTestSuite : public base::TestSuite {
 public:
  ArTestSuite(int argc, char** argv);

  ArTestSuite(const ArTestSuite&) = delete;
  ArTestSuite& operator=(const ArTestSuite&) = delete;

  ~ArTestSuite() override;

 protected:
  void Initialize() override;
  void Shutdown() override;

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_TEST_AR_TEST_SUITE_H_

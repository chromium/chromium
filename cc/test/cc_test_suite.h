// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_CC_TEST_SUITE_H_
#define CC_TEST_CC_TEST_SUITE_H_

#include <memory>

#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite.h"

namespace base {
namespace test {
class TaskEnvironment;
}
}  // namespace base

namespace cc {

class CCTestSuite : public base::TestSuite {
 public:
  CCTestSuite(int argc, char** argv);
  CCTestSuite(const CCTestSuite&) = delete;
  ~CCTestSuite() override;

  CCTestSuite& operator=(const CCTestSuite&) = delete;

  static void RunUntilIdle();

 protected:
  // Overridden from base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

 private:
  static std::unique_ptr<base::test::TaskEnvironment> task_environment_;

  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
};

}  // namespace cc

#endif  // CC_TEST_CC_TEST_SUITE_H_

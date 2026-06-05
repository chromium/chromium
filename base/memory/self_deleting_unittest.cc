// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/self_deleting.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class TestSelfDeleter : public SelfDeleting {
 public:
  TestSelfDeleter(int v1, int v2, SelfDeletingPassKey key)
      : SelfDeleting(key), v1_(v1), v2_(v2) {}

  int DoWorkAndDeleteSelf() {
    int result = v1_ + v2_;
    delete this;
    return result;
  }

 private:
  ~TestSelfDeleter() = default;

  const int v1_;
  const int v2_;
};

TEST(SelfDeleting, CreateAndDelete) {
  auto* obj = MakeSelfDeleting<TestSelfDeleter>(1, 2);
  EXPECT_EQ(3, obj->DoWorkAndDeleteSelf());
}

}  // namespace base

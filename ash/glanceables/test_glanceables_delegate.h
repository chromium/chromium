// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TEST_GLANCEABLES_DELEGATE_H_
#define ASH_GLANCEABLES_TEST_GLANCEABLES_DELEGATE_H_

#include "ash/glanceables/glanceables_delegate.h"

namespace ash {

// A GlanceablesDelegate implementation for unit tests.
class TestGlanceablesDelegate : public GlanceablesDelegate {
 public:
  TestGlanceablesDelegate();
  TestGlanceablesDelegate(const TestGlanceablesDelegate&) = delete;
  TestGlanceablesDelegate& operator=(const TestGlanceablesDelegate&) = delete;
  ~TestGlanceablesDelegate() override;

  // GlanceablesDelegate:
  void OnGlanceablesClosed() override;

  int on_glanceables_closed_count() { return on_glanceables_closed_count_; }

 private:
  int on_glanceables_closed_count_ = 0;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TEST_GLANCEABLES_DELEGATE_H_

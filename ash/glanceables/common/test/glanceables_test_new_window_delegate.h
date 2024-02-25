// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_COMMON_TEST_GLANCEABLES_TEST_NEW_WINDOW_DELEGATE_H_
#define ASH_GLANCEABLES_COMMON_TEST_GLANCEABLES_TEST_NEW_WINDOW_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

class GURL;

namespace ash {

class GlanceablesTestNewWindowDelegateImpl;
class TestNewWindowDelegateProvider;

// Convenient tests helper to check the last opened URL via `NewWindowDelegate`.
class ASH_EXPORT GlanceablesTestNewWindowDelegate {
 public:
  GlanceablesTestNewWindowDelegate();
  GlanceablesTestNewWindowDelegate(const GlanceablesTestNewWindowDelegate&) =
      delete;
  GlanceablesTestNewWindowDelegate& operator=(
      const GlanceablesTestNewWindowDelegate&) = delete;
  ~GlanceablesTestNewWindowDelegate();

  GURL GetLastOpenedUrl() const;

 private:
  std::unique_ptr<TestNewWindowDelegateProvider> new_window_delegate_provider_;
  raw_ptr<GlanceablesTestNewWindowDelegateImpl> new_window_delegate_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_COMMON_TEST_GLANCEABLES_TEST_NEW_WINDOW_DELEGATE_H_

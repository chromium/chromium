// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/diagnostics/diagnostics_browser_delegate_impl.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {

class DiagnosticsBrowserDelegateImplTest : public testing::Test {
 public:
  DiagnosticsBrowserDelegateImplTest() = default;
  ~DiagnosticsBrowserDelegateImplTest() override = default;
};

TEST_F(DiagnosticsBrowserDelegateImplTest, GetActiveUserProfileDir) {
  DiagnosticsBrowserDelegateImpl delegate;
  EXPECT_EQ(base::FilePath(), delegate.GetActiveUserProfileDir());
}

}  // namespace diagnostics
}  // namespace ash

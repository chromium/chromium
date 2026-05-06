// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_keyed_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dictation {

class DictationKeyedServiceTest : public testing::Test {
 public:
  DictationKeyedServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(kDictation);
    // Passing nullptr for profile as it's not used in the methods we test.
    service_ = std::make_unique<DictationKeyedService>(nullptr);
  }
  ~DictationKeyedServiceTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<MockBrowserWindowInterface> window_;
  std::unique_ptr<DictationKeyedService> service_;
};

// Ending a non-existent session should not crash.
TEST_F(DictationKeyedServiceTest, EndSessionDoesNotCrash) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->EndSession();
}

TEST_F(DictationKeyedServiceTest, StartSessionWithNullTarget) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->StartSession(window_, nullptr);
  EXPECT_NE(service_->session_controller(), nullptr);
}

TEST_F(DictationKeyedServiceTest, EndSessionRemovesController) {
  service_->StartSession(window_, nullptr);
  ASSERT_NE(service_->session_controller(), nullptr);
  service_->EndSession();
  EXPECT_EQ(service_->session_controller(), nullptr);
}

}  // namespace dictation

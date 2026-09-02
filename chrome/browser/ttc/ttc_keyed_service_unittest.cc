// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/ttc_keyed_service.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ttc/features.h"
#include "chrome/browser/ttc/session_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

class TtcKeyedServiceUnitTest : public testing::Test {
 public:
  TtcKeyedServiceUnitTest() {
    scoped_feature_list_.InitAndEnableFeature(kTtc);
    service_ = std::make_unique<TtcKeyedService>(&profile_);
  }
  ~TtcKeyedServiceUnitTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TtcKeyedService> service_;
};

TEST_F(TtcKeyedServiceUnitTest, EndSessionDoesNotCrash) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->EndSession();
}

TEST_F(TtcKeyedServiceUnitTest, StartSession) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->StartSession();
  EXPECT_NE(service_->session_controller(), nullptr);
}

TEST_F(TtcKeyedServiceUnitTest, EndSessionRemovesController) {
  service_->StartSession();
  ASSERT_NE(service_->session_controller(), nullptr);
  service_->EndSession();
  EXPECT_EQ(service_->session_controller(), nullptr);
}

TEST_F(TtcKeyedServiceUnitTest, ShutdownRemovesController) {
  service_->StartSession();
  ASSERT_NE(service_->session_controller(), nullptr);
  service_->Shutdown();
  EXPECT_EQ(service_->session_controller(), nullptr);
}

}  // namespace ttc

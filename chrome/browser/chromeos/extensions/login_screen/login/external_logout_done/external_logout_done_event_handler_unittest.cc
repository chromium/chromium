// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/external_logout_done/external_logout_done_event_handler.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/api/login.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool WasEventDispatched(const std::string& event_name,
                        const extensions::TestEventRouterObserver* observer) {
  return base::Contains(observer->events(), event_name);
}

}  // namespace

namespace extensions {

class ExternalLogoutDoneEventHandlerUnittest : public testing::Test {
 public:
  ExternalLogoutDoneEventHandlerUnittest() {}

  ExternalLogoutDoneEventHandlerUnittest(
      const ExternalLogoutDoneEventHandlerUnittest&) = delete;
  ExternalLogoutDoneEventHandlerUnittest& operator=(
      const ExternalLogoutDoneEventHandlerUnittest&) = delete;

  ~ExternalLogoutDoneEventHandlerUnittest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ =
        profile_manager_->CreateTestingProfile(chrome::kInitialProfile);
    event_router_ = std::make_unique<EventRouter>(testing_profile_, nullptr);
    external_logout_done_event_handler_ =
        std::make_unique<ExternalLogoutDoneEventHandler>(testing_profile_);
    external_logout_done_event_handler_->SetEventRouterForTesting(
        event_router_.get());
    observer_ = std::make_unique<TestEventRouterObserver>(event_router_.get());
  }

  void TearDown() override {
    observer_.reset();
    event_router_.reset();
    testing_profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<ExternalLogoutDoneEventHandler>
      external_logout_done_event_handler_;
  std::unique_ptr<EventRouter> event_router_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<TestEventRouterObserver> observer_;
};

TEST_F(ExternalLogoutDoneEventHandlerUnittest,
       OnExternalLogoutDoneDispatchesEvent) {
  external_logout_done_event_handler_->OnExternalLogoutDone();
  // `onExternalLogoutDone` event is dispatched.
  EXPECT_TRUE(WasEventDispatched(api::login::OnExternalLogoutDone::kEventName,
                                 observer_.get()));
}

}  // namespace extensions

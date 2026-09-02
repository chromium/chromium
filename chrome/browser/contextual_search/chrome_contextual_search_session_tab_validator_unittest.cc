// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_search/chrome_contextual_search_session_tab_validator.h"

#include "chrome/test/base/testing_profile.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestTabInterface : public tabs::MockTabInterface {
 public:
  TestTabInterface() = default;
  ~TestTabInterface() override = default;

  void SetSessionIdForTest(SessionID session_id) {
    SetSessionId(session_id.id());
  }
};

class ChromeContextualSearchSessionTabValidatorTest : public testing::Test {
 protected:
  ChromeContextualSearchSessionTabValidatorTest() = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    validator_ = std::make_unique<ChromeContextualSearchSessionTabValidator>(
        profile_.get());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ChromeContextualSearchSessionTabValidator> validator_;
};

TEST_F(ChromeContextualSearchSessionTabValidatorTest, InvalidFileInfo) {
  contextual_search::FileInfo file_info;
  // Missing session id and url.
  EXPECT_FALSE(validator_->IsTabValidAndPointingToUrl(file_info));

  file_info.tab_session_id = SessionID::FromSerializedValue(1);
  // Missing url.
  EXPECT_FALSE(validator_->IsTabValidAndPointingToUrl(file_info));

  file_info.tab_session_id = std::nullopt;
  file_info.tab_url = GURL("https://example.com");
  // Missing session id.
  EXPECT_FALSE(validator_->IsTabValidAndPointingToUrl(file_info));
}

TEST_F(ChromeContextualSearchSessionTabValidatorTest, TabClosed) {
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = SessionID::FromSerializedValue(1);
  file_info.tab_url = GURL("https://example.com");

  // Tab with session id 1 is not registered in the factory.
  EXPECT_FALSE(validator_->IsTabValidAndPointingToUrl(file_info));
}

TEST_F(ChromeContextualSearchSessionTabValidatorTest, ProfileMismatch) {
  auto test_tab = std::make_unique<TestTabInterface>();
  SessionID session_id = SessionID::NewUnique();
  test_tab->SetSessionIdForTest(session_id);

  // Set up tab mock to return a different profile.
  std::unique_ptr<TestingProfile> other_profile =
      std::make_unique<TestingProfile>();
  EXPECT_CALL(*test_tab, GetProfile())
      .WillRepeatedly(testing::Return(other_profile.get()));

  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.tab_url = GURL("https://example.com");

  EXPECT_FALSE(validator_->IsTabValidAndPointingToUrl(file_info));
}

TEST_F(ChromeContextualSearchSessionTabValidatorTest, ValidTabAndUrlMatch) {
  auto test_tab = std::make_unique<TestTabInterface>();
  SessionID session_id = SessionID::NewUnique();
  test_tab->SetSessionIdForTest(session_id);

  EXPECT_CALL(*test_tab, GetProfile())
      .WillRepeatedly(testing::Return(profile_.get()));
  EXPECT_CALL(*test_tab, GetURL())
      .WillRepeatedly(testing::Return(GURL("https://example.com")));
  EXPECT_CALL(*test_tab, GetTitle())
      .WillRepeatedly(testing::Return(u"Example"));

  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.tab_url = GURL("https://example.com");
  file_info.tab_title = "Example";

  EXPECT_TRUE(validator_->IsTabValidAndPointingToUrl(file_info));
}

TEST_F(ChromeContextualSearchSessionTabValidatorTest, UrlMismatch) {
  auto test_tab = std::make_unique<TestTabInterface>();
  SessionID session_id = SessionID::NewUnique();
  test_tab->SetSessionIdForTest(session_id);

  EXPECT_CALL(*test_tab, GetProfile())
      .WillRepeatedly(testing::Return(profile_.get()));
  EXPECT_CALL(*test_tab, GetURL())
      .WillRepeatedly(testing::Return(GURL("https://example.com")));
  EXPECT_CALL(*test_tab, GetTitle())
      .WillRepeatedly(testing::Return(u"Example"));

  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.tab_url = GURL("https://different.com"); // Mismatch
  file_info.tab_title = "Example";

  EXPECT_FALSE(validator_->IsTabValidAndPointingToUrl(file_info));
}

}  // namespace

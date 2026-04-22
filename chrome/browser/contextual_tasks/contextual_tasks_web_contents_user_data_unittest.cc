// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"

#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

class ContextualTasksWebContentsUserDataTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();
    web_contents_ = web_contents_factory_->CreateWebContents(profile_.get());
  }

  void TearDown() override {
    web_contents_ = nullptr;
    web_contents_factory_.reset();
    profile_.reset();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
};

TEST_F(ContextualTasksWebContentsUserDataTest, GetOrCreate) {
  // Initially null.
  EXPECT_FALSE(
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_));

  // Create it.
  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  EXPECT_TRUE(user_data);

  // Get it again.
  auto* user_data2 =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  EXPECT_EQ(user_data, user_data2);
}

TEST_F(ContextualTasksWebContentsUserDataTest, SetAndGetModel) {
  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);

  EXPECT_FALSE(user_data->input_state_model());

  auto mock_handle =
      std::make_shared<contextual_search::MockContextualSearchSessionHandle>();
  omnibox::SearchboxConfig config;
  auto input_state_model = std::make_unique<contextual_search::InputStateModel>(
      *mock_handle, config, GURL(), false, false);

  auto weak_ptr = input_state_model->AsWeakPtr();

  user_data->set_input_state_model(std::move(input_state_model));
  EXPECT_EQ(user_data->input_state_model().get(), weak_ptr.get());
}

}  // namespace contextual_tasks

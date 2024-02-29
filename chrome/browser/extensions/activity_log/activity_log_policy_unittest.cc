// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/values.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/extensions/activity_log/activity_log_policy.h"
#include "extensions/common/api/web_request/web_request_activity_log_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ActivityLogPolicyUtilTest : public testing::Test {};

// Test that incognito values are stripped, and non-incognito ones aren't.
TEST_F(ActivityLogPolicyUtilTest, StripPrivacySensitive) {
  scoped_refptr<Action> action =
      new Action("punky",
                 base::Time::Now(),
                 Action::ACTION_API_CALL,
                 "tabs.executeScript");
  action->mutable_args().Append("woof");
  action->set_page_url(GURL("http://www.google.com/"));
  action->set_page_incognito(true);
  action->set_page_title("private");
  action->set_arg_url(GURL("http://www.youtube.com/?privatekey"));

  ASSERT_EQ("<incognito>http://www.google.com/", action->SerializePageUrl());

  ActivityLogPolicy::Util::StripPrivacySensitiveFields(action);

  ASSERT_FALSE(action->page_url().is_valid());
  ASSERT_EQ("", action->SerializePageUrl());
  ASSERT_EQ("", action->page_title());
  ASSERT_EQ("http://www.youtube.com/", action->arg_url().spec());
}

// Test that WebRequest details are stripped for privacy.
TEST_F(ActivityLogPolicyUtilTest, StripPrivacySensitiveWebRequest) {
  scoped_refptr<Action> action = new Action(
      "punky", base::Time::Now(), Action::ACTION_WEB_REQUEST, "webRequest");
  base::Value::Dict root;
  root.Set(web_request_activity_log_constants::kNewUrlKey,
           "http://www.youtube.com/");
  root.Set(web_request_activity_log_constants::kAddedRequestHeadersKey,
           base::Value::List());
  action->mutable_other().Set(activity_log_constants::kActionWebRequest,
                              std::move(root));

  ActivityLogPolicy::Util::StripPrivacySensitiveFields(action);

  ASSERT_EQ(
      "{\"web_request\":{\"added_request_headers\":true,\"new_url\":true}}",
      ActivityLogPolicy::Util::Serialize(action->other()));
}

// Test that argument values are stripped as appropriate.
TEST_F(ActivityLogPolicyUtilTest, StripArguments) {
  ActivityLogPolicy::Util::ApiSet allowlist;
  allowlist.insert(
      std::make_pair(Action::ACTION_API_CALL, "tabs.executeScript"));

  // API is in allowlist; not stripped.
  scoped_refptr<Action> action =
      new Action("punky",
                 base::Time::Now(),
                 Action::ACTION_API_CALL,
                 "tabs.executeScript");
  action->mutable_args().Append("woof");
  ActivityLogPolicy::Util::StripArguments(allowlist, action);
  ASSERT_EQ("[\"woof\"]", ActivityLogPolicy::Util::Serialize(action->args()));

  // Not in allowlist: stripped.
  action = new Action(
      "punky", base::Time::Now(), Action::ACTION_API_CALL, "tabs.create");
  action->mutable_args().Append("woof");
  ActivityLogPolicy::Util::StripArguments(allowlist, action);
  ASSERT_EQ("", ActivityLogPolicy::Util::Serialize(action->args()));
}

// Test parsing of URLs serialized to strings.
TEST_F(ActivityLogPolicyUtilTest, ParseUrls) {
  scoped_refptr<Action> action =
      new Action("punky",
                 base::Time::Now(),
                 Action::ACTION_API_CALL,
                 "tabs.executeScript");
  action->ParsePageUrl("<incognito>http://www.google.com/");
  EXPECT_EQ("http://www.google.com/", action->page_url().spec());
  EXPECT_TRUE(action->page_incognito());
}

}  // namespace extensions

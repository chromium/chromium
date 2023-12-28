// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kExtensionId[] = "extensionid";
const char kApiCall[] = "api.call";
const char kArgs[] = "[\"hello\",\"world\"]";

}  // extensions

namespace extensions {

using api::activity_log_private::ExtensionActivity;

using ActivityLogApiUnitTest = testing::Test;

TEST_F(ActivityLogApiUnitTest, ConvertChromeApiAction) {
  base::Value::List args;
  args.Append("hello");
  args.Append("world");
  scoped_refptr<Action> action(new Action(kExtensionId,
                                          base::Time::Now(),
                                          Action::ACTION_API_CALL,
                                          kApiCall));
  action->set_args(std::move(args));
  ExtensionActivity result = action->ConvertToExtensionActivity();
  ASSERT_EQ(api::activity_log_private::ExtensionActivityType::kApiCall,
            result.activity_type);
  ASSERT_EQ(kExtensionId, *result.extension_id);
  ASSERT_EQ(kApiCall, *result.api_call);
  ASSERT_EQ(kArgs, *result.args);
  EXPECT_FALSE(result.activity_id);
}

TEST_F(ActivityLogApiUnitTest, ConvertDomAction) {
  base::Value::List args;
  args.Append("hello");
  args.Append("world");
  scoped_refptr<Action> action(new Action(kExtensionId,
                               base::Time::Now(),
                               Action::ACTION_DOM_ACCESS,
                               kApiCall,
                               12345));
  action->set_args(std::move(args));
  action->set_page_url(GURL("http://www.google.com"));
  action->set_page_title("Title");
  action->mutable_other().Set(activity_log_constants::kActionDomVerb,
                              DomActionType::INSERTED);
  action->mutable_other().Set(activity_log_constants::kActionPrerender, false);
  ExtensionActivity result = action->ConvertToExtensionActivity();
  ASSERT_EQ(kExtensionId, *result.extension_id);
  ASSERT_EQ("http://www.google.com/", *result.page_url);
  ASSERT_EQ("Title", *result.page_title);
  ASSERT_EQ(kApiCall, *result.api_call);
  ASSERT_EQ(kArgs, *result.args);
  auto other = std::move(result.other);
  ASSERT_EQ(api::activity_log_private::ExtensionActivityDomVerb::kInserted,
            other->dom_verb);
  ASSERT_TRUE(other->prerender);
  ASSERT_EQ("12345", *result.activity_id);
}

}  // namespace extensions

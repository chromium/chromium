// Copyright 2013 The Chromium Authors. All rights reserved.
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

typedef testing::Test ActivityLogApiUnitTest;

TEST_F(ActivityLogApiUnitTest, ConvertChromeApiAction) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append("hello");
  args->Append("world");
  scoped_refptr<Action> action(new Action(kExtensionId,
                                          base::Time::Now(),
                                          Action::ACTION_API_CALL,
                                          kApiCall));
  action->set_args(std::move(args));
  ExtensionActivity result = action->ConvertToExtensionActivity();
  ASSERT_EQ(api::activity_log_private::EXTENSION_ACTIVITY_TYPE_API_CALL,
            result.activity_type);
  ASSERT_EQ(kExtensionId, *(result.extension_id.get()));
  ASSERT_EQ(kApiCall, *(result.api_call.get()));
  ASSERT_EQ(kArgs, *(result.args.get()));
  ASSERT_EQ(NULL, result.activity_id.get());
}

TEST_F(ActivityLogApiUnitTest, ConvertDomAction) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append("hello");
  args->Append("world");
  scoped_refptr<Action> action(new Action(kExtensionId,
                               base::Time::Now(),
                               Action::ACTION_DOM_ACCESS,
                               kApiCall,
                               12345));
  action->set_args(std::move(args));
  action->set_page_url(GURL("http://www.google.com"));
  action->set_page_title("Title");
  action->mutable_other()->SetIntKey(activity_log_constants::kActionDomVerb,
                                     DomActionType::INSERTED);
  action->mutable_other()->SetBoolKey(activity_log_constants::kActionPrerender,
                                      false);
  ExtensionActivity result = action->ConvertToExtensionActivity();
  ASSERT_EQ(kExtensionId, *(result.extension_id.get()));
  ASSERT_EQ("http://www.google.com/", *(result.page_url.get()));
  ASSERT_EQ("Title", *(result.page_title.get()));
  ASSERT_EQ(kApiCall, *(result.api_call.get()));
  ASSERT_EQ(kArgs, *(result.args.get()));
  std::unique_ptr<ExtensionActivity::Other> other(std::move(result.other));
  ASSERT_EQ(api::activity_log_private::EXTENSION_ACTIVITY_DOM_VERB_INSERTED,
            other->dom_verb);
  ASSERT_TRUE(other->prerender.get());
  ASSERT_EQ("12345", *(result.activity_id.get()));
}

}  // namespace extensions

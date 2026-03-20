// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/activity_log_ingester.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/safe_browsing/extension_telemetry/dom_access_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/script_injection_signal.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/dom_action_types.h"
#include "extensions/common/extension_features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

class MockExtensionTelemetryService : public ExtensionTelemetryService {
 public:
  explicit MockExtensionTelemetryService(Profile* profile)
      : ExtensionTelemetryService(profile, nullptr) {}
  ~MockExtensionTelemetryService() override = default;

  MOCK_METHOD(void,
              AddSignal,
              (std::unique_ptr<ExtensionSignal> signal),
              (override));
};

}  // namespace

class ActivityLogIngesterTest : public testing::Test {
 protected:
  ActivityLogIngesterTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kEnterpriseExtensionDOMActivityTelemetry);
  }
  ~ActivityLogIngesterTest() override = default;

  void SetUp() override {
    telemetry_service_ =
        std::make_unique<testing::StrictMock<MockExtensionTelemetryService>>(
            &profile_);
    ingester_ = std::make_unique<ActivityLogIngester>(&profile_,
                                                      telemetry_service_.get());
  }

  void TearDown() override {
    ingester_.reset();
    telemetry_service_.reset();
  }

  scoped_refptr<extensions::Action> CreateAction(
      extensions::Action::ActionType action_type,
      const std::string& api_name,
      base::ListValue args,
      std::optional<int> dom_verb = std::nullopt) {
    auto action = base::MakeRefCounted<extensions::Action>(
        "ext-1", base::Time::Now(), action_type, api_name);
    action->set_args(std::move(args));
    action->set_page_url(GURL("http://www.example.com"));

    if (dom_verb) {
      action->mutable_other().Set(activity_log_constants::kActionDomVerb,
                                  *dom_verb);
    }
    return action;
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<MockExtensionTelemetryService> telemetry_service_;
  std::unique_ptr<ActivityLogIngester> ingester_;
};

TEST_F(ActivityLogIngesterTest, RegistersWithActivityLog) {
  auto* activity_log = extensions::ActivityLog::GetInstance(&profile_);
  EXPECT_TRUE(activity_log->IsTelemetryLoggingActive());

  ingester_.reset();
  EXPECT_FALSE(activity_log->IsTelemetryLoggingActive());
}

TEST_F(ActivityLogIngesterTest, HandlesMissingArguments) {
  // Create an action without arguments.
  auto action = base::MakeRefCounted<extensions::Action>(
      "ext-1", base::Time::Now(), extensions::Action::ACTION_DOM_ACCESS,
      "Document.cookie");
  action->set_page_url(GURL("http://www.example.com"));
  ASSERT_FALSE(action->args().has_value());

  // Should not crash and should correctly identify the signal.
  EXPECT_CALL(*telemetry_service_, AddSignal(testing::_))
      .WillOnce([](std::unique_ptr<ExtensionSignal> signal) {
        ASSERT_EQ(signal->GetType(), ExtensionSignalType::kDOMAccess);
      });

  ingester_->OnExtensionActivity(action);
}

TEST_F(ActivityLogIngesterTest, IngestsDOMAccessSignal_Read) {
  base::ListValue args;
  auto action =
      CreateAction(extensions::Action::ACTION_DOM_ACCESS, "Document.cookie",
                   std::move(args), extensions::DomActionType::GETTER);

  EXPECT_CALL(*telemetry_service_, AddSignal(testing::_))
      .WillOnce([](std::unique_ptr<ExtensionSignal> signal) {
        ASSERT_EQ(signal->GetType(), ExtensionSignalType::kDOMAccess);
        auto* dom_signal = static_cast<DOMAccessSignal*>(signal.get());
        EXPECT_EQ(dom_signal->extension_id(), "ext-1");
        EXPECT_EQ(dom_signal->api_name(), "Document.cookie");
        EXPECT_EQ(dom_signal->access_type(), DOMAccessSignal::DOMAccess::READ);
      });

  ingester_->OnExtensionActivity(action);
}

TEST_F(ActivityLogIngesterTest, IngestsScriptInjectionSignal_ExecuteScript) {
  base::ListValue args;
  args.Append("code to inject");
  auto action = CreateAction(extensions::Action::ACTION_API_CALL,
                             "scripting.executeScript", std::move(args));
  action->set_arg_url(GURL("http://www.target.com/path/to/page.html"));

  EXPECT_CALL(*telemetry_service_, AddSignal(testing::_))
      .WillOnce([](std::unique_ptr<ExtensionSignal> signal) {
        ASSERT_EQ(signal->GetType(), ExtensionSignalType::kScriptInjection);
        auto* script_signal = static_cast<ScriptInjectionSignal*>(signal.get());
        EXPECT_EQ(script_signal->extension_id(), "ext-1");
        EXPECT_EQ(script_signal->api_name(), "scripting.executeScript");
        ASSERT_EQ(script_signal->args_list().size(), 1u);
        EXPECT_EQ(script_signal->args_list()[0], "code to inject");
        // Target URL should have been moved from arg_url to url and sanitized.
        EXPECT_EQ(script_signal->url(), "http://www.target.com/path/to/");
      });

  ingester_->OnExtensionActivity(action);
}

TEST_F(ActivityLogIngesterTest, IngestsScriptInjectionSignal_DOMInjection) {
  base::ListValue args;
  args.Append("script");
  auto action =
      CreateAction(extensions::Action::ACTION_DOM_ACCESS, "blinkAddElement",
                   std::move(args), extensions::DomActionType::METHOD);

  EXPECT_CALL(*telemetry_service_, AddSignal(testing::_))
      .WillOnce([](std::unique_ptr<ExtensionSignal> signal) {
        ASSERT_EQ(signal->GetType(), ExtensionSignalType::kScriptInjection);
        auto* script_signal = static_cast<ScriptInjectionSignal*>(signal.get());
        EXPECT_EQ(script_signal->extension_id(), "ext-1");
        EXPECT_EQ(script_signal->api_name(), "blinkAddElement");
        ASSERT_EQ(script_signal->args_list().size(), 1u);
        EXPECT_EQ(script_signal->args_list()[0], "script");
      });

  ingester_->OnExtensionActivity(action);
}

TEST_F(ActivityLogIngesterTest, IgnoresBenignAction) {
  base::ListValue args;
  auto action = CreateAction(extensions::Action::ACTION_API_CALL, "tabs.create",
                             std::move(args));

  // StrictMock ensures no calls are made to AddSignal.
  ingester_->OnExtensionActivity(action);
}

}  // namespace safe_browsing

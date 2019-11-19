// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_launch_from_native.h"

#include <memory>
#include <utility>

#include "base/path_service.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class MockEventRouter : public EventRouter {
 public:
  MockEventRouter(content::BrowserContext* browser_context,
                  ExtensionPrefs* extension_prefs,
                  const bool* has_listener_result)
      : EventRouter(browser_context, extension_prefs),
        has_listener_result_(has_listener_result) {
    DCHECK(has_listener_result_);
  }

  bool ExtensionHasEventListener(const std::string& extension_id,
                                 const std::string& event_name) const override {
    return *has_listener_result_;
  }

 private:
  const bool* has_listener_result_;
};

std::unique_ptr<KeyedService> BuildMockEventRouter(
    const bool* has_listener_result,
    content::BrowserContext* context) {
  return std::make_unique<MockEventRouter>(
      context, ExtensionPrefs::Get(context), has_listener_result);
}

class ExtensionSupportsConnectionFromNativeAppTest : public ::testing::Test {
 public:
  ExtensionSupportsConnectionFromNativeAppTest()
      : channel_(version_info::Channel::DEV) {}

  void SetUp() override {
    EventRouterFactory::GetInstance()->SetTestingFactory(
        &profile_,
        base::BindRepeating(&BuildMockEventRouter, &has_listener_result_));
  }

 protected:
  void RegisterExtension(bool natively_connectable,
                         bool transient_background_permission,
                         bool native_messaging_permission) {
    DictionaryBuilder manifest_builder(
        static_cast<base::DictionaryValue&&>(base::test::ParseJson(R"(
            {
              "version": "1.0.0.0",
              "manifest_version": 2,
              "name": "native messaging test",
              "description": "native messaging test",
              "background": {
                "scripts": ["test.js"],
                "persistent": false
              }
            }
    )")));

    if (natively_connectable) {
      ListBuilder natively_connectable_hosts;
      natively_connectable_hosts.Append(
          ScopedTestNativeMessagingHost::kHostName);

      natively_connectable_hosts.Append(
          ScopedTestNativeMessagingHost::
              kSupportsNativeInitiatedConnectionsHostName);
      manifest_builder.Set(manifest_keys::kNativelyConnectable,
                           natively_connectable_hosts.Build());
    }

    ListBuilder permissions;
    if (transient_background_permission) {
      permissions.Append("transientBackground");
    }
    if (native_messaging_permission) {
      permissions.Append("nativeMessaging");
    }
    manifest_builder.Set(manifest_keys::kPermissions, permissions.Build());

    base::FilePath path;
    EXPECT_TRUE(base::PathService::Get(DIR_TEST_DATA, &path));

    std::string error;
    scoped_refptr<Extension> extension(
        Extension::Create(path, Manifest::INTERNAL, *manifest_builder.Build(),
                          Extension::NO_FLAGS, &error));
    ASSERT_TRUE(extension.get()) << error;
    ExtensionRegistry::Get(&profile_)->AddEnabled(extension);
    extension_id_ = extension->id();
  }

  ScopedCurrentChannel channel_;
  content::BrowserTaskEnvironment task_environment_;
  bool has_listener_result_ = true;
  TestingProfile profile_;
  std::string extension_id_;
};

TEST_F(ExtensionSupportsConnectionFromNativeAppTest, Success) {
  ASSERT_NO_FATAL_FAILURE(RegisterExtension(true, true, true));

  EXPECT_TRUE(ExtensionSupportsConnectionFromNativeApp(
      extension_id_,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      &profile_, false));
}

TEST_F(ExtensionSupportsConnectionFromNativeAppTest, NoOnConnectNative) {
  ASSERT_NO_FATAL_FAILURE(RegisterExtension(true, true, true));
  has_listener_result_ = false;

  EXPECT_FALSE(ExtensionSupportsConnectionFromNativeApp(
      extension_id_,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      &profile_, false));
}

TEST_F(ExtensionSupportsConnectionFromNativeAppTest, OffTheRecordProfile) {
  auto* off_the_record_profile = profile_.GetOffTheRecordProfile();
  ASSERT_NO_FATAL_FAILURE(RegisterExtension(true, true, true));

  EXPECT_FALSE(ExtensionSupportsConnectionFromNativeApp(
      extension_id_,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      off_the_record_profile, false));
}

TEST_F(ExtensionSupportsConnectionFromNativeAppTest, NotNativelyConnectable) {
  ASSERT_NO_FATAL_FAILURE(RegisterExtension(false, true, true));

  EXPECT_FALSE(ExtensionSupportsConnectionFromNativeApp(
      extension_id_,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      &profile_, false));
}

TEST_F(ExtensionSupportsConnectionFromNativeAppTest, NotTransientBackground) {
  ASSERT_NO_FATAL_FAILURE(RegisterExtension(true, false, true));

  EXPECT_FALSE(ExtensionSupportsConnectionFromNativeApp(
      extension_id_,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      &profile_, false));
}

TEST_F(ExtensionSupportsConnectionFromNativeAppTest, NotNativeMessaging) {
  ASSERT_NO_FATAL_FAILURE(RegisterExtension(true, true, false));

  EXPECT_FALSE(ExtensionSupportsConnectionFromNativeApp(
      extension_id_,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      &profile_, false));
}

TEST_F(ExtensionSupportsConnectionFromNativeAppTest, NativeMessagingOnly) {
  ASSERT_NO_FATAL_FAILURE(RegisterExtension(false, false, true));

  EXPECT_FALSE(ExtensionSupportsConnectionFromNativeApp(
      extension_id_,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      &profile_, false));
}

TEST_F(ExtensionSupportsConnectionFromNativeAppTest, TransientBackgroundOnly) {
  ASSERT_NO_FATAL_FAILURE(RegisterExtension(false, true, false));

  EXPECT_FALSE(ExtensionSupportsConnectionFromNativeApp(
      extension_id_,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      &profile_, false));
}

TEST_F(ExtensionSupportsConnectionFromNativeAppTest, NativelyConnectableOnly) {
  ASSERT_NO_FATAL_FAILURE(RegisterExtension(true, false, false));

  EXPECT_FALSE(ExtensionSupportsConnectionFromNativeApp(
      extension_id_,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      &profile_, false));
}

TEST_F(ExtensionSupportsConnectionFromNativeAppTest, NoPermissions) {
  ASSERT_NO_FATAL_FAILURE(RegisterExtension(false, false, false));
  has_listener_result_ = false;

  EXPECT_FALSE(ExtensionSupportsConnectionFromNativeApp(
      extension_id_,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      &profile_, false));
}

TEST_F(ExtensionSupportsConnectionFromNativeAppTest, UnknownExtension) {
  EXPECT_FALSE(ExtensionSupportsConnectionFromNativeApp(
      "fake extension id",
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      &profile_, false));
}

}  // namespace
}  // namespace extensions

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/clipboard/chrome_clipboard_permission_context_delegate.h"

#include <string>

#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

class ChromeClipboardPermissionContextDelegateTest : public testing::Test {
 protected:
  // Registers an extension under `kExtensionId`, optionally holding the
  // clipboardWrite and clipboardRead API permissions.
  void AddExtension(bool with_clipboard_write,
                    bool with_clipboard_read = false) {
    extensions::ExtensionBuilder builder("test");
    builder.SetID(kExtensionId);
    if (with_clipboard_write) {
      builder.AddAPIPermission("clipboardWrite");
    }
    if (with_clipboard_read) {
      builder.AddAPIPermission("clipboardRead");
    }
    extensions::ExtensionRegistry::Get(&profile_)->AddEnabled(builder.Build());
  }

  GURL ExtensionOrigin() const {
    return GURL(std::string("chrome-extension://") + kExtensionId + "/sw.js");
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ChromeClipboardPermissionContextDelegateTest, NonExtensionOriginDefers) {
  ChromeClipboardPermissionContextDelegate delegate(
      ChromeClipboardPermissionContextDelegate::Type::kSanitizedWrite);
  EXPECT_EQ(std::nullopt, delegate.GetPermissionStatusForWorker(
                              &profile_, GURL("https://example.com/sw.js")));
}

TEST_F(ChromeClipboardPermissionContextDelegateTest,
       UnknownExtensionIsBlocked) {
  ChromeClipboardPermissionContextDelegate delegate(
      ChromeClipboardPermissionContextDelegate::Type::kSanitizedWrite);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, delegate.GetPermissionStatusForWorker(
                                       &profile_, ExtensionOrigin()));
}

TEST_F(ChromeClipboardPermissionContextDelegateTest, ReadIsBlocked) {
  AddExtension(/*with_clipboard_write=*/true, /*with_clipboard_read=*/true);
  ChromeClipboardPermissionContextDelegate delegate(
      ChromeClipboardPermissionContextDelegate::Type::kReadWrite);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, delegate.GetPermissionStatusForWorker(
                                       &profile_, ExtensionOrigin()));
}

TEST_F(ChromeClipboardPermissionContextDelegateTest, WriteNeedsThePermission) {
  AddExtension(/*with_clipboard_write=*/false);
  ChromeClipboardPermissionContextDelegate delegate(
      ChromeClipboardPermissionContextDelegate::Type::kSanitizedWrite);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, delegate.GetPermissionStatusForWorker(
                                       &profile_, ExtensionOrigin()));
}

TEST_F(ChromeClipboardPermissionContextDelegateTest, WriteWithThePermission) {
  AddExtension(/*with_clipboard_write=*/true);
  ChromeClipboardPermissionContextDelegate delegate(
      ChromeClipboardPermissionContextDelegate::Type::kSanitizedWrite);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, delegate.GetPermissionStatusForWorker(
                                       &profile_, ExtensionOrigin()));
}

}  // namespace

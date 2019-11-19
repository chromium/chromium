// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_util.h"

#include "base/macros.h"
#include "chrome/common/extensions/api/tabs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class ExtensionTabUtilTestDelegate : public ExtensionTabUtil::Delegate {
 public:
  ExtensionTabUtilTestDelegate() {}
  ~ExtensionTabUtilTestDelegate() override {}

  // ExtensionTabUtil::Delegate
  ExtensionTabUtil::ScrubTabBehaviorType GetScrubTabBehavior(
      const Extension* extension) override {
    return ExtensionTabUtil::kScrubTabUrlToOrigin;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionTabUtilTestDelegate);
};

}  // namespace

// Test that the custom GetScrubTabBehavior delegate works - in this test it
// always returns kScrubTabUrlToOrigin
TEST(ExtensionTabUtilTest, Delegate) {
  ExtensionTabUtil::SetPlatformDelegate(
      std::make_unique<ExtensionTabUtilTestDelegate>());

  // Build an extension that passes the permission checks for the generic
  // GetScrubTabBehavior
  auto extension = ExtensionBuilder("test").AddPermission("tabs").Build();

  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(
          extension.get(), Feature::Context::UNSPECIFIED_CONTEXT,
          GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kScrubTabUrlToOrigin,
            scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kScrubTabUrlToOrigin,
            scrub_tab_behavior.pending_info);

  // Unset the delegate.
  ExtensionTabUtil::SetPlatformDelegate(nullptr);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForTabsPermission) {
  auto extension = ExtensionBuilder("Extension with tabs permission")
                       .AddPermission("tabs")
                       .Build();
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(
          extension.get(), Feature::Context::UNSPECIFIED_CONTEXT,
          GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForNoPermission) {
  auto extension = ExtensionBuilder("Extension with no permissions").Build();
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(
          extension.get(), Feature::Context::UNSPECIFIED_CONTEXT,
          GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully,
            scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForHostPermission) {
  auto extension = ExtensionBuilder("Extension with host permission")
                       .AddPermission("*://www.google.com/*")
                       .Build();
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(
          extension.get(), Feature::Context::UNSPECIFIED_CONTEXT,
          GURL("http://www.google.com/some/path"));
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForNoExtension) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(
          nullptr, Feature::Context::UNSPECIFIED_CONTEXT,
          GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully,
            scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForWebUI) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(nullptr,
                                            Feature::Context::WEBUI_CONTEXT,
                                            GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.pending_info);
}

}  // namespace extensions

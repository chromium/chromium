// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/data_controls/test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/data_controls/features.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"

namespace enterprise_data_protection {

namespace {

class PolicyControllerTest : public ui::DataTransferPolicyController {
 public:
  PolicyControllerTest() = default;
  ~PolicyControllerTest() override = default;

  MOCK_METHOD3(IsClipboardReadAllowed,
               bool(base::optional_ref<const ui::DataTransferEndpoint> data_src,
                    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
                    const std::optional<size_t> size));

  MOCK_METHOD5(
      PasteIfAllowed,
      void(base::optional_ref<const ui::DataTransferEndpoint> data_src,
           base::optional_ref<const ui::DataTransferEndpoint> data_dst,
           absl::variant<size_t, std::vector<base::FilePath>> pasted_content,
           content::RenderFrameHost* rfh,
           base::OnceCallback<void(bool)> callback));

  MOCK_METHOD3(DropIfAllowed,
               void(const ui::OSExchangeData* drag_data,
                    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
                    base::OnceClosure drop_cb));
};

content::ClipboardEndpoint SourceEndpoint() {
  return content::ClipboardEndpoint(
      ui::DataTransferEndpoint(GURL("https://source.com")));
}

class DataProtectionClipboardTest : public testing::Test {
 public:
  DataProtectionClipboardTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    scoped_features_.InitAndEnableFeature(
        data_controls::kEnableDesktopDataControls);
  }

  content::WebContents* contents() {
    if (!web_contents_) {
      content::WebContents::CreateParams params(profile_);
      web_contents_ = content::WebContents::Create(params);
    }
    return web_contents_.get();
  }

  content::BrowserContext* browser_context() {
    return contents()->GetBrowserContext();
  }

  content::ClipboardEndpoint DestinationEndpoint() {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(GURL("https://source.com")),
        base::BindLambdaForTesting(
            [this]() { return contents()->GetBrowserContext(); }),
        *contents()->GetPrimaryMainFrame());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_features_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
};

using DataProtectionPasteIfAllowedByPolicyTest = DataProtectionClipboardTest;
using DataProtectionIsClipboardCopyAllowedByPolicyTest =
    DataProtectionClipboardTest;

}  // namespace

TEST_F(DataProtectionPasteIfAllowedByPolicyTest,
       DataTransferPolicyController_NoController) {
  // Without a controller set up, the paste should be allowed through.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      SourceEndpoint(), DestinationEndpoint(), {.size = 1234},
      content::ClipboardPasteData("text", "image", {}), future.GetCallback());
  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, "text");
  EXPECT_EQ(paste_data->image, "image");
}

TEST_F(DataProtectionPasteIfAllowedByPolicyTest,
       DataTransferPolicyController_Allowed) {
  PolicyControllerTest policy_controller;
  EXPECT_CALL(policy_controller, PasteIfAllowed)
      .WillOnce(testing::Invoke(
          [](base::optional_ref<const ui::DataTransferEndpoint> data_src,
             base::optional_ref<const ui::DataTransferEndpoint> data_dst,
             absl::variant<size_t, std::vector<base::FilePath>> pasted_content,
             content::RenderFrameHost* rfh,
             base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          }));

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      SourceEndpoint(), DestinationEndpoint(), {.size = 1234},
      content::ClipboardPasteData("text", "image", {}), future.GetCallback());

  testing::Mock::VerifyAndClearExpectations(&policy_controller);
  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, "text");
  EXPECT_EQ(paste_data->image, "image");
}

TEST_F(DataProtectionPasteIfAllowedByPolicyTest,
       DataTransferPolicyController_Blocked) {
  PolicyControllerTest policy_controller;
  EXPECT_CALL(policy_controller, PasteIfAllowed)
      .WillOnce(testing::Invoke(
          [](base::optional_ref<const ui::DataTransferEndpoint> data_src,
             base::optional_ref<const ui::DataTransferEndpoint> data_dst,
             absl::variant<size_t, std::vector<base::FilePath>> pasted_content,
             content::RenderFrameHost* rfh,
             base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          }));

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      SourceEndpoint(), DestinationEndpoint(), {.size = 1234},
      content::ClipboardPasteData("text", "image", {}), future.GetCallback());

  testing::Mock::VerifyAndClearExpectations(&policy_controller);
  EXPECT_FALSE(future.Get());
}

TEST_F(DataProtectionPasteIfAllowedByPolicyTest,
       DataProtectionPaste_NoDestinationWebContents) {
  // Missing a destination WebContents implies the tab is gone, so null should
  // always be returned even if no DC rule is set.
  base::test::TestFuture<absl::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      SourceEndpoint(),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://destination.com"))),
      {.size = 1234}, content::ClipboardPasteData("text", "image", {}),
      future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest, Default) {
  std::u16string replacement;
  EXPECT_TRUE(IsClipboardCopyAllowedByPolicy(
      browser_context(), GURL("https://source.com"), 123, replacement));
  EXPECT_TRUE(replacement.empty());
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest, StringReplacement) {
  data_controls::SetDataControls(profile_->GetPrefs(), {
                                                           R"({
                    "sources": {
                      "urls": ["source.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  std::u16string replacement;
  EXPECT_FALSE(IsClipboardCopyAllowedByPolicy(
      browser_context(), GURL("https://source.com"), 123, replacement));
  EXPECT_EQ(replacement,
            u"Pasting this content here is blocked by your administrator.");
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest, NoStringReplacement) {
  data_controls::SetDataControls(profile_->GetPrefs(), {
                                                           R"({
                    "sources": {
                      "urls": ["source.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  std::u16string replacement;
  EXPECT_TRUE(IsClipboardCopyAllowedByPolicy(
      browser_context(), GURL("https://random.com"), 123, replacement));
  EXPECT_TRUE(replacement.empty());
}

}  // namespace enterprise_data_protection

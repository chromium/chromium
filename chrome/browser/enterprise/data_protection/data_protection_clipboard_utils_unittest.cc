// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"

#include <variant>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_metadata.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"

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
           std::variant<size_t, std::vector<base::FilePath>> pasted_content,
           content::RenderFrameHost* rfh,
           base::OnceCallback<void(bool)> callback));

  MOCK_METHOD4(DropIfAllowed,
               void(std::optional<ui::DataTransferEndpoint> data_src,
                    std::optional<ui::DataTransferEndpoint> data_dst,
                    std::optional<std::vector<ui::FileInfo>> filenames,
                    base::OnceClosure drop_cb));
};

ui::ClipboardMetadata CopyMetadata() {
  return {.size = 123};
}

content::ClipboardPasteData MakeClipboardPasteData(
    std::string text,
    std::string image,
    std::vector<base::FilePath> file_paths) {
  content::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = base::UTF8ToUTF16(text);
  clipboard_paste_data.png = std::vector<uint8_t>(image.begin(), image.end());
  clipboard_paste_data.file_paths = std::move(file_paths);
  return clipboard_paste_data;
}

class DataProtectionClipboardTest : public testing::Test {
 public:
  DataProtectionClipboardTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void SetUp() override { ui::TestClipboard::CreateForCurrentThread(); }

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

  content::ClipboardEndpoint SourceEndpoint() {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(GURL("https://source.com")),
        base::BindLambdaForTesting(
            [this]() { return contents()->GetBrowserContext(); }),
        *contents()->GetPrimaryMainFrame());
  }

  content::ClipboardEndpoint NoBrowserContextSourceEndpoint() {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(GURL("https://source.com")));
  }

  content::ClipboardEndpoint DestinationEndpoint() {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(GURL("https://destination.com")),
        base::BindLambdaForTesting(
            [this]() { return contents()->GetBrowserContext(); }),
        *contents()->GetPrimaryMainFrame());
  }

  content::ClipboardEndpoint CopyEndpoint(GURL url) {
    return content::ClipboardEndpoint(ui::DataTransferEndpoint(std::move(url)),
                                      base::BindLambdaForTesting([this]() {
                                        return contents()->GetBrowserContext();
                                      }),
                                      *contents()->GetPrimaryMainFrame());
  }

#if BUILDFLAG(IS_ANDROID)
  void EnableDataControls() {
    scoped_features_.InitWithFeatures(
        /* enabled_features */ {data_controls::
                                    kEnableClipboardDataControlsAndroid},
        /* disabled_features */ {});
  }

  void DisableDataControls() {
    scoped_features_.InitWithFeatures(
        /* enabled_features */ {},
        /* disabled_features */ {
            data_controls::kEnableClipboardDataControlsAndroid});
  }
#endif  // BUILDFLAG(IS_ANDROID)

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::test::ScopedFeatureList scoped_features_;
};

using DataProtectionPasteIfAllowedByPolicyTest = DataProtectionClipboardTest;
using DataProtectionIsClipboardCopyAllowedByPolicyTest =
    DataProtectionClipboardTest;

class DataProtectionClipboardDistilledURLTest
    : public DataProtectionClipboardTest {
 public:
  void SetUp() override {
    DataProtectionClipboardTest::SetUp();
    scoped_features_.InitAndEnableFeature(
        data_controls::kDataControlsSearchWith);
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr);

    const GURL article_url("https://source.com/article.html");
    const GURL distiller_url =
        dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
            dom_distiller::kDomDistillerScheme, article_url, "title");

    content::WebContentsTester::For(test_web_contents_.get())
        ->NavigateAndCommit(distiller_url);
  }

  void SetBlockCopyingFromSourceURLRule() {
    data_controls::SetDataControls(profile_->GetPrefs(), {
                                                             R"({
        "sources": {
          "urls": ["source.com"]
        },
        "destinations": {
          "os_clipboard": true
        },
        "restrictions": [
          {"class": "CLIPBOARD", "level": "BLOCK"}
        ]
    })"});
  }

  void SetWarnCopyingFromSourceURLRule() {
    data_controls::SetDataControls(profile_->GetPrefs(), {
                                                             R"({
        "sources": {
          "urls": ["source.com"]
        },
        "destinations": {
          "os_clipboard": true
        },
        "restrictions": [
          {"class": "CLIPBOARD", "level": "WARN"}
        ]
      })"});
  }

 protected:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> test_web_contents_;
};

}  // namespace

TEST_F(DataProtectionPasteIfAllowedByPolicyTest,
       DataTransferPolicyController_NoController) {
  // Without a controller set up, the paste should be allowed through.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  auto source = SourceEndpoint();
  auto destination = DestinationEndpoint();
  ui::ClipboardMetadata metadata = {.size = 1234};
  EXPECT_FALSE(IsPastePolicyCheckRequired(source, destination, metadata));
  PasteIfAllowedByPolicy(source, destination, metadata,
                         MakeClipboardPasteData("text", "image", {}),
                         future.GetCallback());
  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");
}

// The DataTransferPolicyController is not relevant / supported by Clank, and
// is thus disabled.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(DataProtectionPasteIfAllowedByPolicyTest,
       DataTransferPolicyController_Allowed) {
  PolicyControllerTest policy_controller;
  EXPECT_CALL(policy_controller, PasteIfAllowed)
      .WillOnce(
          [](base::optional_ref<const ui::DataTransferEndpoint> data_src,
             base::optional_ref<const ui::DataTransferEndpoint> data_dst,
             std::variant<size_t, std::vector<base::FilePath>> pasted_content,
             content::RenderFrameHost* rfh,
             base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          });

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  auto source = SourceEndpoint();
  auto destination = DestinationEndpoint();
  ui::ClipboardMetadata metadata = {.size = 1234};
  EXPECT_TRUE(IsPastePolicyCheckRequired(source, destination, metadata));
  PasteIfAllowedByPolicy(source, destination, metadata,
                         MakeClipboardPasteData("text", "image", {}),
                         future.GetCallback());

  testing::Mock::VerifyAndClearExpectations(&policy_controller);
  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");
}

TEST_F(DataProtectionPasteIfAllowedByPolicyTest,
       DataTransferPolicyController_Blocked) {
  PolicyControllerTest policy_controller;
  EXPECT_CALL(policy_controller, PasteIfAllowed)
      .WillOnce(
          [](base::optional_ref<const ui::DataTransferEndpoint> data_src,
             base::optional_ref<const ui::DataTransferEndpoint> data_dst,
             std::variant<size_t, std::vector<base::FilePath>> pasted_content,
             content::RenderFrameHost* rfh,
             base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  auto source = SourceEndpoint();
  auto destination = DestinationEndpoint();
  ui::ClipboardMetadata metadata = {.size = 1234};
  EXPECT_TRUE(IsPastePolicyCheckRequired(source, destination, metadata));
  PasteIfAllowedByPolicy(source, destination, metadata,
                         MakeClipboardPasteData("text", "image", {}),
                         future.GetCallback());

  testing::Mock::VerifyAndClearExpectations(&policy_controller);
  EXPECT_FALSE(future.Get());
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(DataProtectionPasteIfAllowedByPolicyTest,
       DataProtectionPaste_NoDestinationWebContents) {
  // Missing a destination WebContents implies the tab is gone, so null should
  // always be returned even if no DC rule is set.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  auto source = SourceEndpoint();
  auto destination = content::ClipboardEndpoint(
      ui::DataTransferEndpoint(GURL("https://destination.com")));
  ui::ClipboardMetadata metadata = {.size = 1234};
  EXPECT_FALSE(IsPastePolicyCheckRequired(source, destination, metadata));
  PasteIfAllowedByPolicy(source, destination, metadata,
                         MakeClipboardPasteData("text", "image", {}),
                         future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest, Default) {
#if BUILDFLAG(IS_ANDROID)
  EnableDataControls();
#endif  // BUILDFLAG(IS_ANDROID)
  auto source = CopyEndpoint(GURL("https://source.com"));
  auto metadata = CopyMetadata();
  EXPECT_FALSE(IsCopyPolicyCheckRequired(source, metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(source, metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 future.GetCallback());
  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest, NoEndpoint) {
#if BUILDFLAG(IS_ANDROID)
  EnableDataControls();
#endif  // BUILDFLAG(IS_ANDROID)
  auto source = content::ClipboardEndpoint(std::nullopt);
  auto metadata = CopyMetadata();
  EXPECT_FALSE(IsCopyPolicyCheckRequired(source, metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(source, metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 future.GetCallback());
  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest, StringReplacement) {
#if BUILDFLAG(IS_ANDROID)
  EnableDataControls();
#endif  // BUILDFLAG(IS_ANDROID)
  data_controls::SetDataControls(profile_->GetPrefs(), {
                                                           R"({
                    "sources": {
                      "urls": ["source.com"]
                    },
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  ui::ClipboardMetadata metadata = CopyMetadata();
  metadata.seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  auto source = CopyEndpoint(GURL("https://source.com"));
  auto copy_metadata = CopyMetadata();
  EXPECT_TRUE(IsCopyPolicyCheckRequired(source, copy_metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      copy_future;
  IsClipboardCopyAllowedByPolicy(source, copy_metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 copy_future.GetCallback());
  auto data = copy_future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = copy_future.Get<std::optional<std::u16string>>();
  EXPECT_TRUE(replacement);
  EXPECT_EQ(*replacement,
            u"Pasting this content here is blocked by your administrator.");

  // This triggers the clipboard observer started by the
  // `IsClipboardCopyAllowedByPolicy` calls so that they're aware of the new
  // seqno.
  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  // Since the rule only applied to copying to the OS clipboard, pasting should
  // still be allowed and use cached data.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      first_paste_future;
  auto paste_source = SourceEndpoint();
  auto paste_destination = DestinationEndpoint();
  EXPECT_TRUE(
      IsPastePolicyCheckRequired(paste_source, paste_destination, metadata));
  PasteIfAllowedByPolicy(paste_source, paste_destination, metadata,
                         MakeClipboardPasteData("to be", "replaced", {}),
                         first_paste_future.GetCallback());

  auto first_paste_data = first_paste_future.Get();
  EXPECT_TRUE(first_paste_data);
  EXPECT_EQ(first_paste_data->text, u"foo");
  EXPECT_TRUE(first_paste_data->png.empty());

  // Same-tab replacement should also work when the same seqno that was replaced
  // is used.
  content::ClipboardPasteData same_tab_data;
  ReplaceSameTabClipboardDataIfRequiredByPolicy(metadata.seqno, same_tab_data);
  EXPECT_EQ(same_tab_data.text, u"foo");
  EXPECT_TRUE(same_tab_data.png.empty());

  // Pasting again with a new seqno implies new data in the clipboard from
  // outside of Chrome, so it should be let through without replacement when it
  // triggers no rule.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      second_paste_future;
  ui::ClipboardMetadata new_metadata;
  EXPECT_FALSE(IsPastePolicyCheckRequired(paste_source, paste_destination,
                                          new_metadata));
  PasteIfAllowedByPolicy(paste_source, paste_destination, new_metadata,
                         MakeClipboardPasteData("text", "image", {}),
                         second_paste_future.GetCallback());

  auto second_paste_data = second_paste_future.Get();
  EXPECT_TRUE(second_paste_data);
  EXPECT_EQ(second_paste_data->text, u"text");
  EXPECT_EQ(
      std::string(second_paste_data->png.begin(), second_paste_data->png.end()),
      "image");

  // Same-tab replacement should also do nothing with that new seqno.
  same_tab_data = {};
  ReplaceSameTabClipboardDataIfRequiredByPolicy(new_metadata.seqno,
                                                same_tab_data);
  EXPECT_TRUE(same_tab_data.empty());
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest,
       StringReplacement_NoBrowserContextSource) {
#if BUILDFLAG(IS_ANDROID)
  EnableDataControls();
#endif  // BUILDFLAG(IS_ANDROID)
  data_controls::SetDataControls(profile_->GetPrefs(), {
                                                           R"({
                    "sources": {
                      "urls": ["source.com"]
                    },
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  ui::ClipboardMetadata metadata = CopyMetadata();
  metadata.seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  auto source = CopyEndpoint(GURL("https://source.com"));
  auto copy_metadata = CopyMetadata();
  EXPECT_TRUE(IsCopyPolicyCheckRequired(source, copy_metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      copy_future;
  IsClipboardCopyAllowedByPolicy(source, copy_metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 copy_future.GetCallback());
  auto data = copy_future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = copy_future.Get<std::optional<std::u16string>>();
  EXPECT_TRUE(replacement);
  EXPECT_EQ(*replacement,
            u"Pasting this content here is blocked by your administrator.");

  // This triggers the clipboard observer started by the
  // `IsClipboardCopyAllowedByPolicy` calls so that they're aware of the new
  // seqno.
  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  // Since the source endpoint is missing (eg. since the profile was closed),
  // the data isn't allowed to be replaced back.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      first_paste_future;
  auto first_paste_source = NoBrowserContextSourceEndpoint();
  auto first_paste_destination = DestinationEndpoint();
  EXPECT_FALSE(IsPastePolicyCheckRequired(first_paste_source,
                                          first_paste_destination, metadata));
  PasteIfAllowedByPolicy(first_paste_source, first_paste_destination, metadata,
                         MakeClipboardPasteData("to be", "kept", {}),
                         first_paste_future.GetCallback());

  auto first_paste_data = first_paste_future.Get();
  EXPECT_TRUE(first_paste_data);
  EXPECT_EQ(first_paste_data->text, u"to be");
  EXPECT_EQ(
      std::string(first_paste_data->png.begin(), first_paste_data->png.end()),
      "kept");

  // Same-tab replacement should still be allow to be replaced as it happened in
  // a known browser context.
  content::ClipboardPasteData same_tab_data;
  ReplaceSameTabClipboardDataIfRequiredByPolicy(metadata.seqno, same_tab_data);
  EXPECT_EQ(same_tab_data.text, u"foo");

  // Pasting again with a new seqno implies new data in the clipboard from
  // outside of Chrome, so it should be let through without replacement when it
  // triggers no rule.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      second_paste_future;
  ui::ClipboardMetadata new_metadata;
  auto second_paste_source = SourceEndpoint();
  auto second_paste_destination = DestinationEndpoint();
  EXPECT_FALSE(IsPastePolicyCheckRequired(
      second_paste_source, second_paste_destination, new_metadata));
  PasteIfAllowedByPolicy(second_paste_source, second_paste_destination,
                         new_metadata,
                         MakeClipboardPasteData("text", "image", {}),
                         second_paste_future.GetCallback());

  auto second_paste_data = second_paste_future.Get();
  EXPECT_TRUE(second_paste_data);
  EXPECT_EQ(second_paste_data->text, u"text");
  EXPECT_EQ(
      std::string(second_paste_data->png.begin(), second_paste_data->png.end()),
      "image");

  // Same-tab replacement should still do nothing since the data wasn't
  // replaced.
  same_tab_data = {};
  ReplaceSameTabClipboardDataIfRequiredByPolicy(new_metadata.seqno,
                                                same_tab_data);
  EXPECT_TRUE(same_tab_data.empty());
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest,
       CustomDataReplacement) {
#if BUILDFLAG(IS_ANDROID)
  EnableDataControls();
#endif  // BUILDFLAG(IS_ANDROID)
  data_controls::SetDataControls(profile_->GetPrefs(), {
                                                           R"({
                    "sources": {
                      "urls": ["source.com"]
                    },
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  ui::ClipboardMetadata metadata = CopyMetadata();
  metadata.seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  metadata.format_type = ui::ClipboardFormatType::WebCustomFormatMap();

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      copy_future;

  // `WebCustomFormatMap` passes empty `ClipboardPasteData` because the real
  // data is passed as an opaque `BigBuffer`. We invoke policy checks using
  // empty object sizes or metadata overrides instead.
  content::ClipboardPasteData empty_custom_data;

  auto source = CopyEndpoint(GURL("https://source.com"));
  EXPECT_TRUE(IsCopyPolicyCheckRequired(source, metadata));

  IsClipboardCopyAllowedByPolicy(source, metadata, std::move(empty_custom_data),
                                 copy_future.GetCallback());

  auto data = copy_future.Get<content::ClipboardPasteData>();
  EXPECT_TRUE(data.custom_data.empty());

  auto replacement = copy_future.Get<std::optional<std::u16string>>();
  EXPECT_TRUE(replacement);
  EXPECT_EQ(*replacement,
            u"Pasting this content here is blocked by your administrator.");

  // This triggers the clipboard observer started by the
  // `IsClipboardCopyAllowedByPolicy` calls so that they're aware of the new
  // seqno.
  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  // Since the rule only applied to copying to the OS clipboard, pasting should
  // still be allowed and use cached data.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      first_paste_future;
  auto paste_source = SourceEndpoint();
  auto paste_destination = DestinationEndpoint();
  EXPECT_TRUE(
      IsPastePolicyCheckRequired(paste_source, paste_destination, metadata));
  PasteIfAllowedByPolicy(paste_source, paste_destination, metadata,
                         content::ClipboardPasteData(),
                         first_paste_future.GetCallback());

  auto first_paste_data = first_paste_future.Get();
  EXPECT_TRUE(first_paste_data);
  EXPECT_TRUE(first_paste_data->empty());

  // Same-tab replacement does not apply to `WebCustomFormatMap` because the
  // policy engine receives and caches empty object, not opaque `BigBuffer`.
  content::ClipboardPasteData same_tab_data;
  ReplaceSameTabClipboardDataIfRequiredByPolicy(metadata.seqno, same_tab_data);
  EXPECT_TRUE(same_tab_data.custom_data.empty());

  // Pasting again with a new seqno means new data in the clipboard from
  // outside of Chrome, so it should be let through without replacement when it
  // triggers no rule.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      second_paste_future;
  ui::ClipboardMetadata new_metadata;
  EXPECT_FALSE(IsPastePolicyCheckRequired(paste_source, paste_destination,
                                          new_metadata));
  PasteIfAllowedByPolicy(paste_source, paste_destination, new_metadata,
                         MakeClipboardPasteData("text", "image", {}),
                         second_paste_future.GetCallback());

  auto new_data = second_paste_future.Get();
  EXPECT_TRUE(new_data);
  EXPECT_TRUE(new_data->custom_data.empty());
  EXPECT_EQ(new_data->text, u"text");
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest,
       StringReplacement_MultiType) {
#if BUILDFLAG(IS_ANDROID)
  EnableDataControls();
#endif  // BUILDFLAG(IS_ANDROID)
  data_controls::SetDataControls(profile_->GetPrefs(), {
                                                           R"({
                    "sources": {
                      "urls": ["source.com"]
                    },
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  ui::ClipboardMetadata text_metadata = CopyMetadata();
  text_metadata.seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  text_metadata.format_type = ui::ClipboardFormatType::PlainTextType();

  ui::ClipboardMetadata image_metadata = text_metadata;
  image_metadata.format_type = ui::ClipboardFormatType::PngType();

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      text_copy_future;
  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      image_copy_future;

  auto source = CopyEndpoint(GURL("https://source.com"));
  EXPECT_TRUE(IsCopyPolicyCheckRequired(source, text_metadata));
  EXPECT_TRUE(IsCopyPolicyCheckRequired(source, image_metadata));

  IsClipboardCopyAllowedByPolicy(source, text_metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 text_copy_future.GetCallback());
  IsClipboardCopyAllowedByPolicy(source, image_metadata,
                                 MakeClipboardPasteData("", "bar", {}),
                                 image_copy_future.GetCallback());

  auto text_data = text_copy_future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(text_data.text, u"foo");
  auto image_data = image_copy_future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(std::string(image_data.png.begin(), image_data.png.end()), "bar");

  auto text_replacement = text_copy_future.Get<std::optional<std::u16string>>();
  EXPECT_TRUE(text_replacement);
  EXPECT_EQ(*text_replacement,
            u"Pasting this content here is blocked by your administrator.");
  auto image_replacement =
      image_copy_future.Get<std::optional<std::u16string>>();
  EXPECT_TRUE(image_replacement);
  EXPECT_EQ(*image_replacement,
            u"Pasting this content here is blocked by your administrator.");

  // This triggers the clipboard observer started by the
  // `IsClipboardCopyAllowedByPolicy` calls so that they're aware of the new
  // seqno.
  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  // Since the rule only applied to copying to the OS clipboard, pasting should
  // still be allowed and use cached data.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      first_text_paste_future;
  auto paste_source = SourceEndpoint();
  auto paste_destination = DestinationEndpoint();
  EXPECT_TRUE(IsPastePolicyCheckRequired(paste_source, paste_destination,
                                         text_metadata));
  PasteIfAllowedByPolicy(paste_source, paste_destination, text_metadata,
                         MakeClipboardPasteData("to be", "replaced", {}),
                         first_text_paste_future.GetCallback());

  auto first_paste_data = first_text_paste_future.Get();
  EXPECT_TRUE(first_paste_data);
  EXPECT_EQ(first_paste_data->text, u"foo");
  EXPECT_EQ(
      std::string(first_paste_data->png.begin(), first_paste_data->png.end()),
      "bar");

  // Same-tab replacement should use the cached data as well.
  content::ClipboardPasteData same_tab_data;
  ReplaceSameTabClipboardDataIfRequiredByPolicy(text_metadata.seqno,
                                                same_tab_data);
  EXPECT_EQ(same_tab_data.text, u"foo");
  EXPECT_EQ(std::string(same_tab_data.png.begin(), same_tab_data.png.end()),
            "bar");

  // Pasting again with a new seqno implies new data in the clipboard from
  // outside of Chrome, so it should be let through without replacement when it
  // triggers no rule.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      second_paste_future;
  ui::ClipboardMetadata new_metadata;
  EXPECT_FALSE(IsPastePolicyCheckRequired(paste_source, paste_destination,
                                          new_metadata));
  PasteIfAllowedByPolicy(paste_source, paste_destination, new_metadata,
                         MakeClipboardPasteData("text", "image", {}),
                         second_paste_future.GetCallback());

  auto second_paste_data = second_paste_future.Get();
  EXPECT_TRUE(second_paste_data);
  EXPECT_EQ(second_paste_data->text, u"text");
  EXPECT_EQ(
      std::string(second_paste_data->png.begin(), second_paste_data->png.end()),
      "image");

  // Same-tab replacement should do nothing with the new seqno as no replacement
  // was performed.
  same_tab_data = {};
  ReplaceSameTabClipboardDataIfRequiredByPolicy(new_metadata.seqno,
                                                same_tab_data);
  EXPECT_TRUE(same_tab_data.empty());
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest, NoStringReplacement) {
#if BUILDFLAG(IS_ANDROID)
  EnableDataControls();
#endif  // BUILDFLAG(IS_ANDROID)
  data_controls::SetDataControls(profile_->GetPrefs(), {
                                                           R"({
                    "sources": {
                      "urls": ["source.com"]
                    },
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  auto source = CopyEndpoint(GURL("https://random.com"));
  ui::ClipboardMetadata metadata = CopyMetadata();
  EXPECT_FALSE(IsCopyPolicyCheckRequired(source, metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(source, metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 future.GetCallback());

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);

  content::ClipboardPasteData same_tab_data;
  ReplaceSameTabClipboardDataIfRequiredByPolicy(metadata.seqno, same_tab_data);
  EXPECT_TRUE(same_tab_data.empty());
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest, BitmapReplacement) {
#if BUILDFLAG(IS_ANDROID)
  EnableDataControls();
#endif  // BUILDFLAG(IS_ANDROID)
  data_controls::SetDataControls(profile_->GetPrefs(), {
                                                           R"({
                    "sources": {
                      "urls": ["source.com"]
                    },
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  ui::ClipboardMetadata metadata = CopyMetadata();
  metadata.seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);

  const SkBitmap kBitmap = gfx::test::CreateBitmap(3, 2);
  content::ClipboardPasteData bitmap_data;
  bitmap_data.bitmap = kBitmap;

  auto source = CopyEndpoint(GURL("https://source.com"));
  auto copy_metadata = CopyMetadata();
  EXPECT_TRUE(IsCopyPolicyCheckRequired(source, copy_metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      copy_future;
  IsClipboardCopyAllowedByPolicy(source, copy_metadata, bitmap_data,
                                 copy_future.GetCallback());
  auto data = copy_future.Get<content::ClipboardPasteData>();
  EXPECT_TRUE(gfx::BitmapsAreEqual(kBitmap, data.bitmap));

  auto replacement = copy_future.Get<std::optional<std::u16string>>();
  EXPECT_TRUE(replacement);
  EXPECT_EQ(*replacement,
            u"Pasting this content here is blocked by your administrator.");

  // This triggers the clipboard observer started by the
  // `IsClipboardCopyAllowedByPolicy` calls so that they're aware of the new
  // seqno.
  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  // Since the rule only applied to copying to the OS clipboard, pasting should
  // still be allowed and use cached data.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      first_paste_future;
  auto paste_source = SourceEndpoint();
  auto paste_destination = DestinationEndpoint();
  EXPECT_TRUE(
      IsPastePolicyCheckRequired(paste_source, paste_destination, metadata));
  PasteIfAllowedByPolicy(paste_source, paste_destination, metadata,
                         MakeClipboardPasteData("to be", "replaced", {}),
                         first_paste_future.GetCallback());

  auto first_paste_data = first_paste_future.Get();
  EXPECT_TRUE(first_paste_data);
  EXPECT_TRUE(first_paste_data->text.empty());
  EXPECT_TRUE(first_paste_data->html.empty());

  // The pasted bitmap should be in the PNG field instead of the bitmap one.
  SkBitmap pasted_bitmap = gfx::PNGCodec::Decode(first_paste_data->png);
  ASSERT_FALSE(pasted_bitmap.isNull());
  EXPECT_TRUE(gfx::BitmapsAreEqual(kBitmap, pasted_bitmap));
  EXPECT_TRUE(first_paste_data->bitmap.empty());

  // Same-tab replacement should apply to the cached bitmap as well.
  content::ClipboardPasteData same_tab_data;
  ReplaceSameTabClipboardDataIfRequiredByPolicy(metadata.seqno, same_tab_data);

  pasted_bitmap = gfx::PNGCodec::Decode(same_tab_data.png);
  ASSERT_FALSE(pasted_bitmap.isNull());
  EXPECT_TRUE(gfx::BitmapsAreEqual(kBitmap, pasted_bitmap));
  EXPECT_TRUE(same_tab_data.bitmap.empty());

  // Pasting again with a new seqno implies new data in the clipboard from
  // outside of Chrome, so it should be let through without replacement when it
  // triggers no rule.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      second_paste_future;
  ui::ClipboardMetadata new_metadata;
  EXPECT_FALSE(IsPastePolicyCheckRequired(paste_source, paste_destination,
                                          new_metadata));
  PasteIfAllowedByPolicy(paste_source, paste_destination, new_metadata,
                         MakeClipboardPasteData("text", "image", {}),
                         second_paste_future.GetCallback());

  auto second_paste_data = second_paste_future.Get();
  EXPECT_TRUE(second_paste_data);
  EXPECT_EQ(second_paste_data->text, u"text");
  EXPECT_EQ(
      std::string(second_paste_data->png.begin(), second_paste_data->png.end()),
      "image");

  // Same-tab replacement should do nothing with the new seqno as no replacement
  // was performed.
  same_tab_data = {};
  ReplaceSameTabClipboardDataIfRequiredByPolicy(new_metadata.seqno,
                                                same_tab_data);
  EXPECT_TRUE(same_tab_data.empty());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest,
       CopyAction_DataControlsDisabledOnAndroid) {
  DisableDataControls();
  data_controls::SetDataControls(profile_->GetPrefs(), {
                                                           R"({
                    "sources": {
                      "urls": ["source.com"]
                    },
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  auto source = CopyEndpoint(GURL("https://source.com"));
  ui::ClipboardMetadata metadata = CopyMetadata();
  EXPECT_FALSE(IsCopyPolicyCheckRequired(source, metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(source, metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 future.GetCallback());

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);

  content::ClipboardPasteData same_tab_data;
  ReplaceSameTabClipboardDataIfRequiredByPolicy(metadata.seqno, same_tab_data);
  EXPECT_TRUE(same_tab_data.empty());
}

TEST_F(DataProtectionPasteIfAllowedByPolicyTest,
       PasteAction_DataControlsDisabledOnAndroid) {
  DisableDataControls();
  data_controls::SetDataControls(profile_->GetPrefs(), {
                                                           R"({
                    "destinations": {
                      "urls": ["destination.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  // Without a controller set up, the paste should be allowed through.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  auto source = SourceEndpoint();
  auto destination = DestinationEndpoint();
  ui::ClipboardMetadata metadata = {.size = 1234};
  EXPECT_FALSE(IsPastePolicyCheckRequired(source, destination, metadata));
  PasteIfAllowedByPolicy(source, destination, metadata,
                         MakeClipboardPasteData("text", "image", {}),
                         future.GetCallback());
  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(DataProtectionClipboardTest, DragAllowed_NoRule) {
  content::DropData drop_data;
  drop_data.text = u"allowed";

  EXPECT_TRUE(IsDragAllowedByPolicy(SourceEndpoint(), drop_data));
}

TEST_F(DataProtectionClipboardDistilledURLTest,
       CanPopulateFindBarFromSelection_Allowed) {
  EXPECT_TRUE(CanPopulateFindBarFromSelection(test_web_contents_.get()));
}

TEST_F(DataProtectionClipboardDistilledURLTest,
       ReplaceCopyFromFindBar_Allowed) {
  std::u16string copy_replacement;
  EXPECT_FALSE(ReplaceCopyFromFindBar(u"text", test_web_contents_.get(),
                                      &copy_replacement));
  EXPECT_TRUE(copy_replacement.empty());
}

TEST_F(DataProtectionClipboardDistilledURLTest, ReplacePasteToFindBar_Allowed) {
  base::test::TestFuture<std::optional<std::u16string>> replace_paste_future;
  ReplacePasteToFindBar(test_web_contents_.get(),
                        replace_paste_future.GetCallback());
  EXPECT_FALSE(replace_paste_future.Get());
}

TEST_F(DataProtectionClipboardDistilledURLTest, IsSearchWithAllowed_Allowed) {
  EXPECT_TRUE(IsSearchWithAllowed(test_web_contents_.get()));
}

TEST_F(DataProtectionClipboardDistilledURLTest, ShouldAllowSearchWith_Allowed) {
  base::test::TestFuture<void> should_allow_search_future;
  ShouldAllowSearchWith(test_web_contents_.get(), 10,
                        should_allow_search_future.GetCallback());
  EXPECT_TRUE(should_allow_search_future.Wait());
}

TEST_F(DataProtectionClipboardDistilledURLTest,
       CanPopulateFindBarFromSelection_Block) {
  SetBlockCopyingFromSourceURLRule();
  EXPECT_FALSE(CanPopulateFindBarFromSelection(test_web_contents_.get()));
}

TEST_F(DataProtectionClipboardDistilledURLTest,
       CanPopulateFindBarFromSelection_Warn) {
  SetWarnCopyingFromSourceURLRule();
  EXPECT_TRUE(CanPopulateFindBarFromSelection(test_web_contents_.get()));
}

TEST_F(DataProtectionClipboardDistilledURLTest, ReplaceCopyFromFindBar_Block) {
  SetBlockCopyingFromSourceURLRule();
  std::u16string copy_replacement;
  EXPECT_TRUE(ReplaceCopyFromFindBar(u"text", test_web_contents_.get(),
                                     &copy_replacement));
  EXPECT_FALSE(copy_replacement.empty());
}

TEST_F(DataProtectionClipboardDistilledURLTest, ReplaceCopyFromFindBar_Warn) {
  SetWarnCopyingFromSourceURLRule();
  std::u16string copy_replacement;
  EXPECT_FALSE(ReplaceCopyFromFindBar(u"text", test_web_contents_.get(),
                                      &copy_replacement));
  EXPECT_TRUE(copy_replacement.empty());
}

TEST_F(DataProtectionClipboardDistilledURLTest, IsSearchWithAllowed_Block) {
  SetBlockCopyingFromSourceURLRule();
  EXPECT_FALSE(IsSearchWithAllowed(test_web_contents_.get()));
}

TEST_F(DataProtectionClipboardDistilledURLTest, IsSearchWithAllowed_Warn) {
  SetWarnCopyingFromSourceURLRule();
  EXPECT_TRUE(IsSearchWithAllowed(test_web_contents_.get()));
}

TEST_F(DataProtectionClipboardDistilledURLTest, ShouldAllowSearchWith_Block) {
  SetBlockCopyingFromSourceURLRule();
  base::test::TestFuture<void> should_allow_search_future;
  ShouldAllowSearchWith(test_web_contents_.get(), 10,
                        should_allow_search_future.GetCallback());
  EXPECT_FALSE(should_allow_search_future.IsReady());
}

TEST_F(DataProtectionClipboardDistilledURLTest, CopyTextToClipboard_Allowed) {
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  CopyTextToClipboard(test_web_contents_->GetPrimaryMainFrame(), u"test text");

  base::test::TestFuture<std::u16string> future;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/std::nullopt,
      future.GetCallback());
  EXPECT_EQ(future.Get(), u"test text");
}

TEST_F(DataProtectionClipboardDistilledURLTest, CopyTextToClipboard_Block) {
  SetBlockCopyingFromSourceURLRule();
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  CopyTextToClipboard(test_web_contents_->GetPrimaryMainFrame(), u"test text");

  base::test::TestFuture<std::u16string> future;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/std::nullopt,
      future.GetCallback());
  EXPECT_EQ(future.Get(),
            l10n_util::GetStringUTF16(
                IDS_ENTERPRISE_DATA_CONTROLS_COPY_PREVENTION_WARNING_MESSAGE));
}

}  // namespace enterprise_data_protection

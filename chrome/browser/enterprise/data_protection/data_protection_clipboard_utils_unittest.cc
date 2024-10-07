// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
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
           absl::variant<size_t, std::vector<base::FilePath>> pasted_content,
           content::RenderFrameHost* rfh,
           base::OnceCallback<void(bool)> callback));

  MOCK_METHOD4(DropIfAllowed,
               void(std::optional<ui::DataTransferEndpoint> data_src,
                    std::optional<ui::DataTransferEndpoint> data_dst,
                    std::optional<std::vector<ui::FileInfo>> filenames,
                    base::OnceClosure drop_cb));
};

content::ClipboardMetadata CopyMetadata() {
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
    scoped_features_.InitAndEnableFeature(
        data_controls::kEnableDesktopDataControls);
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
      MakeClipboardPasteData("text", "image", {}), future.GetCallback());
  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");
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
      MakeClipboardPasteData("text", "image", {}), future.GetCallback());

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
      MakeClipboardPasteData("text", "image", {}), future.GetCallback());

  testing::Mock::VerifyAndClearExpectations(&policy_controller);
  EXPECT_FALSE(future.Get());
}

TEST_F(DataProtectionPasteIfAllowedByPolicyTest,
       DataProtectionPaste_NoDestinationWebContents) {
  // Missing a destination WebContents implies the tab is gone, so null should
  // always be returned even if no DC rule is set.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      SourceEndpoint(),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://destination.com"))),
      {.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest, Default) {
  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      CopyEndpoint(GURL("https://source.com")), CopyMetadata(),
      MakeClipboardPasteData("foo", "", {}), future.GetCallback());
  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest, NoEndpoint) {
  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      content::ClipboardEndpoint(std::nullopt), CopyMetadata(),
      MakeClipboardPasteData("foo", "", {}), future.GetCallback());
  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest, StringReplacement) {
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

  content::ClipboardMetadata metadata = CopyMetadata();
  metadata.seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      copy_future;
  IsClipboardCopyAllowedByPolicy(
      CopyEndpoint(GURL("https://source.com")), CopyMetadata(),
      MakeClipboardPasteData("foo", "", {}), copy_future.GetCallback());
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
  PasteIfAllowedByPolicy(SourceEndpoint(), DestinationEndpoint(), metadata,
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
  content::ClipboardMetadata new_metadata;
  PasteIfAllowedByPolicy(SourceEndpoint(), DestinationEndpoint(), new_metadata,
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

  content::ClipboardMetadata metadata = CopyMetadata();
  metadata.seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      copy_future;
  IsClipboardCopyAllowedByPolicy(
      CopyEndpoint(GURL("https://source.com")), CopyMetadata(),
      MakeClipboardPasteData("foo", "", {}), copy_future.GetCallback());
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
  PasteIfAllowedByPolicy(NoBrowserContextSourceEndpoint(),
                         DestinationEndpoint(), metadata,
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
  content::ClipboardMetadata new_metadata;
  PasteIfAllowedByPolicy(SourceEndpoint(), DestinationEndpoint(), new_metadata,
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
       StringReplacement_MultiType) {
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

  content::ClipboardMetadata text_metadata = CopyMetadata();
  text_metadata.seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  text_metadata.format_type = ui::ClipboardFormatType::PlainTextType();

  content::ClipboardMetadata image_metadata = text_metadata;
  image_metadata.format_type = ui::ClipboardFormatType::PngType();

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      text_copy_future;
  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      image_copy_future;

  IsClipboardCopyAllowedByPolicy(
      CopyEndpoint(GURL("https://source.com")), text_metadata,
      MakeClipboardPasteData("foo", "", {}), text_copy_future.GetCallback());
  IsClipboardCopyAllowedByPolicy(
      CopyEndpoint(GURL("https://source.com")), image_metadata,
      MakeClipboardPasteData("", "bar", {}), image_copy_future.GetCallback());

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
  PasteIfAllowedByPolicy(SourceEndpoint(), DestinationEndpoint(), text_metadata,
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
  content::ClipboardMetadata new_metadata;
  PasteIfAllowedByPolicy(SourceEndpoint(), DestinationEndpoint(), new_metadata,
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

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  content::ClipboardMetadata metadata = CopyMetadata();
  IsClipboardCopyAllowedByPolicy(
      CopyEndpoint(GURL("https://random.com")), metadata,
      MakeClipboardPasteData("foo", "", {}), future.GetCallback());

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);

  content::ClipboardPasteData same_tab_data;
  ReplaceSameTabClipboardDataIfRequiredByPolicy(metadata.seqno, same_tab_data);
  EXPECT_TRUE(same_tab_data.empty());
}

TEST_F(DataProtectionIsClipboardCopyAllowedByPolicyTest, BitmapReplacement) {
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

  content::ClipboardMetadata metadata = CopyMetadata();
  metadata.seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);

  const SkBitmap kBitmap = gfx::test::CreateBitmap(3, 2);
  content::ClipboardPasteData bitmap_data;
  bitmap_data.bitmap = kBitmap;

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      copy_future;
  IsClipboardCopyAllowedByPolicy(CopyEndpoint(GURL("https://source.com")),
                                 CopyMetadata(), bitmap_data,
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
  PasteIfAllowedByPolicy(SourceEndpoint(), DestinationEndpoint(), metadata,
                         MakeClipboardPasteData("to be", "replaced", {}),
                         first_paste_future.GetCallback());

  auto first_paste_data = first_paste_future.Get();
  EXPECT_TRUE(first_paste_data);
  EXPECT_TRUE(first_paste_data->text.empty());
  EXPECT_TRUE(first_paste_data->html.empty());

  // The pasted bitmap should be in the PNG field instead of the bitmap one.
  SkBitmap pasted_bitmap;
  gfx::PNGCodec::Decode(first_paste_data->png.data(),
                        first_paste_data->png.size(), &pasted_bitmap);
  EXPECT_TRUE(gfx::BitmapsAreEqual(kBitmap, pasted_bitmap));
  EXPECT_TRUE(first_paste_data->bitmap.empty());

  // Same-tab replacement should apply to the cached bitmap as well.
  content::ClipboardPasteData same_tab_data;
  ReplaceSameTabClipboardDataIfRequiredByPolicy(metadata.seqno, same_tab_data);

  pasted_bitmap = SkBitmap();
  gfx::PNGCodec::Decode(same_tab_data.png.data(), same_tab_data.png.size(),
                        &pasted_bitmap);
  EXPECT_TRUE(gfx::BitmapsAreEqual(kBitmap, pasted_bitmap));
  EXPECT_TRUE(same_tab_data.bitmap.empty());

  // Pasting again with a new seqno implies new data in the clipboard from
  // outside of Chrome, so it should be let through without replacement when it
  // triggers no rule.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      second_paste_future;
  content::ClipboardMetadata new_metadata;
  PasteIfAllowedByPolicy(SourceEndpoint(), DestinationEndpoint(), new_metadata,
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

}  // namespace enterprise_data_protection

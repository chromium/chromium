// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_resource_manager.h"

#include <string>
#include <unordered_map>

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/icu_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace {

void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

SkBitmap GetRandomBitmap() {
  SkColor color = rand() % 0xFFFFFF + 1;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(24, 24);
  bitmap.eraseARGB(255, SkColorGetR(color), SkColorGetG(color),
                   SkColorGetB(color));
  return bitmap;
}

ui::ImageModel GetRandomImageModel() {
  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkia::CreateFrom1xBitmap(GetRandomBitmap()));
}

}  // namespace

// Tests -----------------------------------------------------------------------

class MockClipboardImageModelFactory : public ClipboardImageModelFactory {
 public:
  MockClipboardImageModelFactory() = default;
  MockClipboardImageModelFactory(const MockClipboardImageModelFactory&) =
      delete;
  MockClipboardImageModelFactory& operator=(
      const MockClipboardImageModelFactory&) = delete;
  ~MockClipboardImageModelFactory() override = default;

  MOCK_METHOD(void,
              Render,
              (const base::UnguessableToken&,
               const std::string&,
               const gfx::Size&,
               ImageModelCallback),
              (override));
  MOCK_METHOD(void, CancelRequest, (const base::UnguessableToken&), (override));
  MOCK_METHOD(void, Activate, (), (override));
  MOCK_METHOD(void, Deactivate, (), (override));
  MOCK_METHOD(void, RenderCurrentPendingRequests, (), (override));
  void OnShutdown() override {}
};

class ClipboardHistoryResourceManagerTest : public AshTestBase {
 public:
  ClipboardHistoryResourceManagerTest() = default;
  ClipboardHistoryResourceManagerTest(
      const ClipboardHistoryResourceManagerTest&) = delete;
  ClipboardHistoryResourceManagerTest& operator=(
      const ClipboardHistoryResourceManagerTest&) = delete;
  ~ClipboardHistoryResourceManagerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    clipboard_history_ =
        Shell::Get()->clipboard_history_controller()->history();
    resource_manager_ =
        Shell::Get()->clipboard_history_controller()->resource_manager();
    mock_image_factory_ =
        std::make_unique<testing::StrictMock<MockClipboardImageModelFactory>>();
  }

  const ClipboardHistory* clipboard_history() const {
    return clipboard_history_;
  }

  const ClipboardHistoryResourceManager* resource_manager() {
    return resource_manager_;
  }

  MockClipboardImageModelFactory* mock_image_factory() {
    return mock_image_factory_.get();
  }

 private:
  const ClipboardHistory* clipboard_history_;
  const ClipboardHistoryResourceManager* resource_manager_;
  std::unique_ptr<MockClipboardImageModelFactory> mock_image_factory_;
};

TEST_F(ClipboardHistoryResourceManagerTest, GetLabel) {
  base::test::ScopedRestoreICUDefaultLocale locale("en_US");

  // Populate a builder with all the data formats that we expect to handle.
  ClipboardHistoryItemBuilder builder;
  builder.SetText("Text")
      .SetMarkup("HTML with no image or table tags")
      .SetRtf("Rtf")
      .SetFilenames({ui::FileInfo(base::FilePath("/path/to/File.txt"),
                                  base::FilePath("File.txt")),
                     ui::FileInfo(base::FilePath("/path/to/Other%20File.txt"),
                                  base::FilePath("Other%20File.txt"))})
      .SetBookmarkTitle("Bookmark Title")
      .SetPng(gfx::test::CreatePNGBytes(10))
      .SetFileSystemData({u"/path/to/File.txt", u"/path/to/Other%20File.txt"})
      .SetWebSmartPaste(true);

  // PNG data always take precedence.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()), u"Image");

  builder.ClearPng();

  // In the absence of PNG data, HTML data takes precedence, but we use
  // plain-text format for the label.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()), u"Text");

  builder.ClearText();

  // If plan-text does not exist, we show a placeholder label.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()), u"HTML Content");

  builder.SetText("Text");

  builder.ClearMarkup();

  // In the absence of markup data, text data takes precedence.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()), u"Text");

  builder.ClearText();

  // In the absence of HTML data, RTF data takes precedence.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()), u"RTF Content");

  builder.ClearRtf();

  // In the absence of RTF data, Filenames data takes precedence.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()),
            u"File.txt, Other File.txt");

  builder.ClearFilenames();

  // In the absence of RTF data, bookmark data takes precedence.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()), u"Bookmark Title");

  builder.ClearBookmarkTitle();

  // In the absence of bookmark data, web smart paste data takes precedence.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()),
            u"Web Smart Paste Content");

  builder.ClearWebSmartPaste();

  // In the absence of web smart paste data, file system data takes precedence.
  // NOTE: File system data is the only kind of custom data currently supported.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()),
            u"File.txt, Other File.txt");
}

// Tests that Render is called once when an eligible <img> is added
// to ClipboardHistory.
TEST_F(ClipboardHistoryResourceManagerTest, BasicImgCachedImageModel) {
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(testing::WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(1);

  // Write a basic ClipboardData which is eligible to render HTML.
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img test>", "source_url");
  }

  FlushMessageLoop();

  EXPECT_EQ(expected_image_model, resource_manager()->GetImageModel(
                                      clipboard_history()->GetItems().front()));
}

// Tests that Render is called once when an eligible <table> is added
// to ClipboardHistory.
TEST_F(ClipboardHistoryResourceManagerTest, BasicTableCachedImageModel) {
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(testing::WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(1);

  // Write a basic ClipboardData which is eligible to render HTML.
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<table test>", "source_url");
  }

  FlushMessageLoop();

  EXPECT_EQ(expected_image_model, resource_manager()->GetImageModel(
                                      clipboard_history()->GetItems().front()));
}

// Tests that Render is not called when ineligble html is added to
// ClipboarHistory
TEST_F(ClipboardHistoryResourceManagerTest, BasicIneligibleCachedImageModel) {
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(testing::WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(0);

  // Write a basic ClipboardData which is eligible to render HTML.
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"html with no img or table tag", "source_url");
  }

  FlushMessageLoop();
}

// Tests that copying duplicate HTML to the buffer results in only one render
// request, and that that request is canceled once when the item is forgotten.
TEST_F(ClipboardHistoryResourceManagerTest, DuplicateHTML) {
  // Write two duplicate ClipboardDatas. Two things should be in clipboard
  // history, but they should share a CachedImageModel.
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(testing::WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(1);

  // Use identical markup but differing source url so that both items are added
  // to the clipboard history.
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img test>", "source_url_1");
  }
  FlushMessageLoop();
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img test>", "source_url_2");
  }
  FlushMessageLoop();

  auto items = clipboard_history()->GetItems();
  EXPECT_EQ(2u, items.size());
  for (const auto& item : items)
    EXPECT_EQ(expected_image_model, resource_manager()->GetImageModel(item));
}

// Tests that two different eligible ClipboardData copied results in two calls
// to Render and Cancel.
TEST_F(ClipboardHistoryResourceManagerTest, DifferentHTML) {
  // Write two ClipboardData with different HTML.
  ui::ImageModel first_expected_image_model = GetRandomImageModel();
  ui::ImageModel second_expected_image_model = GetRandomImageModel();
  std::deque<ui::ImageModel> expected_image_models{first_expected_image_model,
                                                   second_expected_image_model};
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(testing::WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_models.front());
            expected_image_models.pop_front();
          }));
  EXPECT_CALL(*mock_image_factory(), Render).Times(2);
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img test>", "source_url");
  }
  FlushMessageLoop();
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img different>", "source_url");
  }
  FlushMessageLoop();

  std::list<ClipboardHistoryItem> items = clipboard_history()->GetItems();
  EXPECT_EQ(2u, items.size());
  EXPECT_EQ(second_expected_image_model,
            resource_manager()->GetImageModel(items.front()));
  items.pop_front();
  EXPECT_EQ(first_expected_image_model,
            resource_manager()->GetImageModel(items.front()));
}

// Tests that items that are ineligible for CachedImageModels (items with image
// representations, or no markup) do not request Render.
TEST_F(ClipboardHistoryResourceManagerTest, IneligibleItem) {
  // Write a ClipboardData with an image, no CachedImageModel should be created.
  EXPECT_CALL(*mock_image_factory(), Render).Times(0);
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"test", "source_url");
    scw.WriteImage(GetRandomBitmap());
  }
  FlushMessageLoop();

  EXPECT_EQ(1u, clipboard_history()->GetItems().size());

  // Write a ClipboardData with no markup and no image. No CachedImageModel
  // should be created.
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);

    scw.WriteText(u"test");

    scw.WriteRTF("rtf");

    scw.WriteBookmark(u"bookmark_title", "test_url");
  }
  FlushMessageLoop();

  EXPECT_EQ(2u, clipboard_history()->GetItems().size());
}

}  // namespace ash

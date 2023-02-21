// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_resource_manager.h"

#include <string>

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/repeating_test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/image/image_skia.h"

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
  ClipboardHistoryResourceManagerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
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

// Tests that an image model is rendered when HTML with an <img> tag is copied.
TEST_F(ClipboardHistoryResourceManagerTest, BasicImgCachedImageModel) {
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(testing::WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(1);

  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img test>", "source_url",
                  ui::ClipboardContentType::kSanitized);
  }
  FlushMessageLoop();

  ASSERT_EQ(clipboard_history()->GetItems().size(), 1u);
  const auto& item = clipboard_history()->GetItems().front();
  ASSERT_TRUE(item.html_preview().has_value());
  EXPECT_EQ(item.html_preview().value(), expected_image_model);
}

// Tests that an image model is rendered when HTML with a <table> tag is copied.
TEST_F(ClipboardHistoryResourceManagerTest, BasicTableCachedImageModel) {
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(testing::WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(1);

  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<table test>", "source_url",
                  ui::ClipboardContentType::kSanitized);
  }
  FlushMessageLoop();

  ASSERT_EQ(clipboard_history()->GetItems().size(), 1u);
  const auto& item = clipboard_history()->GetItems().front();
  ASSERT_TRUE(item.html_preview().has_value());
  EXPECT_EQ(item.html_preview().value(), expected_image_model);
}

// Tests that an image model is not rendered when HTML without render-eligible
// tags is copied.
TEST_F(ClipboardHistoryResourceManagerTest, BasicIneligibleCachedImageModel) {
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(testing::WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(0);

  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"HTML with no img or table tag", "source_url",
                  ui::ClipboardContentType::kSanitized);
  }
  FlushMessageLoop();

  ASSERT_EQ(clipboard_history()->GetItems().size(), 1u);
  EXPECT_FALSE(
      clipboard_history()->GetItems().front().html_preview().has_value());
}

// Tests that copying duplicate HTML to the buffer results in only one render
// request.
TEST_F(ClipboardHistoryResourceManagerTest, DuplicateHTML) {
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(testing::WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(1);

  // Write identical markup from two different source URLs so that both items
  // are added to the clipboard history.
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img test>", "source_url_1",
                  ui::ClipboardContentType::kSanitized);
  }
  FlushMessageLoop();

  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img test>", "source_url_2",
                  ui::ClipboardContentType::kSanitized);
  }
  FlushMessageLoop();

  // Because the HTML for the two items renders to the same image, we should
  // only try to render one time.
  auto items = clipboard_history()->GetItems();
  EXPECT_EQ(items.size(), 2u);
  for (const auto& item : items) {
    ASSERT_TRUE(item.html_preview().has_value());
    EXPECT_EQ(item.html_preview().value(), expected_image_model);
  }
}

// Tests that copying different HTML items results in each one being rendered.
TEST_F(ClipboardHistoryResourceManagerTest, DifferentHTML) {
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
    scw.WriteHTML(u"<img test>", "source_url",
                  ui::ClipboardContentType::kSanitized);
  }
  FlushMessageLoop();

  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img different>", "source_url",
                  ui::ClipboardContentType::kSanitized);
  }
  FlushMessageLoop();

  std::list<ClipboardHistoryItem> items = clipboard_history()->GetItems();
  ASSERT_EQ(items.size(), 2u);
  ASSERT_TRUE(items.front().html_preview().has_value());
  EXPECT_EQ(items.front().html_preview().value(), second_expected_image_model);

  items.pop_front();
  ASSERT_TRUE(items.front().html_preview().has_value());
  EXPECT_EQ(items.front().html_preview().value(), first_expected_image_model);
}

// Tests that copying content with non-HTML display formats does not result in
// any render requests.
TEST_F(ClipboardHistoryResourceManagerTest, IneligibleDisplayTypes) {
  EXPECT_CALL(*mock_image_factory(), Render).Times(0);
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);

  // Write clipboard data with what would otherwise be render-eligible markup,
  // alongside an image. The image data format takes higher precedence, so no
  // image model should be rendered.
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img test>", "source_url",
                  ui::ClipboardContentType::kSanitized);
    scw.WriteImage(GetRandomBitmap());
  }
  FlushMessageLoop();

  ASSERT_EQ(clipboard_history()->GetItems().size(), 1u);
  EXPECT_FALSE(
      clipboard_history()->GetItems().front().html_preview().has_value());

  // Write clipboard data without an HTML format. No image model should be
  // rendered.
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(u"test");
    scw.WriteRTF("rtf");
    scw.WriteBookmark(u"bookmark_title", "test_url");
  }
  FlushMessageLoop();

  ASSERT_EQ(clipboard_history()->GetItems().size(), 2u);
  EXPECT_FALSE(
      clipboard_history()->GetItems().front().html_preview().has_value());
}

// Tests that a placeholder image model is cached while rendering is ongoing.
TEST_F(ClipboardHistoryResourceManagerTest, PlaceholderDuringRender) {
  constexpr const auto kRenderDelay = base::Seconds(1);
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(testing::WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            // Delay the processing of the rendered image until after the
            // clipboard history item has been created.
            base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), expected_image_model),
                kRenderDelay);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(1);

  base::test::RepeatingTestFuture<bool> operation_confirmed_future_;
  Shell::Get()
      ->clipboard_history_controller()
      ->set_confirmed_operation_callback_for_test(
          operation_confirmed_future_.GetCallback());

  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img test>", "source_url",
                  ui::ClipboardContentType::kSanitized);
  }

  // Wait for the clipboard history item to be created. This allows us to check
  // for the item's intermediate placeholder image model.
  EXPECT_TRUE(operation_confirmed_future_.Take());

  // Between the time a clipboard history item is first created and the time its
  // image model finishes rendering, it should have a placeholder HTML preview.
  ASSERT_EQ(clipboard_history()->GetItems().size(), 1u);
  const auto& item = clipboard_history()->GetItems().front();
  ASSERT_TRUE(item.html_preview().has_value());
  EXPECT_NE(item.html_preview().value(), expected_image_model);
  EXPECT_EQ(item.html_preview().value(),
            clipboard_history_util::GetHtmlPreviewPlaceholder());

  // Allow the resource manager to process the rendered image model.
  task_environment()->FastForwardBy(kRenderDelay);
  FlushMessageLoop();

  // After the resource manager processes the rendered image, it should be
  // cached in the clipboard history item.
  ASSERT_TRUE(item.html_preview().has_value());
  EXPECT_EQ(item.html_preview().value(), expected_image_model);
}

}  // namespace ash

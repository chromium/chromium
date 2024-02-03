// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_resource_manager.h"

#include <string>

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_url_title_fetcher.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

using ::testing::_;
using ::testing::Bool;
using ::testing::StrictMock;
using ::testing::WithArg;
using ::testing::WithParamInterface;

class MockClipboardImageModelFactory : public ClipboardImageModelFactory {
 public:
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

class MockClipboardHistoryUrlTitleFetcher
    : public ClipboardHistoryUrlTitleFetcher {
 public:
  MOCK_METHOD(void,
              QueryHistory,
              (const GURL& url, OnHistoryQueryCompleteCallback callback),
              (override));
};

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

class ClipboardHistoryResourceManagerTest : public AshTestBase {
 public:
  ClipboardHistoryResourceManagerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ClipboardHistoryResourceManagerTest(
      const ClipboardHistoryResourceManagerTest&) = delete;
  ClipboardHistoryResourceManagerTest& operator=(
      const ClipboardHistoryResourceManagerTest&) = delete;
  ~ClipboardHistoryResourceManagerTest() override = default;

  // AshTestBase::
  void SetUp() override {
    AshTestBase::SetUp();
    clipboard_history_ =
        Shell::Get()->clipboard_history_controller()->history();
    resource_manager_ =
        Shell::Get()->clipboard_history_controller()->resource_manager();
    mock_image_factory_ =
        std::make_unique<StrictMock<MockClipboardImageModelFactory>>();
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
  raw_ptr<const ClipboardHistory, DanglingUntriaged> clipboard_history_;
  raw_ptr<const ClipboardHistoryResourceManager, DanglingUntriaged>
      resource_manager_;
  std::unique_ptr<MockClipboardImageModelFactory> mock_image_factory_;
};

// Tests that an image model is rendered when HTML with an <img> tag is copied.
TEST_F(ClipboardHistoryResourceManagerTest, BasicImgCachedImageModel) {
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(1);

  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img test>", "source_url");
  }
  FlushMessageLoop();

  ASSERT_EQ(clipboard_history()->GetItems().size(), 1u);
  const auto& item = clipboard_history()->GetItems().front();
  ASSERT_TRUE(item.display_image().has_value());
  EXPECT_EQ(item.display_image().value(), expected_image_model);
}

// Tests that an image model is rendered when HTML with a <table> tag is copied.
TEST_F(ClipboardHistoryResourceManagerTest, BasicTableCachedImageModel) {
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(1);

  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<table test>", "source_url");
  }
  FlushMessageLoop();

  ASSERT_EQ(clipboard_history()->GetItems().size(), 1u);
  const auto& item = clipboard_history()->GetItems().front();
  ASSERT_TRUE(item.display_image().has_value());
  EXPECT_EQ(item.display_image().value(), expected_image_model);
}

// Tests that an image model is not rendered when HTML without render-eligible
// tags is copied.
TEST_F(ClipboardHistoryResourceManagerTest, BasicIneligibleCachedImageModel) {
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(0);

  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"HTML with no img or table tag", "source_url");
  }
  FlushMessageLoop();

  ASSERT_EQ(clipboard_history()->GetItems().size(), 1u);
  EXPECT_FALSE(
      clipboard_history()->GetItems().front().display_image().has_value());
}

// Tests that copying duplicate HTML to the buffer results in only one render
// request.
TEST_F(ClipboardHistoryResourceManagerTest, DuplicateHTML) {
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(WithArg<3>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(1);

  // Write identical markup from two different source URLs so that both items
  // are added to the clipboard history.
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

  // Because the HTML for the two items renders to the same image, we should
  // only try to render one time.
  auto items = clipboard_history()->GetItems();
  EXPECT_EQ(items.size(), 2u);
  for (const auto& item : items) {
    ASSERT_TRUE(item.display_image().has_value());
    EXPECT_EQ(item.display_image().value(), expected_image_model);
  }
}

// Tests that copying different HTML items results in each one being rendered.
TEST_F(ClipboardHistoryResourceManagerTest, DifferentHTML) {
  ui::ImageModel first_expected_image_model = GetRandomImageModel();
  ui::ImageModel second_expected_image_model = GetRandomImageModel();
  std::deque<ui::ImageModel> expected_image_models{first_expected_image_model,
                                                   second_expected_image_model};
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(WithArg<3>(
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
  ASSERT_EQ(items.size(), 2u);
  ASSERT_TRUE(items.front().display_image().has_value());
  EXPECT_EQ(items.front().display_image().value(), second_expected_image_model);

  items.pop_front();
  ASSERT_TRUE(items.front().display_image().has_value());
  EXPECT_EQ(items.front().display_image().value(), first_expected_image_model);
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
    scw.WriteHTML(u"<img test>", "source_url");
    scw.WriteImage(GetRandomBitmap());
  }
  FlushMessageLoop();

  // There should be a display image for the bitmap, but no render request
  // should have been issued.
  ASSERT_EQ(clipboard_history()->GetItems().size(), 1u);
  EXPECT_TRUE(
      clipboard_history()->GetItems().front().display_image().has_value());

  // Write clipboard data without an HTML format. No image model should be
  // rendered.
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(u"test");
    scw.WriteRTF("rtf");
    scw.WriteBookmark(u"bookmark_title", "test_url");
  }
  FlushMessageLoop();

  // There should be neither a display image nor any issued render request.
  ASSERT_EQ(clipboard_history()->GetItems().size(), 2u);
  EXPECT_FALSE(
      clipboard_history()->GetItems().front().display_image().has_value());
}

// Tests that a placeholder image model is cached while rendering is ongoing.
TEST_F(ClipboardHistoryResourceManagerTest, PlaceholderDuringRender) {
  constexpr const auto kRenderDelay = base::Seconds(1);
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(WithArg<3>(
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

  base::test::TestFuture<bool> operation_confirmed_future_;
  Shell::Get()
      ->clipboard_history_controller()
      ->set_confirmed_operation_callback_for_test(
          operation_confirmed_future_.GetRepeatingCallback());

  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img test>", "source_url");
  }

  // Wait for the clipboard history item to be created. This allows us to check
  // for the item's intermediate placeholder image model.
  EXPECT_TRUE(operation_confirmed_future_.Take());

  // Between the time a clipboard history item is first created and the time its
  // image model finishes rendering, it should have a placeholder HTML preview.
  ASSERT_EQ(clipboard_history()->GetItems().size(), 1u);
  const auto& item = clipboard_history()->GetItems().front();
  ASSERT_TRUE(item.display_image().has_value());
  EXPECT_NE(item.display_image().value(), expected_image_model);
  EXPECT_EQ(item.display_image().value(),
            clipboard_history_util::GetHtmlPreviewPlaceholder());

  // Allow the resource manager to process the rendered image model.
  task_environment()->FastForwardBy(kRenderDelay);
  FlushMessageLoop();

  // After the resource manager processes the rendered image, it should be
  // cached in the clipboard history item.
  ASSERT_TRUE(item.display_image().has_value());
  EXPECT_EQ(item.display_image().value(), expected_image_model);
}

// Base class for `ClipboardHistoryMenuResourceManager` tests parameterized by
// whether the clipboard history URL titles feature is enabled.
class ClipboardHistoryResourceManagerUrlTitlesTest
    : public ClipboardHistoryResourceManagerTest,
      public WithParamInterface</*enable_url_titles=*/bool> {
 public:
  ClipboardHistoryResourceManagerUrlTitlesTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{chromeos::features::kClipboardHistoryRefresh,
          IsClipboardHistoryUrlTitlesEnabled()},
         {features::kClipboardHistoryUrlTitles,
          IsClipboardHistoryUrlTitlesEnabled()},
         {chromeos::features::kJelly, IsClipboardHistoryUrlTitlesEnabled()}});
  }

  // ClipboardHistoryResourceManagerTest:
  void SetUp() override {
    ClipboardHistoryResourceManagerTest::SetUp();
    Shell::Get()
        ->clipboard_history_controller()
        ->set_confirmed_operation_callback_for_test(
            operation_confirmed_future_.GetRepeatingCallback());
  }

  void WriteTextToClipboardAndConfirm(const std::u16string& str) {
    EXPECT_FALSE(operation_confirmed_future_.IsReady());
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteText(str);
    }
    EXPECT_TRUE(operation_confirmed_future_.Take());
  }

  bool IsClipboardHistoryUrlTitlesEnabled() const { return GetParam(); }

  StrictMock<MockClipboardHistoryUrlTitleFetcher>& mock_url_title_fetcher() {
    return mock_url_title_fetcher_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  StrictMock<MockClipboardHistoryUrlTitleFetcher> mock_url_title_fetcher_;
  base::test::TestFuture<bool> operation_confirmed_future_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClipboardHistoryResourceManagerUrlTitlesTest,
                         /*enable_url_titles=*/Bool());

// Verifies the value of clipboard history text items' secondary display text
// based on whether their display text is a URL, whether the URL title fetcher
// finds a title for the URL, and what that title is.
TEST_P(ClipboardHistoryResourceManagerUrlTitlesTest, SecondaryDisplayText) {
  struct {
    const std::u16string text;
    const std::optional<std::u16string> returned_title;
    const std::optional<std::u16string> expected_secondary_display_text;
  } test_cases[]{
      // Test that copying a visited URL sets the item's secondary display text
      // with the page's title.
      {u"https://visited.com", u"Title", u"Title"},
      // Test that a visited URL's page title has its whitespace trimmed before
      // being set as an item's secondary display text.
      {u"https://visited.com", u" Title ", u"Title"},
      // Test that a whitespace-only title is not treated as text an item should
      // display.
      {u"https://visited.com", u" ", std::nullopt},
      // Test that copying an unvisited URL triggers a history query but does
      // not set the item's secondary display text.
      {u"https://unvisited.com", std::nullopt, std::nullopt},
      // Test that copying non-URL text does not trigger a history query or set
      // the item's secondary display text.
      {u"Not a URL", std::nullopt, std::nullopt},
  };

  for (const auto& [text, returned_title, expected_secondary_display_text] :
       test_cases) {
    const GURL url(text);
    const bool should_fetch_title =
        IsClipboardHistoryUrlTitlesEnabled() && url.is_valid();

    EXPECT_CALL(mock_url_title_fetcher(), QueryHistory(url, _))
        .Times(should_fetch_title ? 1 : 0)
        .WillOnce(base::test::RunOnceCallback<1>(returned_title));

    WriteTextToClipboardAndConfirm(text);
    ASSERT_FALSE(clipboard_history()->IsEmpty());
    const auto& item = clipboard_history()->GetItems().front();
    EXPECT_EQ(
        item.secondary_display_text(),
        should_fetch_title ? expected_secondary_display_text : std::nullopt);
  }
}

}  // namespace ash

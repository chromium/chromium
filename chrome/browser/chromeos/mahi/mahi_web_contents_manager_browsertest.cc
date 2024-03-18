// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_web_contents_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/mahi/mahi_browser_util.h"
#include "chrome/browser/chromeos/mahi/test/fake_mahi_web_contents_manager.h"
#include "chrome/browser/chromeos/mahi/test/mock_mahi_crosapi.h"
#include "chrome/browser/chromeos/mahi/test/scoped_mahi_web_contents_manager_for_testing.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/test/scoped_feature_list.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace mahi {

namespace {

// Fake context menu click action.
constexpr int64_t kDisplayID = 1;
constexpr ButtonType kButtonType = ButtonType::kQA;
constexpr char16_t kQuestion[] = u"dump question";

// Fake web content.
constexpr char kUrl[] = "data:text/html,<p>kittens!</p>";

}  // namespace

class MahiWebContentsManagerBrowserTest : public InProcessBrowserTest {
 public:
  MahiWebContentsManagerBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    scoped_feature_list_.InitWithFeatures({chromeos::features::kMahi}, {});
#endif
  }
  ~MahiWebContentsManagerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // If `MahiBrowserDelegate` interface is not available on ash-chrome, this
    // test suite will no-op.
    if (!IsServiceAvailable()) {
      return;
    }
#endif

    fake_mahi_web_contents_manager_.Initialize();
    scoped_mahi_web_contents_manager_ =
        std::make_unique<ScopedMahiWebContentsManagerForTesting>(
            &fake_mahi_web_contents_manager_);

// Replace the production Mahi browser delegate with a mock for testing
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    fake_mahi_web_contents_manager_.BindMahiBrowserDelegateForTesting(
        receiver_.BindNewPipeAndPassRemote());
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
    fake_mahi_web_contents_manager_.SetMahiBrowserDelegateForTesting(
        &browser_delegate_);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    crosapi::mojom::BrowserInitParamsPtr init_params =
        chromeos::BrowserInitParams::GetForTests()->Clone();
    init_params->is_mahi_enabled = true;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
  }

  bool IsServiceAvailable() const {
    chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
    return lacros_service &&
           lacros_service->IsAvailable<crosapi::mojom::MahiBrowserDelegate>();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Simulates opening a new tab with url.
  void CreateWebContent() {
    // Simulates chrome open.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/")));
    // Then navigates to the target page.
    EXPECT_TRUE(AddTabAtIndex(0, GURL(kUrl), ui::PAGE_TRANSITION_TYPED));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif

  testing::StrictMock<MockMahiCrosapi> browser_delegate_;
  mojo::Receiver<crosapi::mojom::MahiBrowserDelegate> receiver_{
      &browser_delegate_};

  FakeMahiWebContentsManager fake_mahi_web_contents_manager_;
  std::unique_ptr<ScopedMahiWebContentsManagerForTesting>
      scoped_mahi_web_contents_manager_;
};

IN_PROC_BROWSER_TEST_F(MahiWebContentsManagerBrowserTest,
                       OnContextMenuClicked) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If `MahiBrowserDelegate` interface is not available on ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Initially, the focused state and the requested state should be different.
  EXPECT_NE(
      fake_mahi_web_contents_manager_.focused_web_content_state().page_id,
      fake_mahi_web_contents_manager_.requested_web_content_state().page_id);
  base::UnguessableToken focused_page_id =
      fake_mahi_web_contents_manager_.focused_web_content_state().page_id;

  base::RunLoop run_loop;
  // Expects that `MahiBrowserDelegate` should receive the context menu click
  // action.
  EXPECT_CALL(browser_delegate_, OnContextMenuClicked)
      .WillOnce([&run_loop](crosapi::mojom::MahiContextMenuRequestPtr request,
                            base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(kDisplayID, request->display_id);
        EXPECT_EQ(MatchButtonTypeToActionType(kButtonType),
                  request->action_type);
        EXPECT_EQ(kQuestion, request->question);
        std::move(callback).Run(/*success=*/true);
        run_loop.Quit();
      });

  fake_mahi_web_contents_manager_.OnContextMenuClicked(kDisplayID, kButtonType,
                                                       kQuestion);
  run_loop.Run();

  // After the context menu request, the requested state should be updated to
  // the focused state and the focused state stays the same.
  EXPECT_EQ(
      focused_page_id,
      fake_mahi_web_contents_manager_.requested_web_content_state().page_id);
  EXPECT_EQ(
      focused_page_id,
      fake_mahi_web_contents_manager_.focused_web_content_state().page_id);
  EXPECT_EQ(GURL(),
            fake_mahi_web_contents_manager_.focused_web_content_state().url);
  EXPECT_EQ(u"",
            fake_mahi_web_contents_manager_.focused_web_content_state().title);
}

IN_PROC_BROWSER_TEST_F(MahiWebContentsManagerBrowserTest,
                       OpenNewPageToChangePageFocus) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If `MahiBrowserDelegate` interface is not available on ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Initially, the focused state and the requested state should be different.
  EXPECT_NE(
      fake_mahi_web_contents_manager_.focused_web_content_state().page_id,
      fake_mahi_web_contents_manager_.requested_web_content_state().page_id);
  // Initially, the focused state's favicon is empty.
  EXPECT_TRUE(fake_mahi_web_contents_manager_.focused_web_content_state()
                  .favicon.isNull());

  base::RunLoop run_loop;
  // Expects that `MahiBrowserDelegate` should receive the focused page change.
  EXPECT_CALL(browser_delegate_, OnFocusedPageChanged)
      // When browser opens with `chrome://newtab`, we should be notified to
      // clear the previous focus info.
      .WillOnce([](crosapi::mojom::MahiPageInfoPtr page_info,
                   base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(GURL(), page_info->url);
        EXPECT_FALSE(page_info->IsDistillable.has_value());
        std::move(callback).Run(/*success=*/true);
      })
      // When a new page gets focus, the `MahiBrowserDelegate` should be
      // notified without the distillability check.
      .WillOnce([](crosapi::mojom::MahiPageInfoPtr page_info,
                   base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(GURL(kUrl), page_info->url);
        EXPECT_FALSE(page_info->IsDistillable.has_value());
        std::move(callback).Run(/*success=*/true);
      })
      // When the focused page finishes loading, the `MahiBrowserDelegate`
      // should be notified with the distillability check.
      .WillOnce([&run_loop](crosapi::mojom::MahiPageInfoPtr page_info,
                            base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(GURL(kUrl), page_info->url);
        EXPECT_TRUE(page_info->IsDistillable.has_value());
        EXPECT_FALSE(page_info->IsDistillable.value());
        // The favicon is not empty.
        EXPECT_FALSE(page_info->favicon_image.isNull());

        std::move(callback).Run(/*success=*/true);
        run_loop.Quit();
      });

  CreateWebContent();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(MahiWebContentsManagerBrowserTest, GetPageContents) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If `MahiBrowserDelegate` interface is not available on ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Initially, the focused state and the requested state should be different.
  EXPECT_NE(
      fake_mahi_web_contents_manager_.focused_web_content_state().page_id,
      fake_mahi_web_contents_manager_.requested_web_content_state().page_id);
  base::UnguessableToken focused_page_id =
      fake_mahi_web_contents_manager_.focused_web_content_state().page_id;

  // First create a web page so there is a place to extract the contents from.
  base::RunLoop run_loop;
  EXPECT_CALL(browser_delegate_, OnFocusedPageChanged)
      .WillOnce([](crosapi::mojom::MahiPageInfoPtr page_info,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(/*success=*/true);
      })
      .WillOnce([](crosapi::mojom::MahiPageInfoPtr page_info,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(/*success=*/true);
      })
      .WillOnce([&run_loop, &focused_page_id, this](
                    crosapi::mojom::MahiPageInfoPtr page_info,
                    base::OnceCallback<void(bool)> callback) {
        EXPECT_TRUE(page_info->IsDistillable.has_value());
        EXPECT_FALSE(page_info->IsDistillable.value());
        std::move(callback).Run(/*success=*/true);

        // Gets the page id of the newly opened page.
        focused_page_id = page_info->page_id;
        // When distillability check is returned, simulates the content request
        // from the mahi manager.
        fake_mahi_web_contents_manager_.RequestContentFromPage(
            focused_page_id,
            base::BindLambdaForTesting(
                [&](crosapi::mojom::MahiPageContentPtr page_content) {
                  run_loop.Quit();
                }));
      });
  CreateWebContent();
  run_loop.Run();

  // After the content request, the requested state should be updated to the
  // focused state and the focused state stays the same.
  EXPECT_EQ(
      focused_page_id,
      fake_mahi_web_contents_manager_.requested_web_content_state().page_id);
  EXPECT_EQ(
      focused_page_id,
      fake_mahi_web_contents_manager_.focused_web_content_state().page_id);
  EXPECT_EQ(GURL(kUrl),
            fake_mahi_web_contents_manager_.focused_web_content_state().url);
  EXPECT_EQ(u"data:text/html,<p>kittens!</p>",
            fake_mahi_web_contents_manager_.focused_web_content_state().title);
}

}  // namespace mahi

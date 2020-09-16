// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/extension_action/test_extension_action_api_observer.h"
#include "chrome/browser/extensions/api/extension_action/test_icon_image_observer.h"
#include "chrome/browser/extensions/extension_action_icon_factory.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/scoped_worker_based_extensions_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"

using content::WebContents;

namespace extensions {
namespace {

void ExecuteExtensionAction(Browser* browser, const Extension* extension) {
  ExtensionActionRunner::GetForWebContents(
      browser->tab_strip_model()->GetActiveWebContents())
      ->RunAction(extension, true);
}

const char kEmptyImageDataError[] =
    "The imageData property must contain an ImageData object or dictionary "
    "of ImageData objects.";
const char kEmptyPathError[] = "The path property must not be empty.";

// Makes sure |bar_rendering| has |model_icon| in the middle (there's additional
// padding that correlates to the rest of the button, and this is ignored).
void VerifyIconsMatch(const gfx::Image& bar_rendering,
                      const gfx::Image& model_icon) {
  gfx::Rect icon_portion(gfx::Point(), bar_rendering.Size());
  icon_portion.ClampToCenteredSize(model_icon.Size());

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      model_icon.AsImageSkia().GetRepresentation(1.0f).GetBitmap(),
      gfx::ImageSkiaOperations::ExtractSubset(bar_rendering.AsImageSkia(),
                                              icon_portion)
          .GetRepresentation(1.0f)
          .GetBitmap()));
}

class BrowserActionApiTest : public ExtensionApiTest {
 public:
  BrowserActionApiTest() {}
  ~BrowserActionApiTest() override {}

  void TearDownOnMainThread() override {
    // Clean up the test util first, so that any created UI properly removes
    // itself before profile destruction.
    browser_action_test_util_.reset();
    ExtensionApiTest::TearDownOnMainThread();
  }

 protected:
  ExtensionActionTestHelper* GetBrowserActionsBar() {
    if (!browser_action_test_util_)
      browser_action_test_util_ = ExtensionActionTestHelper::Create(browser());
    return browser_action_test_util_.get();
  }

  ExtensionAction* GetBrowserAction(Browser* browser,
                                    const Extension& extension) {
    ExtensionAction* extension_action =
        ExtensionActionManager::Get(browser->profile())
            ->GetExtensionAction(extension);
    return extension_action->action_type() == ActionInfo::TYPE_BROWSER
               ? extension_action
               : nullptr;
  }

 private:
  std::unique_ptr<ExtensionActionTestHelper> browser_action_test_util_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionApiTest);
};

// Canvas tests rely on the harness producing pixel output in order to read back
// pixels from a canvas element. So we have to override the setup function.
class BrowserActionApiCanvasTest : public BrowserActionApiTest {
 public:
  void SetUp() override {
    EnablePixelOutput();
    BrowserActionApiTest::SetUp();
  }
};

using ContextType = ExtensionBrowserTest::ContextType;

class BrowserActionApiLazyTest
    : public BrowserActionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  BrowserActionApiLazyTest() {
    // Service Workers are currently only available on certain channels, so set
    // the channel for those tests.
    if (GetParam() == ContextType::kServiceWorker)
      current_channel_ = std::make_unique<ScopedWorkerBasedExtensionsChannel>();
  }

  const extensions::Extension* LoadExtensionWithParamFlags(
      const base::FilePath& path) {
    int flags = kFlagEnableFileAccess;
    if (GetParam() == ContextType::kServiceWorker)
      flags |= ExtensionBrowserTest::kFlagRunAsServiceWorkerBasedExtension;
    return LoadExtensionWithFlags(path, flags);
  }

  void RunUpdateTest(base::StringPiece path, bool expect_failure) {
    ExtensionTestMessageListener ready_listener("ready", true);
    ASSERT_TRUE(embedded_test_server()->Start());
    const Extension* extension =
        LoadExtensionWithParamFlags(test_data_dir_.AppendASCII(path));
    ASSERT_TRUE(extension) << message_;
    // Test that there is a browser action in the toolbar.
    ASSERT_EQ(1, GetBrowserActionsBar()->NumberOfBrowserActions());

    ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
    ExtensionAction* action = GetBrowserAction(browser(), *extension);
    EXPECT_EQ("This is the default title.",
              action->GetTitle(ExtensionAction::kDefaultTabId));
    EXPECT_EQ(
        "", action->GetExplicitlySetBadgeText(ExtensionAction::kDefaultTabId));
    EXPECT_EQ(SkColorSetARGB(0, 0, 0, 0),
              action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));

    // Tell the extension to update the browser action state and then
    // catch the result.
    ResultCatcher catcher;
    ready_listener.Reply("update");

    if (expect_failure) {
      EXPECT_FALSE(catcher.GetNextResult());
      EXPECT_EQ("The source image could not be decoded.", catcher.message());
      return;
    }

    EXPECT_TRUE(catcher.GetNextResult());
    // Test that we received the changes.
    EXPECT_EQ("Modified", action->GetTitle(ExtensionAction::kDefaultTabId));
    EXPECT_EQ("badge", action->GetExplicitlySetBadgeText(
                           ExtensionAction::kDefaultTabId));
    EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255),
              action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<extensions::ScopedWorkerBasedExtensionsChannel>
      current_channel_;
};

IN_PROC_BROWSER_TEST_P(BrowserActionApiLazyTest, Basic) {
  ExtensionTestMessageListener ready_listener("ready", false);
  ASSERT_TRUE(embedded_test_server()->Start());
  const Extension* extension = LoadExtensionWithParamFlags(
      test_data_dir_.AppendASCII("browser_action/basics"));
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar()->NumberOfBrowserActions());

  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Open a URL in the tab, so the event handler can check the tab's
  // "url" and "title" properties.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.txt"));

  ResultCatcher catcher;
  // Simulate the browser action being clicked.
  ExecuteExtensionAction(browser(), extension);

  EXPECT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_P(BrowserActionApiLazyTest, Update) {
  ASSERT_NO_FATAL_FAILURE(RunUpdateTest("browser_action/update", false));
}

IN_PROC_BROWSER_TEST_P(BrowserActionApiLazyTest, UpdateSvg) {
  // TODO(crbug.com/1064671): Service Workers currently don't support loading
  // SVG images.
  const bool expect_failure = GetParam() == ContextType::kServiceWorker;
  ASSERT_NO_FATAL_FAILURE(
      RunUpdateTest("browser_action/update_svg", expect_failure));
}

INSTANTIATE_TEST_SUITE_P(EventPage,
                         BrowserActionApiLazyTest,
                         ::testing::Values(ContextType::kEventPage));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         BrowserActionApiLazyTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_F(BrowserActionApiCanvasTest, DynamicBrowserAction) {
  ASSERT_TRUE(RunExtensionTest("browser_action/no_icon")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

#if defined(OS_MAC)
  // We need this on mac so we don't loose 2x representations from browser icon
  // in transformations gfx::ImageSkia -> NSImage -> gfx::ImageSkia.
  std::vector<ui::ScaleFactor> supported_scale_factors;
  supported_scale_factors.push_back(ui::SCALE_FACTOR_100P);
  supported_scale_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::SetSupportedScaleFactors(supported_scale_factors);
#endif

  // We should not be creating icons asynchronously, so we don't need an
  // observer.
  ExtensionActionIconFactory icon_factory(
      profile(), extension, GetBrowserAction(browser(), *extension), nullptr);
  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar()->NumberOfBrowserActions());
  EXPECT_TRUE(GetBrowserActionsBar()->HasIcon(0));

  gfx::Image action_icon = icon_factory.GetIcon(0);
  uint32_t action_icon_last_id = action_icon.ToSkBitmap()->getGenerationID();

  // Let's check that |GetIcon| doesn't always return bitmap with new id.
  ASSERT_EQ(action_icon_last_id,
            icon_factory.GetIcon(0).ToSkBitmap()->getGenerationID());

  gfx::Image last_bar_icon = GetBrowserActionsBar()->GetIcon(0);
  EXPECT_TRUE(gfx::test::AreImagesEqual(last_bar_icon,
                                        GetBrowserActionsBar()->GetIcon(0)));

  // The reason we don't test more standard scales (like 1x, 2x, etc.) is that
  // these may be generated from the provided scales.
  float kSmallIconScale = 21.f / ExtensionAction::ActionIconSize();
  float kLargeIconScale = 42.f / ExtensionAction::ActionIconSize();
  ASSERT_FALSE(ui::IsSupportedScale(kSmallIconScale));
  ASSERT_FALSE(ui::IsSupportedScale(kLargeIconScale));

  // Tell the extension to update the icon using ImageData object.
  ResultCatcher catcher;
  GetBrowserActionsBar()->Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  EXPECT_FALSE(gfx::test::AreImagesEqual(last_bar_icon,
                                         GetBrowserActionsBar()->GetIcon(0)));
  last_bar_icon = GetBrowserActionsBar()->GetIcon(0);

  action_icon = icon_factory.GetIcon(0);
  uint32_t action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;
  VerifyIconsMatch(last_bar_icon, action_icon);

  // Check that only the smaller size was set (only a 21px icon was provided).
  EXPECT_TRUE(action_icon.ToImageSkia()->HasRepresentation(kSmallIconScale));
  EXPECT_FALSE(action_icon.ToImageSkia()->HasRepresentation(kLargeIconScale));

  // Tell the extension to update the icon using path.
  GetBrowserActionsBar()->Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  // Make sure the browser action bar updated.
  EXPECT_FALSE(gfx::test::AreImagesEqual(last_bar_icon,
                                         GetBrowserActionsBar()->GetIcon(0)));
  last_bar_icon = GetBrowserActionsBar()->GetIcon(0);

  action_icon = icon_factory.GetIcon(0);
  action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;
  VerifyIconsMatch(last_bar_icon, action_icon);

  // Check that only the smaller size was set (only a 21px icon was provided).
  EXPECT_TRUE(action_icon.ToImageSkia()->HasRepresentation(kSmallIconScale));
  EXPECT_FALSE(action_icon.ToImageSkia()->HasRepresentation(kLargeIconScale));

  // Tell the extension to update the icon using dictionary of ImageData
  // objects.
  GetBrowserActionsBar()->Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  EXPECT_FALSE(gfx::test::AreImagesEqual(last_bar_icon,
                                         GetBrowserActionsBar()->GetIcon(0)));
  last_bar_icon = GetBrowserActionsBar()->GetIcon(0);

  action_icon = icon_factory.GetIcon(0);
  action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;
  VerifyIconsMatch(last_bar_icon, action_icon);

  // Check both sizes were set (as two icon sizes were provided).
  EXPECT_TRUE(action_icon.ToImageSkia()->HasRepresentation(kSmallIconScale));
  EXPECT_TRUE(action_icon.AsImageSkia().HasRepresentation(kLargeIconScale));

  // Tell the extension to update the icon using dictionary of paths.
  GetBrowserActionsBar()->Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  EXPECT_FALSE(gfx::test::AreImagesEqual(last_bar_icon,
                                         GetBrowserActionsBar()->GetIcon(0)));
  last_bar_icon = GetBrowserActionsBar()->GetIcon(0);

  action_icon = icon_factory.GetIcon(0);
  action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;
  VerifyIconsMatch(last_bar_icon, action_icon);

  // Check both sizes were set (as two icon sizes were provided).
  EXPECT_TRUE(action_icon.ToImageSkia()->HasRepresentation(kSmallIconScale));
  EXPECT_TRUE(action_icon.AsImageSkia().HasRepresentation(kLargeIconScale));

  // Tell the extension to update the icon using dictionary of ImageData
  // objects, but setting only one size.
  GetBrowserActionsBar()->Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  EXPECT_FALSE(gfx::test::AreImagesEqual(last_bar_icon,
                                         GetBrowserActionsBar()->GetIcon(0)));
  last_bar_icon = GetBrowserActionsBar()->GetIcon(0);

  action_icon = icon_factory.GetIcon(0);
  action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;
  VerifyIconsMatch(last_bar_icon, action_icon);

  // Check that only the smaller size was set (only a 21px icon was provided).
  EXPECT_TRUE(action_icon.ToImageSkia()->HasRepresentation(kSmallIconScale));
  EXPECT_FALSE(action_icon.ToImageSkia()->HasRepresentation(kLargeIconScale));

  // Tell the extension to update the icon using dictionary of paths, but
  // setting only one size.
  GetBrowserActionsBar()->Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  EXPECT_FALSE(gfx::test::AreImagesEqual(last_bar_icon,
                                         GetBrowserActionsBar()->GetIcon(0)));
  last_bar_icon = GetBrowserActionsBar()->GetIcon(0);

  action_icon = icon_factory.GetIcon(0);
  action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;
  VerifyIconsMatch(last_bar_icon, action_icon);

  // Check that only the smaller size was set (only a 21px icon was provided).
  EXPECT_TRUE(action_icon.ToImageSkia()->HasRepresentation(kSmallIconScale));
  EXPECT_FALSE(action_icon.ToImageSkia()->HasRepresentation(kLargeIconScale));

  // Tell the extension to update the icon using dictionary of ImageData
  // objects, but setting only size 42.
  GetBrowserActionsBar()->Press(0);
  ASSERT_TRUE(catcher.GetNextResult());

  EXPECT_FALSE(gfx::test::AreImagesEqual(last_bar_icon,
                                         GetBrowserActionsBar()->GetIcon(0)));
  last_bar_icon = GetBrowserActionsBar()->GetIcon(0);

  action_icon = icon_factory.GetIcon(0);
  action_icon_current_id = action_icon.ToSkBitmap()->getGenerationID();
  EXPECT_GT(action_icon_current_id, action_icon_last_id);
  action_icon_last_id = action_icon_current_id;

  // Check that only the larger size was set (only a 42px icon was provided).
  EXPECT_FALSE(action_icon.ToImageSkia()->HasRepresentation(kSmallIconScale));
  EXPECT_TRUE(action_icon.AsImageSkia().HasRepresentation(kLargeIconScale));

  // Try setting icon with empty dictionary of ImageData objects.
  GetBrowserActionsBar()->Press(0);
  ASSERT_FALSE(catcher.GetNextResult());
  EXPECT_EQ(kEmptyImageDataError, catcher.message());

  // Try setting icon with empty dictionary of path objects.
  GetBrowserActionsBar()->Press(0);
  ASSERT_FALSE(catcher.GetNextResult());
  EXPECT_EQ(kEmptyPathError, catcher.message());
}

IN_PROC_BROWSER_TEST_F(BrowserActionApiCanvasTest, InvisibleIconBrowserAction) {
  // Turn this on so errors are reported.
  ExtensionActionSetIconFunction::SetReportErrorForInvisibleIconForTesting(
      true);
  ASSERT_TRUE(RunExtensionTest("browser_action/invisible_icon")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar()->NumberOfBrowserActions());
  EXPECT_TRUE(GetBrowserActionsBar()->HasIcon(0));
  gfx::Image initial_bar_icon = GetBrowserActionsBar()->GetIcon(0);

  ExtensionHost* background_page =
      ProcessManager::Get(profile())->GetBackgroundHostForExtension(
          extension->id());
  ASSERT_TRUE(background_page);

  static constexpr char kScript[] =
      "setIcon(%s).then(function(arg) {"
      "  domAutomationController.send(arg);"
      "});";

  const std::string histogram_name =
      "Extensions.DynamicExtensionActionIconWasVisible";
  const std::string new_histogram_name =
      "Extensions.DynamicExtensionActionIconWasVisibleRendered";
  {
    base::HistogramTester histogram_tester;
    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        background_page->host_contents(),
        base::StringPrintf(kScript, "invisibleImageData"), &result));
    EXPECT_EQ("Icon not sufficiently visible.", result);
    // The icon should not have changed.
    EXPECT_TRUE(gfx::test::AreImagesEqual(initial_bar_icon,
                                          GetBrowserActionsBar()->GetIcon(0)));
    EXPECT_THAT(histogram_tester.GetAllSamples(histogram_name),
                testing::ElementsAre(base::Bucket(0, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(new_histogram_name),
                testing::ElementsAre(base::Bucket(0, 1)));
  }

  {
    base::HistogramTester histogram_tester;
    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        background_page->host_contents(),
        base::StringPrintf(kScript, "visibleImageData"), &result));
    EXPECT_EQ("", result);
    // The icon should have changed.
    EXPECT_FALSE(gfx::test::AreImagesEqual(initial_bar_icon,
                                           GetBrowserActionsBar()->GetIcon(0)));
    EXPECT_THAT(histogram_tester.GetAllSamples(histogram_name),
                testing::ElementsAre(base::Bucket(1, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(new_histogram_name),
                testing::ElementsAre(base::Bucket(1, 1)));
  }
}

IN_PROC_BROWSER_TEST_P(BrowserActionApiLazyTest,
                       TabSpecificBrowserActionState) {
  ASSERT_TRUE(RunExtensionTest("browser_action/tab_specific_state")) <<
      message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar and that it has an icon.
  ASSERT_EQ(1, GetBrowserActionsBar()->NumberOfBrowserActions());
  EXPECT_TRUE(GetBrowserActionsBar()->HasIcon(0));

  // Execute the action, its title should change.
  ResultCatcher catcher;
  GetBrowserActionsBar()->Press(0);
  ASSERT_TRUE(catcher.GetNextResult());
  EXPECT_EQ("Showing icon 2", GetBrowserActionsBar()->GetTooltip(0));

  // Open a new tab, the title should go back.
  chrome::NewTab(browser());
  EXPECT_EQ("hi!", GetBrowserActionsBar()->GetTooltip(0));

  // Go back to first tab, changed title should reappear.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  EXPECT_EQ("Showing icon 2", GetBrowserActionsBar()->GetTooltip(0));

  // Reload that tab, default title should come back.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  EXPECT_EQ("hi!", GetBrowserActionsBar()->GetTooltip(0));
}

// Test that calling chrome.browserAction.setPopup() can enable and change
// a popup.
IN_PROC_BROWSER_TEST_P(BrowserActionApiLazyTest, BrowserActionAddPopup) {
  ASSERT_TRUE(RunExtensionTest("browser_action/add_popup")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  ExtensionAction* browser_action = GetBrowserAction(browser(), *extension);
  ASSERT_TRUE(browser_action)
      << "Browser action test extension should have a browser action.";

  ASSERT_FALSE(browser_action->HasPopup(tab_id));
  ASSERT_FALSE(browser_action->HasPopup(ExtensionAction::kDefaultTabId));

  // Simulate a click on the browser action icon.  The onClicked handler
  // will add a popup.
  {
    ResultCatcher catcher;
    GetBrowserActionsBar()->Press(0);
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // The call to setPopup in background.html set a tab id, so the
  // current tab's setting should have changed, but the default setting
  // should not have changed.
  ASSERT_TRUE(browser_action->HasPopup(tab_id))
      << "Clicking on the browser action should have caused a popup to "
      << "be added.";
  ASSERT_FALSE(browser_action->HasPopup(ExtensionAction::kDefaultTabId))
      << "Clicking on the browser action should not have set a default "
      << "popup.";

  ASSERT_STREQ("/a_popup.html",
               browser_action->GetPopupUrl(tab_id).path().c_str());

  // Now change the popup from a_popup.html to another_popup.html by loading
  // a page which removes the popup using chrome.browserAction.setPopup().
  {
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(),
        GURL(extension->GetResourceURL("change_popup.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // The call to setPopup in change_popup.html did not use a tab id,
  // so the default setting should have changed as well as the current tab.
  ASSERT_TRUE(browser_action->HasPopup(tab_id));
  ASSERT_TRUE(browser_action->HasPopup(ExtensionAction::kDefaultTabId));
  ASSERT_STREQ("/another_popup.html",
               browser_action->GetPopupUrl(tab_id).path().c_str());
}

// Test that calling chrome.browserAction.setPopup() can remove a popup.
IN_PROC_BROWSER_TEST_P(BrowserActionApiLazyTest, BrowserActionRemovePopup) {
  // Load the extension, which has a browser action with a default popup.
  ASSERT_TRUE(RunExtensionTest("browser_action/remove_popup")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  ExtensionAction* browser_action = GetBrowserAction(browser(), *extension);
  ASSERT_TRUE(browser_action)
      << "Browser action test extension should have a browser action.";

  ASSERT_TRUE(browser_action->HasPopup(tab_id))
      << "Expect a browser action popup before the test removes it.";
  ASSERT_TRUE(browser_action->HasPopup(ExtensionAction::kDefaultTabId))
      << "Expect a browser action popup is the default for all tabs.";

  // Load a page which removes the popup using chrome.browserAction.setPopup().
  {
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(),
        GURL(extension->GetResourceURL("remove_popup.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  ASSERT_FALSE(browser_action->HasPopup(tab_id))
      << "Browser action popup should have been removed.";
  ASSERT_TRUE(browser_action->HasPopup(ExtensionAction::kDefaultTabId))
      << "Browser action popup default should not be changed by setting "
      << "a specific tab id.";
}

#if defined(OS_WIN) || defined(OS_LINUX)
// Flaky: https://crbug.com/1113904
#define MAYBE_IncognitoBasic DISABLED_IncognitoBasic
#else
#define MAYBE_IncognitoBasic IncognitoBasic
#endif

IN_PROC_BROWSER_TEST_P(BrowserActionApiLazyTest, MAYBE_IncognitoBasic) {
  ExtensionTestMessageListener ready_listener("ready", false);
  ASSERT_TRUE(embedded_test_server()->Start());
  scoped_refptr<const Extension> extension = LoadExtensionWithParamFlags(
      test_data_dir_.AppendASCII("browser_action/basics"));
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar()->NumberOfBrowserActions());

  // Open an incognito window and test that the browser action isn't there by
  // default.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());

  ASSERT_EQ(0, ExtensionActionTestHelper::Create(incognito_browser)
                   ->NumberOfBrowserActions());

  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Now enable the extension in incognito mode, and test that the browser
  // action shows up.
  // SetIsIncognitoEnabled() requires a reload of the extension, so we have to
  // wait for it.
  ExtensionTestMessageListener incognito_ready_listener("ready", false);
  TestExtensionRegistryObserver registry_observer(
      ExtensionRegistry::Get(profile()), extension->id());
  extensions::util::SetIsIncognitoEnabled(
      extension->id(), browser()->profile(), true);
  extension = registry_observer.WaitForExtensionLoaded();

  ASSERT_EQ(1, ExtensionActionTestHelper::Create(incognito_browser)
                   ->NumberOfBrowserActions());

  ASSERT_TRUE(incognito_ready_listener.WaitUntilSatisfied());

  // Open a URL in the tab, so the event handler can check the tab's
  // "url" and "title" properties.
  ui_test_utils::NavigateToURL(
      incognito_browser,
      embedded_test_server()->GetURL("/extensions/test_file.txt"));

  ResultCatcher catcher;
  // Simulate the browser action being clicked.
  ExecuteExtensionAction(incognito_browser, extension.get());

  EXPECT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_P(BrowserActionApiLazyTest, IncognitoUpdate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ExtensionTestMessageListener incognito_not_allowed_listener(
      "incognito not allowed", false);
  scoped_refptr<const Extension> extension = LoadExtensionWithParamFlags(
      test_data_dir_.AppendASCII("browser_action/update"));
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(incognito_not_allowed_listener.WaitUntilSatisfied());
  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar()->NumberOfBrowserActions());

  // Open an incognito window and test that the browser action isn't there by
  // default.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());

  ASSERT_EQ(0, ExtensionActionTestHelper::Create(incognito_browser)
                   ->NumberOfBrowserActions());

  // Set up a listener so we can reply for the extension to do the update.
  // This listener also adds a sequence point between the browser and the
  // renderer for the transition between incognito mode not being allowed
  // and it's being allowed. This ensures the browser ignores the renderer's
  // execution until the transition is completed, since the script will
  // start and stop multiple times during the initial load of the extension
  // and the enabling of incognito mode.
  ExtensionTestMessageListener incognito_allowed_listener("incognito allowed",
                                                          true);
  // Now enable the extension in incognito mode, and test that the browser
  // action shows up. SetIsIncognitoEnabled() requires a reload of the
  // extension, so we have to wait for it to finish.
  TestExtensionRegistryObserver registry_observer(
      ExtensionRegistry::Get(profile()), extension->id());
  extensions::util::SetIsIncognitoEnabled(extension->id(), browser()->profile(),
                                          true);
  extension = registry_observer.WaitForExtensionLoaded();
  ASSERT_TRUE(extension);
  ASSERT_EQ(1, ExtensionActionTestHelper::Create(incognito_browser)
                   ->NumberOfBrowserActions());

  ASSERT_TRUE(incognito_allowed_listener.WaitUntilSatisfied());
  ExtensionAction* action = GetBrowserAction(incognito_browser, *extension);
  EXPECT_EQ("This is the default title.",
            action->GetTitle(ExtensionAction::kDefaultTabId));
  EXPECT_EQ("",
            action->GetExplicitlySetBadgeText(ExtensionAction::kDefaultTabId));
  EXPECT_EQ(SkColorSetARGB(0, 0, 0, 0),
            action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));
  // Tell the extension to update the browser action state and then
  // catch the result.
  ResultCatcher incognito_catcher;
  incognito_allowed_listener.Reply("incognito update");
  ASSERT_TRUE(incognito_catcher.GetNextResult());

  // Test that we received the changes.
  EXPECT_EQ("Modified", action->GetTitle(ExtensionAction::kDefaultTabId));
  EXPECT_EQ("badge",
            action->GetExplicitlySetBadgeText(ExtensionAction::kDefaultTabId));
  EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255),
            action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));
}

// Tests that events are dispatched to the correct profile for split mode
// extensions.
IN_PROC_BROWSER_TEST_P(BrowserActionApiLazyTest, IncognitoSplit) {
  ResultCatcher catcher;
  const Extension* extension = LoadExtensionWithFlags(
      test_data_dir_.AppendASCII("browser_action/split_mode"),
      kFlagEnableIncognito);
  ASSERT_TRUE(extension) << message_;

  // Open an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  ASSERT_EQ(1, ExtensionActionTestHelper::Create(incognito_browser)
                   ->NumberOfBrowserActions());

  // A click in the regular profile should open a tab in the regular profile.
  ExecuteExtensionAction(browser(), extension);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // A click in the incognito profile should open a tab in the
  // incognito profile.
  ExecuteExtensionAction(incognito_browser, extension);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, CloseBackgroundPage) {
  ExtensionTestMessageListener listener("ready", /*will_reply=*/false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("browser_action/close_background")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // There is a background page and a browser action with no badge text.
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(browser()->profile());

  ExtensionHost* extension_host =
      manager->GetBackgroundHostForExtension(extension->id());
  ASSERT_TRUE(extension_host);

  ExtensionAction* action = GetBrowserAction(browser(), *extension);
  ASSERT_EQ("",
            action->GetExplicitlySetBadgeText(ExtensionAction::kDefaultTabId));

  // A helper class to wait for the ExtensionHost to shut down.
  // TODO(devlin): Hoist this somewhere more common and track down other similar
  // usages.
  class ExtensionHostDestructionObserver : public ExtensionHostObserver {
   public:
    explicit ExtensionHostDestructionObserver(ExtensionHost* host) {
      host_observer_.Add(host);
    }
    ExtensionHostDestructionObserver(
        const ExtensionHostDestructionObserver& other) = delete;
    ExtensionHostDestructionObserver& operator=(
        const ExtensionHostDestructionObserver& other) = delete;
    ~ExtensionHostDestructionObserver() override = default;

    void OnExtensionHostDestroyed(ExtensionHost* host) override {
      ASSERT_TRUE(host_observer_.IsObserving(host));
      host_observer_.Remove(host);
      run_loop_.QuitWhenIdle();
    }

    void Wait() { run_loop_.Run(); }

   private:
    base::RunLoop run_loop_;
    ScopedObserver<ExtensionHost, ExtensionHostObserver> host_observer_{this};
  };

  ExtensionHostDestructionObserver host_destroyed_observer(extension_host);

  // Click the browser action.
  ExecuteExtensionAction(browser(), extension);

  host_destroyed_observer.Wait();

  EXPECT_FALSE(manager->GetBackgroundHostForExtension(extension->id()));
  EXPECT_EQ("X",
            action->GetExplicitlySetBadgeText(ExtensionAction::kDefaultTabId));
}

IN_PROC_BROWSER_TEST_P(BrowserActionApiLazyTest, BadgeBackgroundColor) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("browser_action/color")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar()->NumberOfBrowserActions());

  // Test that CSS values (#FF0000) set color correctly.
  ExtensionAction* action = GetBrowserAction(browser(), *extension);
  ASSERT_EQ(SkColorSetARGB(255, 255, 0, 0),
            action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));

  // Tell the extension to update the browser action state.
  ResultCatcher catcher;
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that CSS values (#0F0) set color correctly.
  ASSERT_EQ(SkColorSetARGB(255, 0, 255, 0),
            action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));

  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update2.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that array values set color correctly.
  ASSERT_EQ(SkColorSetARGB(255, 255, 255, 255),
            action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));

  ui_test_utils::NavigateToURL(browser(),
                               GURL(extension->GetResourceURL("update3.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that hsl() values 'hsl(120, 100%, 50%)' set color correctly.
  ASSERT_EQ(SkColorSetARGB(255, 0, 255, 0),
            action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));

  // Test basic color keyword set correctly.
  ui_test_utils::NavigateToURL(browser(),
                               GURL(extension->GetResourceURL("update4.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  ASSERT_EQ(SkColorSetARGB(255, 0, 0, 255),
            action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));
}

IN_PROC_BROWSER_TEST_P(BrowserActionApiLazyTest, Getters) {
  ASSERT_TRUE(RunExtensionTest("browser_action/getters")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar()->NumberOfBrowserActions());

  // Test the getters for defaults.
  ResultCatcher catcher;
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test the getters for a specific tab.
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update2.html")));
  ASSERT_TRUE(catcher.GetNextResult());
}

// Verify triggering browser action.
IN_PROC_BROWSER_TEST_P(BrowserActionApiLazyTest, TestTriggerBrowserAction) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(RunExtensionTest("trigger_actions/browser_action")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar()->NumberOfBrowserActions());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/simple.html"));

  ExtensionAction* browser_action = GetBrowserAction(browser(), *extension);
  EXPECT_TRUE(browser_action);

  // Simulate a click on the browser action icon.
  {
    ResultCatcher catcher;
    GetBrowserActionsBar()->Press(0);
    EXPECT_TRUE(catcher.GetNextResult());
  }

  WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(tab);

  // Verify that the browser action turned the background color red.
  const std::string script =
      "window.domAutomationController.send(document.body.style."
      "backgroundColor);";
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(tab, script, &result));
  EXPECT_EQ(result, "red");
}

IN_PROC_BROWSER_TEST_P(BrowserActionApiLazyTest,
                       BrowserActionWithRectangularIcon) {
  ExtensionTestMessageListener ready_listener("ready", true);

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("browser_action").AppendASCII("rect_icon"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());

  // Wait for the default icon to load before accessing the underlying
  // gfx::Image.
  TestIconImageObserver::WaitForExtensionActionIcon(extension, profile());

  gfx::Image first_icon = GetBrowserActionsBar()->GetIcon(0);
  ASSERT_FALSE(first_icon.IsEmpty());

  TestExtensionActionAPIObserver observer(profile(), extension->id());
  ResultCatcher catcher;
  ready_listener.Reply(std::string());
  EXPECT_TRUE(catcher.GetNextResult());
  // Wait for extension action to be updated.
  observer.Wait();

  gfx::Image next_icon = GetBrowserActionsBar()->GetIcon(0);
  ASSERT_FALSE(next_icon.IsEmpty());
  EXPECT_FALSE(gfx::test::AreImagesEqual(first_icon, next_icon));
}

// Verify video can enter and exit Picture-in_Picture when browser action icon
// is clicked.
IN_PROC_BROWSER_TEST_F(BrowserActionApiTest,
                       TestPictureInPictureOnBrowserActionIconClick) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ASSERT_TRUE(
      RunExtensionTest("trigger_actions/browser_action_picture_in_picture"))
      << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar()->NumberOfBrowserActions());

  ExtensionAction* browser_action = GetBrowserAction(browser(), *extension);
  EXPECT_TRUE(browser_action);

  // Find the background page.
  ProcessManager* process_manager =
      extensions::ProcessManager::Get(browser()->profile());
  content::WebContents* web_contents =
      process_manager->GetBackgroundHostForExtension(extension->id())
          ->web_contents();
  ASSERT_TRUE(web_contents);
  content::PictureInPictureWindowController* window_controller =
      content::PictureInPictureWindowController::GetOrCreateForWebContents(
          web_contents);
  ASSERT_TRUE(window_controller->GetWindowForTesting());
  EXPECT_FALSE(window_controller->GetWindowForTesting()->IsVisible());

  // Click on the browser action icon to enter Picture-in-Picture.
  ResultCatcher catcher;
  GetBrowserActionsBar()->Press(0);
  EXPECT_TRUE(catcher.GetNextResult());
  EXPECT_TRUE(window_controller->GetWindowForTesting()->IsVisible());

  // Click on the browser action icon to exit Picture-in-Picture.
  GetBrowserActionsBar()->Press(0);
  EXPECT_TRUE(catcher.GetNextResult());
  EXPECT_FALSE(window_controller->GetWindowForTesting()->IsVisible());
}

}  // namespace
}  // namespace extensions

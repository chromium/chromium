// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/stringprintf.h"
#include "chrome/browser/hid/chrome_hid_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hid/hid_chooser.h"
#include "chrome/browser/ui/hid/hid_chooser_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

constexpr int kMicroBitVendorId = 0x0d28;
constexpr int kMicroBitProductId = 0x0204;
#if BUILDFLAG(IS_WIN)
constexpr char16_t kMicroBit[] = u"CMSIS-DAP";
#elif BUILDFLAG(IS_MAC)
constexpr char16_t kMicroBit[] = u"BBC micro:bit CMSIS-DAP";
#else
constexpr char16_t kMicroBit[] = u"ARM BBC micro:bit CMSIS-DAP";
#endif  // BUILDFLAG(IS_WIN)

class RealTargetTestChooserView : public permissions::ChooserController::View {
 public:
  explicit RealTargetTestChooserView(
      std::unique_ptr<permissions::ChooserController> controller)
      : controller_(std::move(controller)) {
    controller_->set_view(this);
  }

  RealTargetTestChooserView(const RealTargetTestChooserView&) = delete;
  RealTargetTestChooserView& operator=(const RealTargetTestChooserView&) =
      delete;

  ~RealTargetTestChooserView() override { controller_->set_view(nullptr); }

  void OnOptionsInitialized() override {
    CHECK_EQ(controller_->NumOptions(), 1u);
    CHECK_EQ(controller_->GetOption(0), kMicroBit);
    controller_->Select({0});
    delete this;
  }

  void OnOptionAdded(size_t index) override {}
  void OnOptionRemoved(size_t index) override {}
  void OnOptionUpdated(size_t index) override {}
  void OnAdapterEnabledChanged(bool enabled) override {}
  void OnRefreshStateChanged(bool refreshing) override {}

 private:
  std::unique_ptr<permissions::ChooserController> controller_;
};

class HidRealTargetTestDelegate : public ChromeHidDelegate {
 public:
  HidRealTargetTestDelegate() = default;
  ~HidRealTargetTestDelegate() override = default;

  std::unique_ptr<content::HidChooser> RunChooser(
      content::RenderFrameHost* render_frame_host,
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
      content::HidChooser::Callback callback) override {
    new RealTargetTestChooserView(std::make_unique<HidChooserController>(
        render_frame_host, std::move(filters), std::move(exclusion_filters),
        std::move(callback)));
    return std::make_unique<HidChooser>(base::NullCallback());
  }
};

class TestContentBrowserClient : public content::ContentBrowserClient {
 public:
  TestContentBrowserClient()
      : hid_delegate_(std::make_unique<HidRealTargetTestDelegate>()) {}
  ~TestContentBrowserClient() override = default;

  content::HidDelegate* GetHidDelegate() override {
    return hid_delegate_.get();
  }

  void SetAsBrowserClient() { content::SetBrowserClientForTesting(this); }

 private:
  std::unique_ptr<HidRealTargetTestDelegate> hid_delegate_;
};

class HidRealTargetTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    // Note that we don't reset the original content browser client here. It is
    // because the HidService destructor might run *after* switching back to the
    // original ContentBrowserClient during test teardown. An inconsistency
    // between the test's HidDelegate and the original client's HidDelegate
    // could lead to a crash, i.e. inconsistent ContextObservation map.
    // Therefore, the test client remains active for the entire test lifetime.
    test_content_browser_client_.SetAsBrowserClient();

    GURL url = embedded_test_server()->GetURL("localhost", "/simple_page.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    content::RenderFrameHost* render_frame_host = browser()
                                                      ->tab_strip_model()
                                                      ->GetActiveWebContents()
                                                      ->GetPrimaryMainFrame();
    EXPECT_EQ(url.DeprecatedGetOriginAsURL(),
              render_frame_host->GetLastCommittedOrigin().GetURL());
  }

 private:
  TestContentBrowserClient test_content_browser_client_;
};

IN_PROC_BROWSER_TEST_F(HidRealTargetTest, OpenAndCloseDevice) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto test_script = base::StringPrintf(
      R"((async () => {
      const devices = await navigator.hid.requestDevice({
        filters: [{ vendorId: %d, productId: %d }]
      });
      let device = devices[0];
      await device.open();
      let result = device.opened;
      await device.close();
      return result;
      })())",
      kMicroBitVendorId, kMicroBitProductId);
  EXPECT_EQ(true, EvalJs(web_contents, test_script));
}

}  // namespace

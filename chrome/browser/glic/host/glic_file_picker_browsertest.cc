// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace glic {
namespace {

class GlicFilePickerBrowserTest : public GlicBrowserTest,
                                  public testing::WithParamInterface<bool> {
 public:
  GlicFilePickerBrowserTest() {
    if (IsNoWebview()) {
      scoped_feature_list_.InitAndEnableFeature(features::kGlicNoWebview);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kGlicNoWebview);
    }
  }

  bool IsNoWebview() const { return GetParam(); }

  TestResult<content::RenderFrameHost*> WaitForVisibleGuestMainFrame(
      GlicInstanceImpl* instance) {
    RETURN_IF_ERROR(WaitForGlicClient(instance));
    RETURN_IF_ERROR(WaitUntilGuestIsShowing(instance));

    content::RenderFrameHost* guest_rfh = instance->host().GetGuestMainFrame();
    if (!guest_rfh) {
      return base::unexpected("Guest main frame is null");
    }
    content::WebContents* guest_contents =
        content::WebContents::FromRenderFrameHost(guest_rfh);
    if (!guest_contents) {
      return base::unexpected("Guest WebContents is null");
    }
    RETURN_IF_ERROR(RunUntilEqual(
        [&]() { return VisibilityAsString(guest_contents->GetVisibility()); },
        VisibilityAsString(content::Visibility::VISIBLE),
        "Timeout waiting for guest WebContents visibility to be VISIBLE"));
    return guest_rfh;
  }

  void SetUpOnMainThread() override {
    GlicBrowserTest::SetUpOnMainThread();

    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_file_ = temp_dir_.GetPath().AppendASCII("test.txt");
    ASSERT_TRUE(base::WriteFile(test_file_, "hello from file picker"));

    ui::SelectFileDialog::SetFactory(
        std::make_unique<content::FakeSelectFileDialogFactory>(
            std::vector<base::FilePath>{test_file_}));
  }

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(temp_dir_.Delete());
    }
    GlicBrowserTest::TearDownOnMainThread();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath test_file_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicFilePickerBrowserTest,
                       ShowOpenFilePickerInNarrowSidePanel) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK_AND_ASSIGN(content::RenderFrameHost * guest_rfh,
                       WaitForVisibleGuestMainFrame(instance));
  ASSERT_TRUE(guest_rfh->GetView());
  // The default side panel is narrower than kMinFileChooserWindowSize (400px).
  EXPECT_LT(guest_rfh->GetView()->GetViewBounds().width(), 400);

  base::HistogramTester histogram_tester;
  constexpr char kShowOpenFilePickerScript[] = R"js(
    (async () => {
      const [handle] = await window.showOpenFilePicker({multiple: true});
      const file = await handle.getFile();
      return `${file.name}:${await file.text()}`;
    })()
  )js";

  EXPECT_EQ("test.txt:hello from file picker",
            content::EvalJs(guest_rfh, kShowOpenFilePickerScript));
  histogram_tester.ExpectUniqueSample(
      "Content.FileChooser.SuppressedByWindowSize", false, 1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_P(GlicFilePickerBrowserTest, InputFilePickerInSidePanel) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK_AND_ASSIGN(content::RenderFrameHost * guest_rfh,
                       WaitForVisibleGuestMainFrame(instance));

  constexpr char kInputFilePickerScript[] = R"js(
    (async () => {
      const input = document.createElement('input');
      input.type = 'file';
      document.body.appendChild(input);
      const result = await new Promise(resolve => {
        input.addEventListener('change', () => resolve('changed'), {once: true});
        input.addEventListener('cancel', () => resolve('canceled'), {once: true});
        input.showPicker();
      });
      if (result !== 'changed') {
        return result;
      }
      const file = input.files[0];
      return `${file.name}:${await file.text()}`;
    })()
  )js";

  EXPECT_EQ("test.txt:hello from file picker",
            content::EvalJs(guest_rfh, kInputFilePickerScript));
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicFilePickerBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NoWebview" : "Webview";
                         });

}  // namespace
}  // namespace glic

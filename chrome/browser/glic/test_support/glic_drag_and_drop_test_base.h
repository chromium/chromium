// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_DRAG_AND_DROP_TEST_BASE_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_DRAG_AND_DROP_TEST_BASE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "ui/gfx/geometry/point.h"

namespace content {
class WebContents;
}  // namespace content

namespace glic {

class GlicDragAndDropTestBase : public GlicApiBrowserTest {
 public:
  GlicDragAndDropTestBase(GlicTestJsPath js_test_file, bool enable_no_webview);
  ~GlicDragAndDropTestBase() override;

  bool IsNoWebview() const { return is_no_webview_; }

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  void PrepareGuestForDrag(Host& glic_host);
  gfx::Point GetGuestCenterInHost(Host& glic_host);

  base::FilePath CreateTestFile(base::ScopedTempDir& temp_dir,
                                const std::string& name,
                                const std::string& contents);

#if !BUILDFLAG(IS_ANDROID)
  content::WebContents* SetupSourceTabWithDraggableImage();
  [[nodiscard]] bool SimulateMouseDownAndWait(content::WebContents* source_wc,
                                              const gfx::Point& point);
  void SimulateMouseDragFromImage(content::WebContents* source_wc);
#endif

 private:
  bool is_no_webview_ = false;
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_DRAG_AND_DROP_TEST_BASE_H_

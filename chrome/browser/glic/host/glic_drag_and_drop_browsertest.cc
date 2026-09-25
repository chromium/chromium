// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/test/run_until.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_drag_and_drop_test_base.h"
#include "chrome/test/base/drag_and_drop_test_utils.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/geometry/point.h"

namespace glic {
namespace {

class GlicDragAndDropBrowserTest : public GlicDragAndDropTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  GlicDragAndDropBrowserTest()
      : GlicDragAndDropTestBase(
            GlicTestJsPath("./glic_drag_and_drop_browsertest.js"),
            GetParam()) {}
};

IN_PROC_BROWSER_TEST_P(GlicDragAndDropBrowserTest, testDragAndDropFile) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  Host* glic_host = &glic_instance->host();
  PrepareGuestForDrag(*glic_host);
  gfx::Point host_relative_point = GetGuestCenterInHost(*glic_host);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  base::FilePath test_file = CreateTestFile(
      temp_dir, "test.txt", "This is some test content for file drop.");

  drag_and_drop_test_utils::DragAndDropSimulator simulator(
      glic_host->webui_contents());
  ASSERT_TRUE(simulator.SimulateDragEnter(host_relative_point, test_file));

  ContinueJsTest();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return glic_host->GetGuestMainFrame()->GetRenderWidgetHost()->GetView() &&
           !glic_host->GetGuestMainFrame()
                ->GetRenderWidgetHost()
                ->GetView()
                ->GetViewBounds()
                .IsEmpty();
  }));
  host_relative_point = GetGuestCenterInHost(*glic_host);
  ASSERT_TRUE(simulator.SimulateDrop(host_relative_point));

  ContinueJsTest();
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicDragAndDropBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NoWebview" : "Webview";
                         });

}  // namespace
}  // namespace glic

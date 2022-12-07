// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/html_test_utils.h"

#include "ash/shell.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

void ExecuteScriptAndExtractInt(content::WebContents* web_contents,
                                const std::string& script,
                                int* result) {
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents,
      base::StringPrintf("window.domAutomationController.send(%s);",
                         script.c_str()),
      result));
}

void ExecuteScript(content::WebContents* web_contents,
                   const std::string& script) {
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));
}

gfx::Rect GetControlBoundsInRoot(content::WebContents* web_contents,
                                 const std::string& field_id) {
  // Use var instead of const or let so that this can be executed many
  // times within a context on different elements without Javascript
  // variable redeclaration errors.
  ExecuteScript(web_contents, base::StringPrintf(R"(
      var element = document.getElementById('%s');
      var bounds = element.getBoundingClientRect();
    )",
                                                 field_id.c_str()));
  int top, left, width, height;
  ExecuteScriptAndExtractInt(web_contents, "bounds.top", &top);
  ExecuteScriptAndExtractInt(web_contents, "bounds.left", &left);
  ExecuteScriptAndExtractInt(web_contents, "bounds.width", &width);
  ExecuteScriptAndExtractInt(web_contents, "bounds.height", &height);
  gfx::Rect rect(left, top, width, height);

  content::RenderWidgetHostView* view = web_contents->GetRenderWidgetHostView();
  gfx::Rect view_bounds_in_screen = view->GetViewBounds();
  gfx::Point origin = rect.origin();
  origin.Offset(view_bounds_in_screen.x(), view_bounds_in_screen.y());
  gfx::Rect rect_in_screen(origin.x(), origin.y(), rect.width(), rect.height());
  ::wm::ConvertRectFromScreen(Shell::GetPrimaryRootWindow(), &rect_in_screen);
  return rect_in_screen;
}

}  // namespace ash

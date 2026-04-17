// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SELECTION_WIDGET_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SELECTION_WIDGET_H_

#include "base/functional/callback.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

namespace glic {

class GlicSelectionWidgetDelegate : public views::BubbleDialogDelegate {
 public:
  // Shows the bubble anchored to `anchor_rect` with `on_click` executed when
  // the Ask Gemini button is clicked. Parented to `web_contents`'s view.
  static views::Widget* Show(content::WebContents* web_contents,
                             const gfx::Rect& anchor_rect,
                             base::RepeatingClosure on_click);

  GlicSelectionWidgetDelegate(const gfx::Rect& anchor_rect,
                              base::RepeatingClosure on_click);
  ~GlicSelectionWidgetDelegate() override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SELECTION_WIDGET_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SELECTION_WIDGET_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SELECTION_WIDGET_CONTROLLER_DELEGATE_H_

#include <optional>
#include <string>

#include "ui/gfx/geometry/rect.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace glic {

class GlicSelectionWidgetControllerDelegate {
 public:
  virtual ~GlicSelectionWidgetControllerDelegate() = default;
  // Returns the frame holding the tab's most recent text selection, or
  // `nullptr` if there is none.
  virtual content::RenderFrameHost* GetSelectedFrame() const = 0;
  // Returns the bounds of the tab's current text selection if it exists, in
  // screen coordinates. Returns `std::nullopt` otherwise.
  virtual std::optional<gfx::Rect> GetCurrentSelectionBounds() const = 0;
  virtual const std::u16string& GetSelectedText() const = 0;
  // TODO(liuwilliam): This is currently duplicated on `ShakeTriggerClient`.
  // The widget controller might be able to check it without the delegate.
  virtual bool IsSidePanelOpen() const = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SELECTION_WIDGET_CONTROLLER_DELEGATE_H_

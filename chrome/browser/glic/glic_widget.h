// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_WIDGET_H_
#define CHROME_BROWSER_GLIC_GLIC_WIDGET_H_

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace glic {

extern void* kGlicWidgetIdentifier;

// Glic panel widget.
class GlicWidget : public views::Widget {
 public:
  GlicWidget(const Widget&) = delete;
  GlicWidget& operator=(const Widget&) = delete;
  ~GlicWidget() override;

  // Create a widget with the given bounds.
  static std::unique_ptr<GlicWidget> Create(Profile* profile,
                                            const gfx::Rect& initial_bounds);

  // Get the most-overlapping display.
  display::Display GetDisplay();

 private:
  explicit GlicWidget(InitParams params);
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_WIDGET_H_

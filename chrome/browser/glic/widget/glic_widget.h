// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_WIDGET_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_WIDGET_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "ui/color/color_provider_key.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace glic {

extern void* kGlicWidgetIdentifier;

// Glic panel widget.
class GlicWidget : public views::Widget, public ThemeServiceObserver {
 public:
  GlicWidget(const Widget&) = delete;
  GlicWidget& operator=(const Widget&) = delete;
  ~GlicWidget() override;

  static gfx::Size GetInitialSize();

  // Create a widget with the given bounds.
  static std::unique_ptr<GlicWidget> Create(
      Profile* profile,
      const gfx::Rect& initial_bounds,
      base::WeakPtr<ui::AcceleratorTarget> accelerator_delegate,
      bool user_resizable);

  // Get the most-overlapping display.
  display::Display GetDisplay();

  // Sets the minimum size for the widget. Used for manual resize.
  void SetMinimumSize(const gfx::Size& size);

  // views:Widget:
  // Gets the minimum size a user can resize to for the widget.
  gfx::Size GetMinimumSize() const override;

  // Convert bounds rects between the visible on-screen bounds, and the actual
  //  widget bounds, which may require additional space for an invisible border
  // on Windows, when the widget is resizable.
  gfx::Rect VisibleToWidgetBounds(gfx::Rect visible_bounds);
  gfx::Rect WidgetToVisibleBounds(gfx::Rect widget_bounds);

 private:
  GlicWidget(ThemeService* theme_service, InitParams params);

  // views::Widget::
  ui::ColorProviderKey GetColorProviderKey() const override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  gfx::Size minimum_widget_size_;

  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_WIDGET_H_

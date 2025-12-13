// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_WIDGET_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_WIDGET_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "ui/color/color_provider_key.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

class BrowserWindowInterface;

namespace glic {
// Distance the detached window should be from the top and the right of the
// display when opened unassociated to a browser.
inline constexpr static int kDefaultDetachedTopRightDistance = 48;

class GlicView;

extern void* kGlicWidgetIdentifier;

// Glic panel widget.
class GlicWidget : public views::Widget, public ThemeServiceObserver {
 public:
  GlicWidget(const GlicWidget&) = delete;
  GlicWidget& operator=(const GlicWidget&) = delete;
  ~GlicWidget() override;

  // Returns the initial size for the single instance floating window.
  static gfx::Size GetInitialSize();

  static gfx::Rect GetInitialBounds(BrowserWindowInterface* browser,
                                    gfx::Size target_size);

  // Return `current_size` or the default minimum size if not provided.
  // The return value is clamped to fit between the minimum and maximum sizes.
  static gfx::Size ClampSize(std::optional<gfx::Size> current_size,
                             const GlicWidget* glic_widget);

  // True if |bounds| is an allowed position the Widget can be shown in.
  static bool IsWidgetLocationAllowed(const gfx::Rect& bounds);

  static std::unique_ptr<views::WidgetDelegate> CreateWidgetDelegate(
      std::unique_ptr<GlicView> contents_view,
      bool user_resizable);

  // Create a widget with the given bounds.
  static std::unique_ptr<GlicWidget> Create(views::WidgetDelegate* delegate,
                                            Profile* profile,
                                            const gfx::Rect& initial_bounds,
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

  base::WeakPtr<GlicWidget> GetWeakPtr();
  GlicView* GetGlicView();

 private:
  GlicWidget(ThemeService* theme_service, InitParams params);

  // views::Widget::
  ui::ColorProviderKey GetColorProviderKey() const override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  gfx::Size minimum_widget_size_;

  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};

  base::WeakPtrFactory<GlicWidget> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_WIDGET_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_widget.h"

#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/common/chrome_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace glic {
namespace {

bool UserResizeEnabled() {
  return base::FeatureList::IsEnabled(features::kGlicUserResize);
}

class GlicWidgetDelegate : public views::WidgetDelegate {
 public:
  GlicWidgetDelegate() {
    SetFocusTraversesOut(true);
    SetAccessibleTitle(l10n_util::GetStringUTF16(IDS_GLIC_WINDOW_TITLE));
    RegisterDeleteDelegateCallback(
        base::BindOnce(&GlicWidgetDelegate::Destroy, base::Unretained(this)));
  }

  GlicWidgetDelegate(const GlicWidgetDelegate&) = delete;
  GlicWidgetDelegate& operator=(const GlicWidgetDelegate&) = delete;

  ~GlicWidgetDelegate() override = default;

 private:
  void Destroy() { delete this; }
};
}  // namespace

void* kGlicWidgetIdentifier = &kGlicWidgetIdentifier;

GlicWidget::GlicWidget(InitParams params) : views::Widget(std::move(params)) {
  if (UserResizeEnabled()) {
    // Widget starts out non-resizable; client may enable resizing.
    minimum_widget_size_ = GetInitialSize();
  }
}

GlicWidget::~GlicWidget() = default;

gfx::Size GlicWidget::GetInitialSize() {
  return {features::kGlicInitialWidth.Get(),
          features::kGlicInitialHeight.Get()};
}

std::unique_ptr<GlicWidget> GlicWidget::Create(
    Profile* profile,
    const gfx::Rect& initial_bounds,
    base::WeakPtr<ui::AcceleratorTarget> accelerator_delegate) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
#if BUILDFLAG(IS_WIN)
  params.dont_show_in_taskbar = true;
  params.force_system_menu_for_frameless = true;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
#endif
  params.bounds = initial_bounds;
  params.sublevel = ChromeWidgetSublevel::kSublevelGlic;
  params.name = "GlicWidget";
  params.corner_radius = kCornerRadius;
  auto delegate = std::make_unique<GlicWidgetDelegate>();
  params.delegate = delegate.release();

  std::unique_ptr<GlicWidget> widget(new GlicWidget(std::move(params)));

  widget->SetContentsView(std::make_unique<GlicView>(
      profile, initial_bounds.size(), accelerator_delegate));

  // Mac fullscreen uses this identifier to find this widget and reparent it to
  // the overlay widget.
  widget->SetNativeWindowProperty(views::kWidgetIdentifierKey,
                                  kGlicWidgetIdentifier);

  return widget;
}

display::Display GlicWidget::GetDisplay() {
  std::optional<display::Display> display = GetNearestDisplay();
  if (display) [[likely]] {
    return *display;
  }

  // This should not happen after Widget::Init().
  return display::Screen::GetScreen()->GetPrimaryDisplay();
}

void GlicWidget::SetMinimumSize(const gfx::Size& size) {
  minimum_widget_size_ = size;
  minimum_widget_size_.SetToMax(GetInitialSize());
}

gfx::Size GlicWidget::GetMinimumSize() const {
  return UserResizeEnabled() ? minimum_widget_size_ : gfx::Size();
}

}  // namespace glic

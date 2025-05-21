// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_widget.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/common/chrome_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider_key.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/win/hwnd_metrics.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace glic {
namespace {

constexpr float kGlicWidgetCornerRadius = 12;

// For resizeable windows, there may be an invisible border which affects the
// widget size. Given a target rect, this method provides the outsets which
// should be applied in order to calculate the correct widget bounds.
gfx::Outsets GetTargetOutsets(const gfx::Rect& bounds) {
  gfx::Outsets outsets;
#if BUILDFLAG(IS_WIN)
  RECT bounds_rect = bounds.ToRECT();
  int frame_thickness = ui::GetFrameThickness(
      MonitorFromRect(&bounds_rect, MONITOR_DEFAULTTONEAREST),
      /*has_caption=*/false);
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(bounds);
  frame_thickness = frame_thickness / display.device_scale_factor();
  // On Windows, the presence of a frame means that we need to adjust both the
  // width and height of the widget by 2*frame thickness, and center the content
  // horizontally.
  outsets.set_left(frame_thickness);
  outsets.set_right(frame_thickness);
  outsets.set_bottom(2 * frame_thickness);
#endif
  return outsets;
}

}  // namespace

class GlicWidgetDelegate : public views::WidgetDelegate {
 public:
  GlicWidgetDelegate() {
    SetFocusTraversesOut(true);
    RegisterDeleteDelegateCallback(
        RegisterDeleteCallbackPassKey(),
        base::BindOnce(&GlicWidgetDelegate::Destroy, base::Unretained(this)));
  }

  GlicWidgetDelegate(const GlicWidgetDelegate&) = delete;
  GlicWidgetDelegate& operator=(const GlicWidgetDelegate&) = delete;

  ~GlicWidgetDelegate() override = default;

 private:
  void Destroy() { delete this; }
};

void* kGlicWidgetIdentifier = &kGlicWidgetIdentifier;

GlicWidget::GlicWidget(ThemeService* theme_service, InitParams params)
    : views::Widget(std::move(params)) {
  minimum_widget_size_ = GetInitialSize();
  OnSizeConstraintsChanged();
  theme_service_observation_.Observe(theme_service);
}

GlicWidget::~GlicWidget() = default;

// static
gfx::Size GlicWidget::GetInitialSize() {
  return {features::kGlicInitialWidth.Get(),
          features::kGlicInitialHeight.Get()};
}

std::unique_ptr<GlicWidget> GlicWidget::Create(
    Profile* profile,
    const gfx::Rect& initial_bounds,
    base::WeakPtr<ui::AcceleratorTarget> accelerator_delegate,
    bool user_resizable) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = initial_bounds;
  if (user_resizable) {
    params.bounds.Outset(GetTargetOutsets(initial_bounds));
  }
#if BUILDFLAG(IS_WIN)
  // If floaty won't be always on top, it should appear in the taskbar and
  // alt tab list.
  if (!base::FeatureList::IsEnabled(features::kGlicZOrderChanges)) {
    params.dont_show_in_taskbar = true;
  }
  params.force_system_menu_for_frameless = true;
#endif
  params.sublevel = ChromeWidgetSublevel::kSublevelGlic;
  // Don't change this name. This is used by other code to identify the glic
  // window. See b/404947780.
  params.name = "GlicWidget";
  // Support of rounded corners varies across platforms. See
  // Widget::InitParams::corner_radius. DO NOT apply this radius using
  // views::Background or in the web client because it will mismatch with
  // the window's actual corner radius. e.g. on win10 resizable windows
  // do have rounded corners.
  params.corner_radius = kGlicWidgetCornerRadius;
#if BUILDFLAG(IS_MAC)
  params.animation_enabled = true;
#endif
  auto delegate = std::make_unique<GlicWidgetDelegate>();
  delegate->SetCanResize(user_resizable);
  params.delegate = delegate.release();

  auto widget = base::WrapUnique(new GlicWidget(
      ThemeServiceFactory::GetForProfile(profile), std::move(params)));
  widget->SetMinimumSize(GetInitialSize());
  widget->SetContentsView(std::make_unique<GlicView>(
      profile, initial_bounds.size(), accelerator_delegate));

  // Mac fullscreen uses this identifier to find this widget and reparent it to
  // the overlay widget.
  widget->SetNativeWindowProperty(views::kWidgetIdentifierKey,
                                  kGlicWidgetIdentifier);

#if BUILDFLAG(IS_WIN)
  HWND hwnd = widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  if (hwnd != nullptr) {
    ui::win::PreventWindowFromPinning(hwnd);
    if (base::FeatureList::IsEnabled(features::kGlicZOrderChanges)) {
      ui::win::SetAppIdForWindow(
          ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()), hwnd);
    }
  }  // BUILDFLAG(IS_WIN)
#endif  //
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
  OnSizeConstraintsChanged();
}

gfx::Size GlicWidget::GetMinimumSize() const {
  return minimum_widget_size_;
}

gfx::Rect GlicWidget::VisibleToWidgetBounds(gfx::Rect visible_bounds) {
  if (widget_delegate()->CanResize()) {
    visible_bounds.Outset(GetTargetOutsets(visible_bounds));
  }
  return visible_bounds;
}

gfx::Rect GlicWidget::WidgetToVisibleBounds(gfx::Rect widget_bounds) {
  if (widget_delegate()->CanResize()) {
    widget_bounds.Inset(-GetTargetOutsets(widget_bounds).ToInsets());
  }
  return widget_bounds;
}

ui::ColorProviderKey GlicWidget::GetColorProviderKey() const {
  ui::ColorProviderKey key = Widget::GetColorProviderKey();

  ThemeService::BrowserColorScheme theme_color_scheme =
      theme_service_observation_.GetSource()->GetBrowserColorScheme();
  if (theme_color_scheme != ThemeService::BrowserColorScheme::kSystem) {
    key.color_mode =
        theme_color_scheme == ThemeService::BrowserColorScheme::kLight
            ? ui::ColorProviderKey::ColorMode::kLight
            : ui::ColorProviderKey::ColorMode::kDark;
  }

  return key;
}

void GlicWidget::OnThemeChanged() {
  NotifyColorProviderChanged();
  ThemeChanged();
}

}  // namespace glic

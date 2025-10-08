// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_widget.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/common/chrome_features.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider_key.h"
#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

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
constexpr int kMaxWidgetSize = 16'384;
constexpr int kInitialPositionBuffer = 4;

// For resizeable windows, there may be an invisible border which affects the
// widget size. Given a target rect, this method provides the outsets which
// should be applied in order to calculate the correct widget bounds.
gfx::Outsets GetTargetOutsets(const gfx::Rect& bounds) {
  gfx::Outsets outsets;
#if BUILDFLAG(IS_WIN)
  RECT bounds_rect = bounds.ToRECT();
  int frame_thickness = ui::GetResizableFrameThicknessFromMonitorInDIP(
      MonitorFromRect(&bounds_rect, MONITOR_DEFAULTTONEAREST),
      /*has_caption=*/false);
  // On Windows, the presence of a frame means that we need to adjust the left,
  // right and bottom by frame thickness.
  outsets.set_left_right(frame_thickness, frame_thickness);
  outsets.set_bottom(frame_thickness);
#endif
  return outsets;
}

class ClientView : public views::ClientView {
 public:
  explicit ClientView(std::unique_ptr<GlicView> glic_view)
      : views::ClientView(/*widget=*/nullptr,
                          /*contents_view=*/glic_view.get()),
        glic_view_(std::move(glic_view)) {}
  ~ClientView() override = default;

  GlicView* glic_view() { return glic_view_.get(); }

 private:
  std::unique_ptr<GlicView> glic_view_;
};

bool UseClientView() {
  return base::FeatureList::IsEnabled(features::kGlicWindowDragRegions);
}

views::Widget::InitParams::Type GetWidgetType() {
  return UseClientView() ? views::Widget::InitParams::TYPE_WINDOW
                         : views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
}

display::Display GetDisplayForOpeningDetached() {
  // Get the Display for the most recently active browser. If there was no
  // recently active browser, use the primary display.
  BrowserWindowInterface* const bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  ui::BaseWindow* const window = bwi ? bwi->GetWindow() : nullptr;
  if (window) {
    const std::optional<display::Display> widget_display =
        views::Widget::GetWidgetForNativeWindow(window->GetNativeWindow())
            ->GetNearestDisplay();
    if (widget_display) {
      return *widget_display;
    }
  }
  return display::Screen::Get()->GetPrimaryDisplay();
}

std::optional<gfx::Rect> GetInitialDetachedBoundsFromBrowser(
    BrowserWindowInterface* browser,
    const gfx::Size& target_size) {
  if (!browser) {
    return std::nullopt;
  }

  // Set the origin so the top right of the glic widget meets the bottom left
  // of the glic button.
  GlicButton* glic_button = GlicButton::FromBrowser(browser);
  if (!glic_button) {
    return std::nullopt;
  }
  gfx::Rect glic_button_inset_bounds = glic_button->GetBoundsWithInset();

  gfx::Point origin(glic_button_inset_bounds.x() - target_size.width() -
                        kInitialPositionBuffer,
                    glic_button_inset_bounds.bottom() + kInitialPositionBuffer);
  gfx::Rect bounds = {origin, target_size};

  return GlicWidget::IsWidgetLocationAllowed(bounds)
             ? std::make_optional(bounds)
             : std::nullopt;
}

gfx::Rect GetInitialDetachedBoundsNoBrowser(const gfx::Size& target_size) {
  // Get the default position offset equal distances from the top right corner
  // of the work area (which excludes system UI such as the taskbar).
  display::Display display = GetDisplayForOpeningDetached();
  gfx::Point top_right = display.work_area().top_right();
  int initial_x =
      top_right.x() - target_size.width() - kDefaultDetachedTopRightDistance;
  int initial_y = top_right.y() + kDefaultDetachedTopRightDistance;
  return {{initial_x, initial_y}, target_size};
}
}  // namespace

class GlicWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit GlicWidgetDelegate(std::unique_ptr<GlicView> glic_view)
      : client_view_(glic_view
                         ? std::make_unique<ClientView>(std::move(glic_view))
                         : nullptr) {
    SetFocusTraversesOut(true);
    RegisterDeleteDelegateCallback(
        RegisterDeleteCallbackPassKey(),
        base::BindOnce(&GlicWidgetDelegate::Destroy, base::Unretained(this)));
  }

  GlicWidgetDelegate(const GlicWidgetDelegate&) = delete;
  GlicWidgetDelegate& operator=(const GlicWidgetDelegate&) = delete;

  ~GlicWidgetDelegate() override = default;

  views::ClientView* CreateClientView(views::Widget* widget) override {
    return client_view_ ? client_view_.get()
                        : views::WidgetDelegate::CreateClientView(widget);
  }

 private:
  void Destroy() { delete this; }

  std::unique_ptr<ClientView> client_view_;
};

void* kGlicWidgetIdentifier = &kGlicWidgetIdentifier;

GlicWidget::GlicWidget(ThemeService* theme_service, InitParams params)
    : views::Widget(std::move(params)) {
  minimum_widget_size_ = GetInitialSize();
  OnSizeConstraintsChanged();
  theme_service_observation_.Observe(theme_service);
}

GlicWidget::~GlicWidget() = default;

// Static
gfx::Size GlicWidget::GetInitialSize() {
  return {features::kGlicInitialWidth.Get(),
          features::kGlicInitialHeight.Get()};
}

gfx::Rect GlicWidget::GetInitialBounds(BrowserWindowInterface* browser,
                                       gfx::Size target_size) {
  std::optional<gfx::Rect> bounds_with_browser =
      GetInitialDetachedBoundsFromBrowser(browser, target_size);
  return bounds_with_browser.value_or(
      GetInitialDetachedBoundsNoBrowser(target_size));
}

gfx::Size GlicWidget::ClampSize(std::optional<gfx::Size> current_size,
                                const GlicWidget* glic_widget) {
  gfx::Size min = GetInitialSize();
  if (glic_widget) {
    gfx::Size widget_min = glic_widget->GetMinimumSize();
    if (!widget_min.IsEmpty()) {
      min = widget_min;
    }
  }
  constexpr gfx::Size max(kMaxWidgetSize, kMaxWidgetSize);

  gfx::Size clamped_size = current_size.value_or(min);
  clamped_size.SetToMax(min);
  clamped_size.SetToMin(max);
  return clamped_size;
}

bool GlicWidget::IsWidgetLocationAllowed(const gfx::Rect& bounds) {
  const std::vector<display::Display>& displays =
      display::Screen::Get()->GetAllDisplays();

  // Calculate inset corners to allow part of the widget to be off screen.
  std::array<gfx::Point, 4> inset_points = {
      // top-left: Allow 40% on left and |kInitialPositionBuffer| on top.
      gfx::Point(bounds.x() + bounds.width() * .4,
                 bounds.y() + kInitialPositionBuffer),
      // top-right: Allow 40% on right and |kInitialPositionBuffer| on top.
      gfx::Point(bounds.right() - bounds.width() * .4,
                 bounds.y() + kInitialPositionBuffer),
      // bottom-left: Allow 40% on left and 70% on bottom.
      gfx::Point(bounds.x() + bounds.width() * .4,
                 bounds.bottom() - bounds.height() * .7),
      // bottom-right: Allow 40% on right and 70% on bottom.
      gfx::Point(bounds.right() - bounds.width() * .4,
                 bounds.bottom() - bounds.height() * .7),
  };
  // Check that all four points are on an existing display.
  return std::ranges::all_of(inset_points, [&](gfx::Point p) {
    return display::FindDisplayContainingPoint(displays, p) != displays.end();
  });
}

// End Static

std::unique_ptr<GlicWidget> GlicWidget::Create(
    Profile* profile,
    const gfx::Rect& initial_bounds,
    base::WeakPtr<ui::AcceleratorTarget> accelerator_delegate,
    bool user_resizable) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET, GetWidgetType());
  params.bounds = initial_bounds;
#if BUILDFLAG(IS_OZONE)
  // Some platforms don't allow accelerated widgets to be positioned from
  // client-side. Don't set an origin in that case.
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .supports_global_screen_coordinates) {
    params.bounds.set_origin({});
  }
#endif
  if (user_resizable) {
    params.bounds.Outset(GetTargetOutsets(initial_bounds));
  }
#if BUILDFLAG(IS_WIN)
  // If floaty won't be always on top, it should appear in the taskbar and
  // alt tab list.
  if (!base::FeatureList::IsEnabled(features::kGlicZOrderChanges)) {
    params.dont_show_in_taskbar = true;
  }
  if (!base::FeatureList::IsEnabled(features::kGlicWindowDragRegions)) {
    params.force_system_menu_for_frameless = true;
  }
#endif
  params.sublevel = ChromeWidgetSublevel::kSublevelGlic;
  // Don't change this name. This is used by other code to identify the glic
  // window. See b/404947780.
  params.name = "GlicWidget";
#if BUILDFLAG(IS_LINUX)
  params.wm_class_class = shell_integration_linux::GetProgramClassClass();
  params.wayland_app_id = params.wm_class_class + "-glic";
#endif
  // Support of rounded corners varies across platforms. See
  // Widget::InitParams::rounded_corners. DO NOT apply this radius using
  // views::Background or in the web client because it will mismatch with
  // the window's actual corner radius. e.g. on win10 resizable windows
  // do have rounded corners.
  params.rounded_corners = gfx::RoundedCornersF(kGlicWidgetCornerRadius);
#if BUILDFLAG(IS_MAC)
  params.animation_enabled = true;
#endif
  if (UseClientView()) {
    params.remove_standard_frame = true;
  }
  auto glic_view = std::make_unique<GlicView>(profile, initial_bounds.size(),
                                              accelerator_delegate);
  std::unique_ptr<GlicWidgetDelegate> delegate;
  if (UseClientView()) {
    delegate = std::make_unique<GlicWidgetDelegate>(std::move(glic_view));
  } else {
    delegate = std::make_unique<GlicWidgetDelegate>(nullptr);
  }
  delegate->SetCanResize(user_resizable);
  params.delegate = delegate.release();

  auto widget = base::WrapUnique(new GlicWidget(
      ThemeServiceFactory::GetForProfile(profile), std::move(params)));
  widget->SetMinimumSize(GetInitialSize());

  if (!UseClientView()) {
    widget->SetContentsView(std::move(glic_view));
  }

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
  }
#endif  // BUILDFLAG(IS_WIN)
  return widget;
}

display::Display GlicWidget::GetDisplay() {
  std::optional<display::Display> display = GetNearestDisplay();
  if (display) [[likely]] {
    return *display;
  }

  // This should not happen after Widget::Init().
  return display::Screen::Get()->GetPrimaryDisplay();
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

base::WeakPtr<GlicWidget> GlicWidget::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

GlicView* GlicWidget::GetGlicView() {
  if (UseClientView()) {
    return static_cast<::glic::ClientView*>(client_view())->glic_view();
  } else {
    return static_cast<GlicView*>(GetContentsView());
  }
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

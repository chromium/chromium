// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/test/compat_mode_test_base.h"

#include "ash/components/arc/compat_mode/arc_window_property_util.h"
#include "ash/public/cpp/window_properties.h"
#include "base/containers/flat_map.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_utils.h"

namespace arc {

namespace {

class TestArcResizeLockPrefDelegate : public ArcResizeLockPrefDelegate {
 public:
  ~TestArcResizeLockPrefDelegate() override = default;

  // ArcResizeLockPrefDelegate:
  mojom::ArcResizeLockState GetResizeLockState(
      const std::string& app_id) const override {
    auto it = resize_lock_states.find(app_id);
    if (it == resize_lock_states.end())
      return mojom::ArcResizeLockState::UNDEFINED;

    return it->second;
  }
  void SetResizeLockState(const std::string& app_id,
                          mojom::ArcResizeLockState state) override {
    resize_lock_states[app_id] = state;
  }

  bool GetResizeLockNeedsConfirmation(const std::string& app_id) override {
    return base::Contains(confirmation_needed_app_ids_, app_id);
  }
  void SetResizeLockNeedsConfirmation(const std::string& app_id,
                                      bool is_needed) override {
    if (GetResizeLockNeedsConfirmation(app_id) == is_needed)
      return;

    if (is_needed)
      confirmation_needed_app_ids_.push_back(app_id);
    else
      std::erase(confirmation_needed_app_ids_, app_id);
  }

  int GetShowSplashScreenDialogCount() const override { return show_count_; }
  void SetShowSplashScreenDialogCount(int count) override {
    show_count_ = count;
  }

 private:
  std::vector<std::string> confirmation_needed_app_ids_;
  base::flat_map<std::string, mojom::ArcResizeLockState> resize_lock_states;
  int show_count_{0};
};

}  // namespace

CompatModeTestBase::CompatModeTestBase()
    : views::ViewsTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
}
CompatModeTestBase::~CompatModeTestBase() = default;

void CompatModeTestBase::SetUp() {
  display::Screen::SetScreenInstance(&test_screen_);
  views::ViewsTestBase::SetUp();
  pref_delegate_ = std::make_unique<TestArcResizeLockPrefDelegate>();

  // FHD size by default. Must be bigger than kPortraitPhoneDp and
  // kLandscapeTabletDp.
  SetDisplayWorkArea(gfx::Rect(0, 0, 1920, 1080));
}

void CompatModeTestBase::TearDown() {
  views::ViewsTestBase::TearDown();
  display::Screen::SetScreenInstance(nullptr);
}

std::unique_ptr<views::Widget> CompatModeTestBase::CreateWidget(bool show) {
  auto widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_WINDOW);
  widget->widget_delegate()->SetCanResize(true);
  if (show)
    widget->Show();
  return widget;
}

std::unique_ptr<views::Widget> CompatModeTestBase::CreateArcWidget(
    std::optional<std::string> app_id,
    bool show) {
  auto widget = CreateWidget(/*show=*/false);
  if (app_id)
    widget->GetNativeWindow()->SetProperty(ash::kAppIDKey, *app_id);
  widget->GetNativeWindow()->SetProperty(chromeos::kAppTypeKey,
                                         chromeos::AppType::ARC_APP);
  if (show)
    widget->Show();
  return widget;
}

void CompatModeTestBase::SetDisplayWorkArea(const gfx::Rect& work_area) {
  display::Display display = test_screen_.GetPrimaryDisplay();
  display.set_work_area(work_area);
  test_screen_.display_list().UpdateDisplay(display);
}

void CompatModeTestBase::LeftClickOnView(const views::Widget* widget,
                                         const views::View* view) const {
  ui::test::EventGenerator event_generator(GetRootWindow(widget));
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
}

void CompatModeTestBase::SyncResizeLockPropertyWithMojoState(
    const views::Widget* widget) {
  auto* const window = widget->GetNativeWindow();
  const auto app_id = GetAppId(window);
  switch (pref_delegate()->GetResizeLockState(*app_id)) {
    case mojom::ArcResizeLockState::UNDEFINED:
      window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::NONE);
      break;
    case mojom::ArcResizeLockState::OFF:
      window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE);
      break;
    case mojom::ArcResizeLockState::ON:
    case mojom::ArcResizeLockState::READY:
    case mojom::ArcResizeLockState::FULLY_LOCKED:
      if (widget->widget_delegate()->CanResize()) {
        window->SetProperty(ash::kArcResizeLockTypeKey,
                            ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
      } else {
        window->SetProperty(
            ash::kArcResizeLockTypeKey,
            ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE);
      }
      break;
  }
}

}  // namespace arc

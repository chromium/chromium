// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/application_hotkey_delegate.h"

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/scoped_observation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"

namespace glic {

namespace {

constexpr std::array<glic::LocalHotkeyManager::Hotkey, 1> kSupportedHotkeys = {
    glic::LocalHotkeyManager::Hotkey::kFocusToggle};

// Implementation of ScopedHotkeyRegistration specifically for application-wide
// hotkeys. It registers and unregisters accelerators with the FocusManager of
// all current and future BrowserViews.
class ApplicationScopedHotkeyRegistration
    : public LocalHotkeyManager::ScopedHotkeyRegistration,
      public BrowserListObserver {
 public:
  ApplicationScopedHotkeyRegistration(
      ui::Accelerator accelerator,
      base::WeakPtr<ui::AcceleratorTarget> target)
      : accelerator_(accelerator), target_(target) {
    CHECK(!accelerator_.IsEmpty());
    for (Browser* browser : *BrowserList::GetInstance()) {
      RegisterAccelerator(browser);
    }
    browser_list_observation_.Observe(BrowserList::GetInstance());
  }

  ~ApplicationScopedHotkeyRegistration() override {
    CHECK(target_);
    for (Browser* browser : *BrowserList::GetInstance()) {
      if (auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser)) {
        browser_view->GetFocusManager()->UnregisterAccelerator(accelerator_,
                                                               target_.get());
      }
    }
  }

 private:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    RegisterAccelerator(browser);
  }

  void RegisterAccelerator(Browser* browser) {
    CHECK(target_);
    if (auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser)) {
      browser_view->GetFocusManager()->RegisterAccelerator(
          accelerator_,
          ui::AcceleratorManager::HandlerPriority::kNormalPriority,
          target_.get());
    }
  }

  ui::Accelerator accelerator_;
  base::WeakPtr<ui::AcceleratorTarget> target_;
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
};
}  // namespace

ApplicationHotkeyDelegate::ApplicationHotkeyDelegate(
    base::WeakPtr<GlicWindowController> window_controller)
    : window_controller_(window_controller) {}

ApplicationHotkeyDelegate::~ApplicationHotkeyDelegate() = default;

const base::span<const LocalHotkeyManager::Hotkey>
ApplicationHotkeyDelegate::GetSupportedHotkeys() const {
  return kSupportedHotkeys;
}

std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
ApplicationHotkeyDelegate::CreateScopedHotkeyRegistration(
    ui::Accelerator accelerator,
    base::WeakPtr<ui::AcceleratorTarget> target) {
  return std::make_unique<ApplicationScopedHotkeyRegistration>(accelerator,
                                                               target);
}

bool ApplicationHotkeyDelegate::AcceleratorPressed(
    LocalHotkeyManager::Hotkey hotkey) {
  if (!window_controller_) {
    return false;
  }

  switch (hotkey) {
    case LocalHotkeyManager::Hotkey::kFocusToggle:
      window_controller_->FocusIfOpen();
      base::RecordAction(base::UserMetricsAction("Glic.FocusHotKey"));
      return true;
    default:
      NOTREACHED() << "no handling implemented for "
                   << LocalHotkeyManager::HotkeyToString(hotkey);
  }
}

std::unique_ptr<LocalHotkeyManager> MakeApplicationHotkeyManager(
    base::WeakPtr<GlicWindowController> window_controller) {
  auto hotkey_manager = std::make_unique<LocalHotkeyManager>(
      window_controller,
      std::make_unique<ApplicationHotkeyDelegate>(window_controller));
  hotkey_manager->InitializeAccelerators();
  return hotkey_manager;
}
}  // namespace glic

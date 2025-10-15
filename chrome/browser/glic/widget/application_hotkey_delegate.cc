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
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
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
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [this](BrowserWindowInterface* browser_window_interface) {
          RegisterAccelerator(browser_window_interface);
          return true;
        });
    browser_list_observation_.Observe(BrowserList::GetInstance());
  }

  ~ApplicationScopedHotkeyRegistration() override {
    CHECK(target_);
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [this](BrowserWindowInterface* browser_window_interface) {
          if (auto* const browser_view = BrowserView::GetBrowserViewForBrowser(
                  browser_window_interface)) {
            browser_view->GetFocusManager()->UnregisterAccelerator(
                accelerator_, target_.get());
          }
          return true;
        });
  }

 private:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    RegisterAccelerator(browser);
  }

  void RegisterAccelerator(BrowserWindowInterface* browser_window_interface) {
    CHECK(target_);
    if (auto* const browser_view =
            BrowserView::GetBrowserViewForBrowser(browser_window_interface)) {
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
    base::WeakPtr<LocalHotkeyManager::Panel> panel)
    : panel_(panel) {}

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
  if (!panel_) {
    return false;
  }

  switch (hotkey) {
    case LocalHotkeyManager::Hotkey::kFocusToggle:
      panel_->FocusIfOpen();
      base::RecordAction(base::UserMetricsAction("Glic.FocusHotKey"));
      return true;
    default:
      NOTREACHED() << "no handling implemented for "
                   << LocalHotkeyManager::HotkeyToString(hotkey);
  }
}

std::unique_ptr<LocalHotkeyManager> MakeApplicationHotkeyManager(
    base::WeakPtr<LocalHotkeyManager::Panel> panel) {
  auto hotkey_manager = std::make_unique<LocalHotkeyManager>(
      panel, std::make_unique<ApplicationHotkeyDelegate>(panel));
  hotkey_manager->InitializeAccelerators();
  return hotkey_manager;
}
}  // namespace glic

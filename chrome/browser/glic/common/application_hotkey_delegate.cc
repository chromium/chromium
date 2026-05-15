// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/application_hotkey_delegate.h"

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/base_window.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif
#if BUILDFLAG(IS_ANDROID)
#include "ui/android/accelerator_manager_android.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#endif

namespace glic {

namespace {

// Implementation of ScopedHotkeyRegistration specifically for application-wide
// hotkeys. It registers and unregisters accelerators with the FocusManager of
// all current and future BrowserViews.
class ApplicationScopedHotkeyRegistration
    : public LocalHotkeyManager::ScopedHotkeyRegistration,
      public BrowserCollectionObserver {
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
    browser_collection_observation_.Observe(
        GlobalBrowserCollection::GetInstance());
  }

  ~ApplicationScopedHotkeyRegistration() override {
    CHECK(target_);
#if !BUILDFLAG(IS_ANDROID)
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [this](BrowserWindowInterface* browser_window_interface) {
          if (auto* const browser_view = BrowserView::GetBrowserViewForBrowser(
                  browser_window_interface)) {
            browser_view->GetFocusManager()->UnregisterAccelerator(
                accelerator_, target_.get());
          }
          return true;
        });
#else
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [this](BrowserWindowInterface* browser_window_interface) {
          if (auto* window = browser_window_interface->GetWindow()) {
            ui::AcceleratorManagerAndroid::FromWindow(
                *window->GetNativeWindow())
                ->UnregisterAccelerator(accelerator_, target_.get());
          }
          return true;
        });
#endif
  }

 private:
  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override {
    RegisterAccelerator(browser);
  }

  void RegisterAccelerator(BrowserWindowInterface* browser_window_interface) {
    CHECK(target_);
#if !BUILDFLAG(IS_ANDROID)
    if (auto* const browser_view =
            BrowserView::GetBrowserViewForBrowser(browser_window_interface)) {
      browser_view->GetFocusManager()->RegisterAccelerator(
          accelerator_,
          ui::AcceleratorManager::HandlerPriority::kNormalPriority,
          target_.get());
    }
#else
    if (auto* window = browser_window_interface->GetWindow()) {
      ui::AcceleratorManagerAndroid::FromWindow(*window->GetNativeWindow())
          ->RegisterAccelerator(
              accelerator_,
              ui::AcceleratorManager::HandlerPriority::kNormalPriority,
              target_.get());
    }
#endif
  }

  ui::Accelerator accelerator_;
  base::WeakPtr<ui::AcceleratorTarget> target_;
  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};
}  // namespace

ApplicationScopedRegistrationDelegate::ApplicationScopedRegistrationDelegate() =
    default;
ApplicationScopedRegistrationDelegate::
    ~ApplicationScopedRegistrationDelegate() = default;

std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
ApplicationScopedRegistrationDelegate::CreateScopedHotkeyRegistration(
    ui::Accelerator accelerator,
    base::WeakPtr<ui::AcceleratorTarget> target) {
  return std::make_unique<ApplicationScopedHotkeyRegistration>(accelerator,
                                                               target);
}
}  // namespace glic

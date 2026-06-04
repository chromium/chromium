// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/application_hotkey_delegate.h"

#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
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
      base::WeakPtr<ui::AcceleratorTarget> target,
      ProfileBrowserCollection* collection)
      : accelerator_(accelerator), target_(target), collection_(collection) {
    CHECK(!accelerator_.IsEmpty());
    CHECK(collection_);
    CHECK(target_);
    collection_->ForEach(
        [this](BrowserWindowInterface* browser_window_interface) {
          RegisterAccelerator(browser_window_interface);
          return true;
        },
        BrowserCollection::Order::kActivation);
    browser_collection_observation_.Observe(collection_);
  }

  ~ApplicationScopedHotkeyRegistration() override {
    CHECK(target_);
    collection_->ForEach(
        [this](BrowserWindowInterface* browser_window_interface) {
          UnregisterAccelerator(browser_window_interface);
          return true;
        },
        BrowserCollection::Order::kActivation);
  }

 private:
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
      auto* accelerator_manager =
          ui::AcceleratorManagerAndroid::FromWindow(window->GetNativeWindow());
      CHECK(accelerator_manager);
      accelerator_manager->RegisterAccelerator(
          accelerator_,
          ui::AcceleratorManager::HandlerPriority::kNormalPriority,
          target_.get());
    }
#endif
  }

  void UnregisterAccelerator(BrowserWindowInterface* browser_window_interface) {
#if !BUILDFLAG(IS_ANDROID)
    if (auto* const browser_view =
            BrowserView::GetBrowserViewForBrowser(browser_window_interface)) {
      browser_view->GetFocusManager()->UnregisterAccelerator(accelerator_,
                                                             target_.get());
    }
#else
    if (auto* window = browser_window_interface->GetWindow()) {
      if (auto* accelerator_manager = ui::AcceleratorManagerAndroid::FromWindow(
              window->GetNativeWindow())) {
        accelerator_manager->UnregisterAccelerator(accelerator_, target_.get());
      }
    }
#endif
  }

  ui::Accelerator accelerator_;
  base::WeakPtr<ui::AcceleratorTarget> target_;
  raw_ptr<ProfileBrowserCollection> collection_;
  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};
}  // namespace

ApplicationScopedRegistrationDelegate::ApplicationScopedRegistrationDelegate(
    Profile* profile)
    : profile_(profile) {}
ApplicationScopedRegistrationDelegate::
    ~ApplicationScopedRegistrationDelegate() = default;

std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
ApplicationScopedRegistrationDelegate::CreateScopedHotkeyRegistration(
    ui::Accelerator accelerator,
    base::WeakPtr<ui::AcceleratorTarget> target) {
  return std::make_unique<ApplicationScopedHotkeyRegistration>(
      accelerator, target, ProfileBrowserCollection::GetForProfile(profile_));
}
}  // namespace glic

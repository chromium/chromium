// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/application_registration_delegate.h"

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"

namespace dictation {

namespace {

// Implementation of ScopedHotkeyRegistration specifically for application-wide
// hotkeys tied to a profile. It registers and unregisters accelerators with the
// FocusManager of all current and future BrowserViews belonging to the profile.
class ApplicationScopedHotkeyRegistration
    : public LocalHotkeyManager::ScopedHotkeyRegistration,
      public BrowserCollectionObserver {
 public:
  ApplicationScopedHotkeyRegistration(ui::Accelerator accelerator,
                                      LocalHotkeyManager& hotkey_manager,
                                      ProfileBrowserCollection* collection);

  ApplicationScopedHotkeyRegistration(
      const ApplicationScopedHotkeyRegistration&) = delete;
  ApplicationScopedHotkeyRegistration& operator=(
      const ApplicationScopedHotkeyRegistration&) = delete;

  ~ApplicationScopedHotkeyRegistration() override;

 private:
  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;

  void RegisterAccelerator(BrowserWindowInterface* browser_window_interface);
  void UnregisterAccelerator(BrowserWindowInterface* browser_window_interface);

  ui::Accelerator accelerator_;
  raw_ref<LocalHotkeyManager> hotkey_manager_;
  raw_ptr<ProfileBrowserCollection> collection_;
  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

ApplicationScopedHotkeyRegistration::ApplicationScopedHotkeyRegistration(
    ui::Accelerator accelerator,
    LocalHotkeyManager& hotkey_manager,
    ProfileBrowserCollection* collection)
    : accelerator_(accelerator),
      hotkey_manager_(hotkey_manager),
      collection_(collection) {
  CHECK(!accelerator_.IsEmpty());
  CHECK(collection_);
  collection_->ForEach(
      [this](BrowserWindowInterface* browser_window_interface) {
        RegisterAccelerator(browser_window_interface);
        return true;
      },
      BrowserCollection::Order::kActivation);
  browser_collection_observation_.Observe(collection_);
}

ApplicationScopedHotkeyRegistration::~ApplicationScopedHotkeyRegistration() {
  collection_->ForEach(
      [this](BrowserWindowInterface* browser_window_interface) {
        UnregisterAccelerator(browser_window_interface);
        return true;
      },
      BrowserCollection::Order::kActivation);
}

void ApplicationScopedHotkeyRegistration::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  RegisterAccelerator(browser);
}

void ApplicationScopedHotkeyRegistration::RegisterAccelerator(
    BrowserWindowInterface* browser_window_interface) {
  if (auto* const browser_view =
          BrowserView::GetBrowserViewForBrowser(browser_window_interface)) {
    browser_view->GetFocusManager()->RegisterAccelerator(
        accelerator_, ui::AcceleratorManager::HandlerPriority::kNormalPriority,
        &(*hotkey_manager_));
  }
}

void ApplicationScopedHotkeyRegistration::UnregisterAccelerator(
    BrowserWindowInterface* browser_window_interface) {
  if (auto* const browser_view =
          BrowserView::GetBrowserViewForBrowser(browser_window_interface)) {
    browser_view->GetFocusManager()->UnregisterAccelerator(accelerator_,
                                                           &(*hotkey_manager_));
  }
}

}  // namespace

ApplicationRegistrationDelegate::ApplicationRegistrationDelegate() = default;

ApplicationRegistrationDelegate::~ApplicationRegistrationDelegate() = default;

std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
ApplicationRegistrationDelegate::CreateScopedHotkeyRegistration(
    Profile* profile,
    ui::Accelerator accelerator,
    LocalHotkeyManager& hotkey_manager) {
  return std::make_unique<ApplicationScopedHotkeyRegistration>(
      accelerator, hotkey_manager,
      ProfileBrowserCollection::GetForProfile(profile));
}

}  // namespace dictation

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_UTIL_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_UTIL_H_

#include <functional>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "components/tabs/public/tab_interface.h"

#if !BUILDFLAG(IS_ANDROID)
#include "ui/views/widget/widget.h"
#endif

class AccountCapabilitiesTestMutator;
class BrowserWindowInterface;
class Profile;

namespace glic {

namespace prefs {
enum class FreStatus;
}  // namespace prefs

// Provides deterministic browser activation behavior.
// Useful in browser tests where focus is not reliable.
class BrowserActivator : public BrowserCollectionObserver {
 public:
  // The different modes in which browser activation can be controlled.
  enum class Mode {
    // Support a single browser, crash if more than one browser is created at
    // one time. Activates the browser when it is created. This is the default
    // mode, to notify test authors that special consideration is necessary.
    kSingleBrowser,
    // Always keep the first browser active.
    kFirst,
    // Use SetActive() to set the active browser.
    kManual,
  };

  BrowserActivator();
  ~BrowserActivator() override;

  // Sets the browser activation mode.
  void SetMode(Mode mode);

  // Sets the active browser. Switches to `Mode::kManual`.
  void SetActive(BrowserWindowInterface* browser);

  // BrowserCollectionObserver impl.
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

 private:
  void SetActivePrivate(BrowserWindowInterface* browser_window_interface);

  Mode mode_ = Mode::kSingleBrowser;
  raw_ptr<BrowserWindowInterface> active_browser_;
  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      observation_{this};

#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  std::unique_ptr<views::Widget::PaintAsActiveLock> active_lock_;
#endif
};

#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
// Tracks a glic instance. Always tracks glic instance associated with the first
// browser. May track based on tab, instance id, or whether the instance is
// floating.
class GlicInstanceTracker {
 public:
  explicit GlicInstanceTracker(Profile* profile = nullptr);
  ~GlicInstanceTracker();

  void SetProfile(Profile* profile);

  // Returns the currently tracked glic instance.
  GlicInstance* GetGlicInstance();

  std::string DescribeGlicTracking();

  // Glic tracking functions. By default, this fixture applies operations toward
  // the glic instance in tab 0. You can change this behavior by calling one of
  // these functions.

  // Have all glic instance operations linked to a glic instance with this ID.
  void TrackGlicInstanceWithId(InstanceId id) {
    Clear();
    tracked_instance_id_ = id;
  }

  // Track the glic instance at a specific tab index.
  void TrackGlicInstanceWithTabIndex(int index) {
    Clear();
    glic_instance_tab_index_ = index;
  }

  // Track the glic instance at this tab.
  void TrackGlicInstanceWithTabHandle(tabs::TabInterface::Handle handle) {
    Clear();
    glic_instance_tab_handle_ = handle;
  }

  void TrackFloatingGlicInstance() {
    Clear();
    track_floating_glic_instance_ = true;
  }

  // Track the only glic instance. CHECK fails if there is ever more than one.
  void TrackOnlyGlicInstance() {
    Clear();
    track_only_glic_instance_ = true;
  }

  Host* GetHost();

  [[nodiscard]] bool WaitForPanelState(mojom::PanelStateKind state);
  [[nodiscard]] bool WaitForShow();

  const std::optional<InstanceId>& tracked_instance_id() const {
    return tracked_instance_id_;
  }
  const std::optional<int>& glic_instance_tab_index() const {
    return glic_instance_tab_index_;
  }
  const std::optional<tabs::TabInterface::Handle>& glic_instance_tab_handle()
      const {
    return glic_instance_tab_handle_;
  }
  bool track_floating_glic_instance() const {
    return track_floating_glic_instance_;
  }

 private:
  BrowserWindowInterface* GetBrowser();
  void Clear();

  // Using a WeakPtr because some tests destroy the browser / profile.
  base::WeakPtr<Profile> profile_;
  // These determine which glic instance is tracked by this class. This affects
  // many functions in this fixture. Only one will be present at a time.
  std::optional<InstanceId> tracked_instance_id_;
  std::optional<int> glic_instance_tab_index_ = 0;
  std::optional<tabs::TabInterface::Handle> glic_instance_tab_handle_;
  bool track_floating_glic_instance_ = false;
  bool track_only_glic_instance_ = false;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// Returns the only glic instance for the given profile, or nullptr if none is
// found. CHECK fails if there is ever more than one.
GlicInstance* GetOnlyGlicInstance(Profile* profile);

// Returns the glic instance bound to the given tab, or nullptr if none is
// found.
GlicInstance* GetInstanceForTab(Profile* profile, tabs::TabInterface* tab);

// Returns the glic instance with the given id, or nullptr if none is found.
GlicInstance* GetInstanceById(Profile* profile, InstanceId id);

// Signs in a primary account, accepts the FRE, and enables the relevant
// capability for that profile. browser_tests and interactive_ui_tests should
// use GlicTestEnvironment. These methods are for unit_tests.
void ForceSigninAndGlicCapability(Profile* profile);
void SigninWithPrimaryAccount(Profile* profile);
void SetGlicCapability(Profile* profile, bool enabled);
void SetGlicCapability(AccountCapabilitiesTestMutator& mutator, bool enabled);
void SetFRECompletion(Profile* profile, prefs::FreStatus fre_status);

void InvalidateAccount(Profile* profile);
void ReauthAccount(Profile* profile);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_UTIL_H_

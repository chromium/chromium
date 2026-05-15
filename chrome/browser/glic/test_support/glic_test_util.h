// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_UTIL_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_UTIL_H_

#include <functional>
#include <sstream>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/test_support/test_result.h"
#include "components/tabs/public/tab_interface.h"

class AccountCapabilitiesTestMutator;
class BrowserWindowInterface;
class Profile;

namespace glic {

namespace prefs {
enum class FreStatus;
}  // namespace prefs

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

class GlicClientConnectionObserverImpl;

// Queues events, and allows waiting on them.
template <typename T>
class EventWaiter {
 public:
  EventWaiter() : future_(base::test::TestFutureMode::kQueue) {}
  ~EventWaiter() { Clear(); }

  // Add an event.
  void AddEvent(T event) { future_.SetValue(std::move(event)); }

  // Wait until the predicate is true for one of the events.
  // Consume all events passed to the predicate.
  [[nodiscard]] TestResult<> WaitUntil(
      base::RepeatingCallback<bool(const T&)> predicate) {
    rejected_events_.clear();
    while (true) {
      // Wait until there is a value or a timeout.
      if (!future_.IsReady() && !future_.Wait()) {
        break;
      }
      T event = future_.Take();
      if (predicate.Run(event)) {
        future_.Clear();
        return base::ok();
      }
      rejected_events_.push_back(std::move(event));
    }

    std::stringstream ss;
    ss << "Predicate not matched. Saw values: {";
    for (const auto& value : rejected_events_) {
      ss << value << ", ";
    }
    ss << "}";
    return base::unexpected(ss.str());
  }

  [[nodiscard]] TestResult<> WaitUntilEqual(const T& expected) {
    return WaitUntil(base::BindRepeating(
        [](const T& expected, const T& event) { return event == expected; },
        expected));
  }

  // Clear all buffered events.
  void Clear() {
    future_.Clear();
    rejected_events_.clear();
  }

 private:
  base::test::TestFuture<T> future_;
  std::vector<T> rejected_events_;
};

// Begins listening to client connection events for a glic instance at
// construction, and allows the test to wait for client connection and
// disconnection.
class GlicClientConnectionObserver {
 public:
  explicit GlicClientConnectionObserver(GlicInstance*);
  ~GlicClientConnectionObserver();

  // Waits until the client is connected, discarding all events to that point.
  // Calling this twice, for example, would assert that the client was connected
  // at least two times.
  [[nodiscard]] TestResult<> WaitForConnected();
  // Waits until the client is disconnected, discarding all events to that
  // point.
  [[nodiscard]] TestResult<> WaitForDisconnected();
  // Clears all events received.
  void Clear();

 private:
  void Notify(bool is_connected);
  friend class GlicClientConnectionObserverImpl;

  std::unique_ptr<GlicClientConnectionObserverImpl> impl_;
  EventWaiter<bool> waiter_;
};

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

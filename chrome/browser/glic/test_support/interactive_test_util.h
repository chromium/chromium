// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_TEST_UTIL_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_TEST_UTIL_H_

#include "base/scoped_observation_traits.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/polling_state_observer.h"

namespace base {
// Set up a custom |ScopedObservationTrait| for
// |GlicWindowController::WebUiStateObserver|.
template <>
struct ScopedObservationTraits<glic::GlicWindowController,
                               glic::GlicWindowController::WebUiStateObserver> {
  static void AddObserver(
      glic::GlicWindowController* controller,
      glic::GlicWindowController::WebUiStateObserver* observer) {
    controller->AddWebUiStateObserver(observer);
  }
  static void RemoveObserver(
      glic::GlicWindowController* controller,
      glic::GlicWindowController::WebUiStateObserver* observer) {
    controller->RemoveWebUiStateObserver(observer);
  }
};
}  // namespace base

namespace glic::test {

namespace internal {

// Observes `controller` for changes to state().
class GlicWindowControllerStateObserver
    : public ui::test::PollingStateObserver<GlicWindowController::State> {
 public:
  explicit GlicWindowControllerStateObserver(
      const GlicWindowController& controller);
  ~GlicWindowControllerStateObserver() override;
};

DECLARE_STATE_IDENTIFIER_VALUE(GlicWindowControllerStateObserver,
                               kGlicWindowControllerState);

// Observers the glic app internal state.
class GlicAppStateObserver : public ui::test::ObservationStateObserver<
                                 mojom::WebUiState,
                                 GlicWindowController,
                                 GlicWindowController::WebUiStateObserver> {
 public:
  explicit GlicAppStateObserver(GlicWindowController* controller);
  ~GlicAppStateObserver() override;
  // GlicWindowController::WebUiStateObserver
  void WebUiStateChanged(mojom::WebUiState state) override;
};

DECLARE_STATE_IDENTIFIER_VALUE(GlicAppStateObserver, kGlicAppState);

// True when the timer is not running. Use `Start()` to start the timer.
class WaitingStateObserver : public ui::test::StateObserver<bool> {
 public:
  WaitingStateObserver() { OnStateObserverStateChanged(true); }
  ~WaitingStateObserver() override = default;

  void Start(base::TimeDelta timeout) {
    OnStateObserverStateChanged(false);
    timer_.Start(FROM_HERE, timeout,
                 base::BindOnce(&WaitingStateObserver::OnTimeout,
                                base::Unretained(this)));
  }

 private:
  void OnTimeout() { OnStateObserverStateChanged(true); }

  base::OneShotTimer timer_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(WaitingStateObserver, kDelayState);
}  // namespace internal

// The glic WebUI web contents.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kGlicHostElementId);
// The glic webview contents.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kGlicContentsElementId);

}  // namespace glic::test

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_TEST_UTIL_H_

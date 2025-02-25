// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_INTERACTIVE_TEST_UTIL_H_
#define CHROME_BROWSER_GLIC_INTERACTIVE_TEST_UTIL_H_

#include "base/scoped_observation_traits.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/glic/glic_window_controller.h"
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

}  // namespace internal

DECLARE_ELEMENT_IDENTIFIER_VALUE(kGlicHostElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kGlicContentsElementId);

}  // namespace glic::test

#endif  // CHROME_BROWSER_GLIC_INTERACTIVE_TEST_UTIL_H_

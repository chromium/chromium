// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_TEST_UTIL_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_TEST_UTIL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation_traits.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/polling_state_observer.h"

namespace base {
// Set up a custom `ScopedObservationTrait` for `Host::Observer`.
template <>
struct ScopedObservationTraits<glic::GlicWindowController,
                               glic::Host::Observer> {
  static void AddObserver(glic::Host* host, glic::Host::Observer* observer) {
    host->AddObserver(observer);
  }
  static void RemoveObserver(glic::Host* host, glic::Host::Observer* observer) {
    host->RemoveObserver(observer);
  }
};
}  // namespace base

namespace glic::test {

namespace internal {

// Observes FRE controller for changes to dialog being shown.
class GlicFreShowingDialogObserver
    : public ui::test::PollingStateObserver<bool> {
 public:
  explicit GlicFreShowingDialogObserver(const GlicFreController& controller);
  ~GlicFreShowingDialogObserver() override;
};

DECLARE_STATE_IDENTIFIER_VALUE(GlicFreShowingDialogObserver,
                               kGlicFreShowingDialogState);

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

// Observes `controller` for changes to animation state.
class GlicWindowContorllerResizeObserver
    : public ui::test::PollingStateObserver<bool> {
 public:
  explicit GlicWindowContorllerResizeObserver(GlicWindowController& controller);
  ~GlicWindowContorllerResizeObserver() override;
};

DECLARE_STATE_IDENTIFIER_VALUE(GlicWindowContorllerResizeObserver,
                               kGlicWindowControllerResizeState);

// Observers the glic app internal state.
class GlicAppStateObserver
    : public ui::test::
          ObservationStateObserver<mojom::WebUiState, Host, Host::Observer> {
 public:
  explicit GlicAppStateObserver(Host* host);
  ~GlicAppStateObserver() override;
  // Host::Observer
  void WebUiStateChanged(mojom::WebUiState state) override;
};

DECLARE_STATE_IDENTIFIER_VALUE(GlicAppStateObserver, kGlicAppState);

// True when the timer is not running. Use `Start()` to start the timer.
class WaitingStateObserver : public ui::test::StateObserver<bool> {
 public:
  WaitingStateObserver();
  ~WaitingStateObserver() override;

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

class WebUiStateObserver : public ui::test::StateObserver<mojom::WebUiState>,
                           public Host::Observer {
 public:
  explicit WebUiStateObserver(Host* host);

  ~WebUiStateObserver() override;

  mojom::WebUiState GetStateObserverInitialState() const override;

  void WebUiStateChanged(mojom::WebUiState state) override;

 private:
  base::ScopedObservation<Host, Host::Observer> observation_{this};
  raw_ptr<Host> host_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(WebUiStateObserver, kWebUiState);

class OnViewChangedObserver
    : public ui::test::StateObserver<mojom::CurrentView>,
      public Host::Observer {
 public:
  explicit OnViewChangedObserver(Host* host);

  ~OnViewChangedObserver() override;

  mojom::CurrentView GetStateObserverInitialState() const override;

  void OnViewChanged(mojom::CurrentView state) override;

 private:
  base::ScopedObservation<Host, Host::Observer> observation_{this};
  raw_ptr<Host> host_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(OnViewChangedObserver, kFloatyViewState);

}  // namespace internal

// The glic WebUI web contents.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kGlicHostElementId);
// The glic webview contents.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kGlicContentsElementId);

// The glic FRE WebUI web contents.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kGlicFreHostElementId);
// The glic FRE webview contents.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kGlicFreContentsElementId);

}  // namespace glic::test

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_TEST_UTIL_H_

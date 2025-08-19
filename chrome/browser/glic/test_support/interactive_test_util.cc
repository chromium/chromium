// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/interactive_test_util.h"

#include "base/scoped_observation_traits.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/views/widget/widget_delegate.h"

namespace glic::test {

namespace internal {

GlicFreShowingDialogObserver::GlicFreShowingDialogObserver(
    const GlicFreController& controller)
    : PollingStateObserver(
          [&controller]() { return controller.IsShowingDialog(); }) {}
GlicFreShowingDialogObserver::~GlicFreShowingDialogObserver() = default;

DEFINE_STATE_IDENTIFIER_VALUE(GlicFreShowingDialogObserver,
                              kGlicFreShowingDialogState);

GlicWindowControllerStateObserver::GlicWindowControllerStateObserver(
    const GlicWindowController& controller)
    : PollingStateObserver([&controller]() { return controller.state(); }) {}
GlicWindowControllerStateObserver::~GlicWindowControllerStateObserver() =
    default;

DEFINE_STATE_IDENTIFIER_VALUE(GlicWindowControllerStateObserver,
                              kGlicWindowControllerState);

GlicWindowContorllerResizeObserver::GlicWindowContorllerResizeObserver(
    GlicWindowController& controller)
    : PollingStateObserver([&controller]() {
        return controller.GetGlicWidget()
                   ? controller.GetGlicWidget()->widget_delegate()->CanResize()
                   : false;
      }) {}
GlicWindowContorllerResizeObserver::~GlicWindowContorllerResizeObserver() =
    default;

DEFINE_STATE_IDENTIFIER_VALUE(GlicWindowContorllerResizeObserver,
                              kGlicWindowControllerResizeState);

GlicAppStateObserver::GlicAppStateObserver(Host* host)
    : ObservationStateObserver(host) {
  WebUiStateChanged(host->GetPrimaryWebUiState());
}

GlicAppStateObserver::~GlicAppStateObserver() = default;

void GlicAppStateObserver::WebUiStateChanged(mojom::WebUiState state) {
  OnStateObserverStateChanged(state);
}

DEFINE_STATE_IDENTIFIER_VALUE(GlicAppStateObserver, kGlicAppState);

WaitingStateObserver::WaitingStateObserver() {
  OnStateObserverStateChanged(true);
}

WaitingStateObserver::~WaitingStateObserver() = default;

WebUiStateObserver::WebUiStateObserver(Host* host) : host_(host) {
  observation_.Observe(host);
}

WebUiStateObserver::~WebUiStateObserver() {
  observation_.Reset();
}

mojom::WebUiState WebUiStateObserver::GetStateObserverInitialState() const {
  return host_->GetPrimaryWebUiState();
}

void WebUiStateObserver::WebUiStateChanged(mojom::WebUiState state) {
  OnStateObserverStateChanged(state);
}

OnViewChangedObserver::OnViewChangedObserver(Host* host) : host_(host) {
  observation_.Observe(host);
}

OnViewChangedObserver::~OnViewChangedObserver() {
  observation_.Reset();
}

mojom::CurrentView OnViewChangedObserver::GetStateObserverInitialState() const {
  return host_->GetPrimaryCurrentView();
}

void OnViewChangedObserver::OnViewChanged(mojom::CurrentView state) {
  OnStateObserverStateChanged(state);
}

}  // namespace internal

DEFINE_ELEMENT_IDENTIFIER_VALUE(kGlicHostElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kGlicContentsElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kGlicFreHostElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kGlicFreContentsElementId);

}  // namespace glic::test

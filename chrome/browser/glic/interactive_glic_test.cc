// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/interactive_glic_test.h"

#include "chrome/browser/glic/glic_window_controller.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/polling_state_observer.h"

namespace glic::test {

GlicInitializedStateObserver::GlicInitializedStateObserver(
    const GlicWindowController& controller)
    : PollingStateObserver(
          [&controller]() { return controller.web_client() != nullptr; }) {}
GlicInitializedStateObserver::~GlicInitializedStateObserver() = default;

DEFINE_ELEMENT_IDENTIFIER_VALUE(kInstrumentedGlicWebContentsElementId);
DEFINE_STATE_IDENTIFIER_VALUE(GlicInitializedStateObserver,
                              kGlicInitializedState);

}  // namespace glic::test

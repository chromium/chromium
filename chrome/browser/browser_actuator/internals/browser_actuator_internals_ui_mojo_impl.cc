// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_ui_mojo_impl.h"

#include <utility>

namespace browser_actuator {

BrowserActuatorInternalsUIMojoImpl::BrowserActuatorInternalsUIMojoImpl(
    mojo::PendingReceiver<
        browser_actuator_internals::mojom::BrowserActuatorInternalsUI> receiver,
    mojo::PendingRemote<
        browser_actuator_internals::mojom::BrowserActuatorInternalsPage> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {}

BrowserActuatorInternalsUIMojoImpl::~BrowserActuatorInternalsUIMojoImpl() =
    default;

}  // namespace browser_actuator

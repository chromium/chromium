// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/browser_actuator/browser_actuator_message_handler.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"
#include "components/browser_actuator/public/features.h"

BrowserActuatorMessageHandler::BrowserActuatorMessageHandler(Profile* profile)
    : GlicExperimentalTriggeringMessageHandler(profile,
                                               /*message_sender=*/nullptr) {}

BrowserActuatorMessageHandler::BrowserActuatorMessageHandler(
    Profile* profile,
    std::unique_ptr<glic::GlicExperimentalTriggeringCoordinator> coordinator)
    : GlicExperimentalTriggeringMessageHandler(profile,
                                               /*message_sender=*/nullptr,
                                               std::move(coordinator)) {}

BrowserActuatorMessageHandler::~BrowserActuatorMessageHandler() = default;

void BrowserActuatorMessageHandler::CheckFeatureFlags() const {
  CHECK(base::FeatureList::IsEnabled(browser_actuator::kBrowserActuator) &&
        base::FeatureList::IsEnabled(
            browser_actuator::
                kEnableBrowserActuatorForGlicExperimentalTriggering));
}

glic::GlicExperimentalTriggeringUpdateCallback
BrowserActuatorMessageHandler::GetUpdateCallback(
    components_sharing_message::SharingMessage&) {
  return base::DoNothing();
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/browser_actuator/browser_actuator_message_handler.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/browser_actuator/browser_actuator_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"

BrowserActuatorMessageHandler::BrowserActuatorMessageHandler(Profile* profile)
    : profile_(profile) {}

BrowserActuatorMessageHandler::~BrowserActuatorMessageHandler() = default;

void BrowserActuatorMessageHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    DoneCallback done_callback) {
  CHECK(base::FeatureList::IsEnabled(browser_actuator::kBrowserActuator));

  if (message.has_glic_experimental_triggering()) {
    CHECK(base::FeatureList::IsEnabled(
        browser_actuator::kEnableBrowserActuatorForGlicExperimentalTriggering));
    HandleGlicExperimentalTriggering(message.glic_experimental_triggering());
  }

  // TODO: Add support for future payload types (e.g.
  // message.has_browser_actuation())

  std::move(done_callback).Run(nullptr);
}

void BrowserActuatorMessageHandler::HandleGlicExperimentalTriggering(
    const components_sharing_message::GlicExperimentalTriggering& triggering) {
  if (!triggering.has_request()) {
    return;
  }

  const auto& request = triggering.request();
  if (!request.has_device_opt_in_request()) {
    // Ignore non-opt-in messages.
    return;
  }

  // For legacy experimental triggering proto, context_id will serve as
  // session_id.
  const std::string& session_id = triggering.context_id();
  if (session_id.empty()) {
    // Ignore messages without a session ID.
    return;
  }

  EnsureTransportSessionCreated(session_id);
}

void BrowserActuatorMessageHandler::EnsureTransportSessionCreated(
    const std::string& session_id) {
  if (!profile_) {
    return;
  }
  browser_actuator::BrowserActuatorService* service =
      browser_actuator::BrowserActuatorServiceFactory::GetForProfile(profile_);
  if (service && service->IsInitialized()) {
    service->GetOrCreateSession(session_id);
  }
}

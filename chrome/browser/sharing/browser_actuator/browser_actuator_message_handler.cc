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
#include "components/browser_actuator/proto/actuator_downstream_message.pb.h"
#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport_session.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"

BrowserActuatorMessageHandler::BrowserActuatorMessageHandler(Profile* profile)
    : profile_(profile) {}

BrowserActuatorMessageHandler::~BrowserActuatorMessageHandler() = default;

void BrowserActuatorMessageHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    DoneCallback done_callback) {
  CHECK(base::FeatureList::IsEnabled(browser_actuator::kBrowserActuator));

  switch (message.payload_case()) {
    case components_sharing_message::SharingMessage::
        kActuatorDownstreamMessage: {
      const browser_actuator::ActuatorDownstreamMessage& bundled_message =
          message.actuator_downstream_message();
      const std::string& session_id = bundled_message.session_id();

      if (!session_id.empty() && profile_) {
        browser_actuator::BrowserActuatorService* service =
            browser_actuator::BrowserActuatorServiceFactory::GetForProfile(
                profile_);
        if (service && service->IsInitialized()) {
          service->GetOrCreateSession(session_id);
        }
      }
      // TODO(crbug.com/538161953): Handle invalid/empty session_id or null
      // profile (e.g. via UMA metrics).
      // TODO(crbug.com/538161953): Iterate over
      // bundled_message.typed_payloads() and dispatch them.
      break;
    }
    case components_sharing_message::SharingMessage::
        kGlicExperimentalTriggering: {
      CHECK(base::FeatureList::IsEnabled(
          browser_actuator::
              kEnableBrowserActuatorForGlicExperimentalTriggering));
      HandleGlicExperimentalTriggering(message.glic_experimental_triggering());
      break;
    }
    default:
      // Other payloads are not handled by this handler.
      break;
  }

  std::move(done_callback).Run(nullptr);
}

void BrowserActuatorMessageHandler::HandleGlicExperimentalTriggering(
    const components_sharing_message::GlicExperimentalTriggering& triggering) {
  if (!triggering.has_request()) {
    return;
  }

  const auto& request = triggering.request();
  // For GlicExperimentalTriggering, the first message can be either an opt-in
  // or a trigger actuation message. Ignore other types.
  if (!request.has_device_opt_in_request() &&
      !request.has_trigger_actuation_request()) {
    return;
  }

  // For legacy experimental triggering proto, context_id will serve as
  // session_id.
  const std::string& session_id = triggering.context_id();
  if (session_id.empty()) {
    // Ignore messages without a session ID.
    return;
  }

  if (!profile_) {
    return;
  }
  browser_actuator::BrowserActuatorService* service =
      browser_actuator::BrowserActuatorServiceFactory::GetForProfile(profile_);
  if (!service || !service->IsInitialized()) {
    return;
  }
  browser_actuator::TransportSession* session =
      service->GetOrCreateSession(session_id);
  if (!session) {
    return;
  }

  session->OnMessage(browser_actuator::PayloadType::kExperimentalTriggering,
                     triggering);
}

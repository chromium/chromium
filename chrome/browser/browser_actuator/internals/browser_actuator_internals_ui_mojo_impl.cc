// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_ui_mojo_impl.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "chrome/browser/browser_actuator/browser_actuator_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_actuator/internal/session_stream_recorder.h"
#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_handler_factory.h"

namespace browser_actuator {

namespace {
// The page shows the newest events for each session. A session has no event
// limit in the recorder, so the reply is capped here to keep the Mojo message
// small. The true count travels in `total_events`.
constexpr size_t kMaxEventsPerSession = 500;
// One payload can carry a screenshot, so a single message is also capped.
constexpr size_t kMaxMessageBytes = 8 * 1024;
}  // namespace

BrowserActuatorInternalsUIMojoImpl::BrowserActuatorInternalsUIMojoImpl(
    mojo::PendingReceiver<
        browser_actuator_internals::mojom::BrowserActuatorInternalsUI> receiver,
    mojo::PendingRemote<
        browser_actuator_internals::mojom::BrowserActuatorInternalsPage> page,
    Profile* profile)
    : profile_(profile),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {}

BrowserActuatorInternalsUIMojoImpl::~BrowserActuatorInternalsUIMojoImpl() =
    default;

void BrowserActuatorInternalsUIMojoImpl::GetSessionHistory(
    GetSessionHistoryCallback callback) {
  BrowserActuatorService* service =
      profile_ ? BrowserActuatorServiceFactory::GetForProfile(profile_)
               : nullptr;
  SessionStreamRecorderFactory* recorder_factory =
      service ? SessionStreamRecorderFactory::FromFactory(
                    service->GetFactory(FactoryId::kSessionStreamRecorder))
              : nullptr;
  if (!recorder_factory) {
    // No service (feature off, or a non-regular profile), or the internals
    // feature did not install the recorder.
    std::move(callback).Run({});
    return;
  }

  // `SessionSnapshot` is typemapped to `mojom::SessionSummary`; see
  // browser_actuator_internals_mojom_traits.h.
  std::move(callback).Run(
      recorder_factory->GetAllSessions(kMaxEventsPerSession, kMaxMessageBytes));
}

}  // namespace browser_actuator

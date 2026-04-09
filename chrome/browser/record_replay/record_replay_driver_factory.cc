// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/record_replay_driver_factory.h"

#include <memory>

#include "base/containers/map_util.h"
#include "chrome/browser/record_replay/content_element_id.h"
#include "chrome/browser/record_replay/content_record_replay_driver.h"
#include "chrome/browser/record_replay/record_replay_client.h"
#include "chrome/browser/record_replay/record_replay_driver.h"
#include "content/public/browser/web_contents.h"

namespace record_replay {

RecordReplayDriverFactory::RecordReplayDriverFactory(RecordReplayClient& client)
    : client_(client) {}

RecordReplayDriverFactory::~RecordReplayDriverFactory() = default;

RecordReplayDriver* RecordReplayDriverFactory::GetOrCreateDriver(
    content::RenderFrameHost* rfh) {
  if (!rfh->IsRenderFrameLive()) {
    return nullptr;
  }
  std::unique_ptr<RecordReplayDriver>& driver = drivers_[rfh->GetFrameToken()];
  if (!driver) {
    driver = std::make_unique<ContentRecordReplayDriver>(rfh, *client_);
  }
  return driver.get();
}

RecordReplayDriver* RecordReplayDriverFactory::GetDriver(
    const blink::LocalFrameToken& frame_token) {
  std::unique_ptr<RecordReplayDriver>* driver =
      base::FindOrNull(drivers_, frame_token);
  return driver ? driver->get() : nullptr;
}

RecordReplayDriver* RecordReplayDriverFactory::GetDriver(
    const ElementId& element_id) {
  const auto& content_element_id =
      static_cast<const ContentElementId&>(element_id);
  return GetDriver(content_element_id.frame_token());
}

RecordReplayDriver* RecordReplayDriverFactory::GetDriver(
    const autofill::FieldGlobalId& field_id) {
  return GetDriver(blink::LocalFrameToken(field_id.frame_token.value()));
}

std::vector<RecordReplayDriver*> RecordReplayDriverFactory::GetActiveDrivers() {
  std::vector<RecordReplayDriver*> drivers;
  ForEachDriver([&](RecordReplayDriver& driver) {
    if (driver.IsActive()) {
      drivers.push_back(&driver);
    }
  });
  return drivers;
}

void RecordReplayDriverFactory::ForEachDriver(
    base::FunctionRef<void(RecordReplayDriver&)> fun) {
  for (const auto& [rfh, driver] : drivers_) {
    fun(*driver);
  }
}

void RecordReplayDriverFactory::RenderFrameCreated(
    content::RenderFrameHost* rfh) {
  RecordReplayDriver* driver = GetOrCreateDriver(rfh);
  if (driver && record_future_drivers_) {
    driver->StartRecording();
  }
}

void RecordReplayDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* rfh) {
  drivers_.erase(rfh->GetFrameToken());
}

void RecordReplayDriverFactory::SetRecordForFutureDrivers(bool enable) {
  record_future_drivers_ = enable;
}

}  // namespace record_replay

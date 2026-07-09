// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_observation.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/test/repeating_test_future.h"
#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

class EventDelegate : public EventObservation::Delegate {
 public:
  ~EventDelegate() override = default;

  // EventManager::Delegate:
  void OnEvent(const extensions::ExtensionId& extension_id,
               EventRouter* event_router,
               ash::cros_healthd::mojom::EventInfoPtr info) override {
    future_.AddValue(std::make_tuple(extension_id, std::move(info)));
  }

  std::tuple<extensions::ExtensionId, ash::cros_healthd::mojom::EventInfoPtr>
  WaitAndGetData() {
    return future_.Take();
  }

 private:
  base::test::RepeatingTestFuture<
      std::tuple<extensions::ExtensionId,
                 ash::cros_healthd::mojom::EventInfoPtr>>
      future_;
};

}  // namespace

class TelemetryExtensionEventObservationTest : public extensions::ApiUnitTest {
 public:
  TelemetryExtensionEventObservationTest() = default;

  void SetUp() override {
    extensions::ApiUnitTest::SetUp();
    event_router_ = std::make_unique<EventRouter>(browser_context());
    event_observation_ = std::make_unique<EventObservation>(
        extension()->id(), api::os_events::EventCategory::kAudioJack,
        event_router_.get(), browser_context());

    event_delegate_ = new EventDelegate();
    event_observation_->SetDelegateForTesting(event_delegate_);
  }

 protected:
  EventObservation* GetEventRouter() { return event_observation_.get(); }

  EventDelegate* GetEventDelegate() { return event_delegate_; }

  mojo::Remote<ash::cros_healthd::mojom::EventObserver>& GetRemote() {
    return remote_;
  }

  void Bind(mojo::PendingRemote<ash::cros_healthd::mojom::EventObserver>
                pending_remote) {
    remote_.Bind(std::move(pending_remote));
  }

 private:
  // The router, observation and its delegate live as long as the test itself.
  std::unique_ptr<EventRouter> event_router_;
  std::unique_ptr<EventObservation> event_observation_;
  raw_ptr<EventDelegate> event_delegate_;

  mojo::Remote<ash::cros_healthd::mojom::EventObserver> remote_;
};

TEST_F(TelemetryExtensionEventObservationTest, CanObserveAudioJackEvent) {
  Bind(GetEventRouter()->GetRemote());

  auto audio_info = ash::cros_healthd::mojom::AudioJackEventInfo::New();
  audio_info->state = ash::cros_healthd::mojom::AudioJackEventInfo::State::kAdd;
  auto info = ash::cros_healthd::mojom::EventInfo::NewAudioJackEventInfo(
      std::move(audio_info));

  GetRemote()->OnEvent(std::move(info));

  // Flush so that the result shows up.
  GetRemote().FlushForTesting();

  auto result = GetEventDelegate()->WaitAndGetData();

  EXPECT_EQ(std::get<0>(result), extension()->id());
  EXPECT_EQ(
      std::get<1>(result),
      ash::cros_healthd::mojom::EventInfo::NewAudioJackEventInfo(
          ash::cros_healthd::mojom::AudioJackEventInfo::New(
              ash::cros_healthd::mojom::AudioJackEventInfo::State::kAdd)));
}

}  // namespace chromeos

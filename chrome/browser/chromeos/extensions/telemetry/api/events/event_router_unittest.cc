// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_router.h"

#include <memory>

#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "extensions/browser/extensions_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

}  // namespace

class TelemetryExtensionEventRouterTest : public extensions::ExtensionsTest {
 public:
  TelemetryExtensionEventRouterTest() = default;

  void SetUp() override {
    extensions::ExtensionsTest::SetUp();

    event_router_ = std::make_unique<EventRouter>(browser_context());
  }

  EventRouter* GetEventRouter() { return event_router_.get(); }

 private:
  std::unique_ptr<EventRouter> event_router_;
};

TEST_F(TelemetryExtensionEventRouterTest, ResetReceiversForExtension) {
  constexpr char kExtensionIdOne[] = "TESTEXTENSION1";
  constexpr char kExtensionIdTwo[] = "TESTEXTENSION2";

  mojo::Remote<crosapi::TelemetryEventObserver> remote_one(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          crosapi::TelemetryEventCategoryEnum::kAudioJack, kExtensionIdOne));

  mojo::Remote<crosapi::TelemetryEventObserver> remote_two(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          crosapi::TelemetryEventCategoryEnum::kAudioJack, kExtensionIdTwo));

  ASSERT_TRUE(remote_one.is_bound());
  ASSERT_TRUE(remote_two.is_bound());

  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdOne, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdTwo, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  GetEventRouter()->ResetReceiversForExtension(kExtensionIdOne);

  // Flush so the result shows up.
  remote_one.FlushForTesting();
  remote_two.FlushForTesting();

  ASSERT_FALSE(remote_one.is_connected());
  ASSERT_TRUE(remote_two.is_connected());

  EXPECT_FALSE(GetEventRouter()->IsExtensionObserving(kExtensionIdOne));
  EXPECT_FALSE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdOne, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObserving(kExtensionIdTwo));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdTwo, crosapi::TelemetryEventCategoryEnum::kAudioJack));
}

TEST_F(TelemetryExtensionEventRouterTest, ResetReceiversOfExtensionByCategory) {
  constexpr char kExtensionIdOne[] = "TESTEXTENSION1";
  constexpr char kExtensionIdTwo[] = "TESTEXTENSION2";

  mojo::Remote<crosapi::TelemetryEventObserver> remote_one_audio(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          crosapi::TelemetryEventCategoryEnum::kAudioJack, kExtensionIdOne));
  mojo::Remote<crosapi::TelemetryEventObserver> remote_one_unmapped(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          crosapi::TelemetryEventCategoryEnum::kUnmappedEnumField,
          kExtensionIdOne));

  mojo::Remote<crosapi::TelemetryEventObserver> remote_two(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          crosapi::TelemetryEventCategoryEnum::kAudioJack, kExtensionIdTwo));

  ASSERT_TRUE(remote_one_audio.is_bound());
  ASSERT_TRUE(remote_one_unmapped.is_bound());
  ASSERT_TRUE(remote_two.is_bound());

  GetEventRouter()->ResetReceiversOfExtensionByCategory(
      kExtensionIdOne, crosapi::TelemetryEventCategoryEnum::kAudioJack);

  // Flush so the result shows up.
  remote_one_audio.FlushForTesting();
  remote_one_unmapped.FlushForTesting();
  remote_two.FlushForTesting();

  ASSERT_FALSE(remote_one_audio.is_connected());
  ASSERT_TRUE(remote_one_unmapped.is_connected());
  ASSERT_TRUE(remote_two.is_connected());

  EXPECT_TRUE(GetEventRouter()->IsExtensionObserving(kExtensionIdOne));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObserving(kExtensionIdTwo));

  // Reset the last category of extension one.
  GetEventRouter()->ResetReceiversOfExtensionByCategory(
      kExtensionIdOne, crosapi::TelemetryEventCategoryEnum::kUnmappedEnumField);

  EXPECT_FALSE(GetEventRouter()->IsExtensionObserving(kExtensionIdOne));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObserving(kExtensionIdTwo));
}

TEST_F(TelemetryExtensionEventRouterTest, RestrictReceiversForExtension) {
  constexpr char kExtensionIdOne[] = "TESTEXTENSION1";
  constexpr char kExtensionIdTwo[] = "TESTEXTENSION2";
  constexpr crosapi::TelemetryEventCategoryEnum regular_event =
      crosapi::TelemetryEventCategoryEnum::kAudioJack;
  constexpr crosapi::TelemetryEventCategoryEnum focus_restriced_event =
      crosapi::TelemetryEventCategoryEnum::kTouchpadConnected;

  mojo::Remote<crosapi::TelemetryEventObserver> remote_one_regular(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          regular_event, kExtensionIdOne));
  mojo::Remote<crosapi::TelemetryEventObserver> remote_one_focus_restriced(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          focus_restriced_event, kExtensionIdOne));
  mojo::Remote<crosapi::TelemetryEventObserver> remote_two_regular(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          regular_event, kExtensionIdTwo));
  mojo::Remote<crosapi::TelemetryEventObserver> remote_two_focus_restriced(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          focus_restriced_event, kExtensionIdTwo));

  ASSERT_TRUE(remote_one_regular.is_bound());
  ASSERT_TRUE(remote_one_focus_restriced.is_bound());
  ASSERT_TRUE(remote_two_regular.is_bound());
  ASSERT_TRUE(remote_two_focus_restriced.is_bound());

  EXPECT_TRUE(GetEventRouter()->IsExtensionObserving(kExtensionIdOne));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(kExtensionIdOne,
                                                                regular_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdOne, focus_restriced_event));
  EXPECT_FALSE(GetEventRouter()->IsExtensionRestricted(kExtensionIdOne));
  EXPECT_TRUE(GetEventRouter()->IsExtensionAllowedForCategory(kExtensionIdOne,
                                                              regular_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionAllowedForCategory(
      kExtensionIdOne, focus_restriced_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObserving(kExtensionIdTwo));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(kExtensionIdTwo,
                                                                regular_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdTwo, focus_restriced_event));
  EXPECT_FALSE(GetEventRouter()->IsExtensionRestricted(kExtensionIdTwo));
  EXPECT_TRUE(GetEventRouter()->IsExtensionAllowedForCategory(kExtensionIdTwo,
                                                              regular_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionAllowedForCategory(
      kExtensionIdTwo, focus_restriced_event));

  GetEventRouter()->RestrictReceiversOfExtension(kExtensionIdOne);
  EXPECT_TRUE(GetEventRouter()->IsExtensionObserving(kExtensionIdOne));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(kExtensionIdOne,
                                                                regular_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdOne, focus_restriced_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionRestricted(kExtensionIdOne));
  EXPECT_TRUE(GetEventRouter()->IsExtensionAllowedForCategory(kExtensionIdOne,
                                                              regular_event));
  EXPECT_FALSE(GetEventRouter()->IsExtensionAllowedForCategory(
      kExtensionIdOne, focus_restriced_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObserving(kExtensionIdTwo));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(kExtensionIdTwo,
                                                                regular_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdTwo, focus_restriced_event));
  EXPECT_FALSE(GetEventRouter()->IsExtensionRestricted(kExtensionIdTwo));
  EXPECT_TRUE(GetEventRouter()->IsExtensionAllowedForCategory(kExtensionIdTwo,
                                                              regular_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionAllowedForCategory(
      kExtensionIdTwo, focus_restriced_event));

  GetEventRouter()->UnrestrictReceiversOfExtension(kExtensionIdOne);
  EXPECT_TRUE(GetEventRouter()->IsExtensionObserving(kExtensionIdOne));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(kExtensionIdOne,
                                                                regular_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdOne, focus_restriced_event));
  EXPECT_FALSE(GetEventRouter()->IsExtensionRestricted(kExtensionIdOne));
  EXPECT_TRUE(GetEventRouter()->IsExtensionAllowedForCategory(kExtensionIdOne,
                                                              regular_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionAllowedForCategory(
      kExtensionIdOne, focus_restriced_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObserving(kExtensionIdTwo));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(kExtensionIdTwo,
                                                                regular_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdTwo, focus_restriced_event));
  EXPECT_FALSE(GetEventRouter()->IsExtensionRestricted(kExtensionIdTwo));
  EXPECT_TRUE(GetEventRouter()->IsExtensionAllowedForCategory(kExtensionIdTwo,
                                                              regular_event));
  EXPECT_TRUE(GetEventRouter()->IsExtensionAllowedForCategory(
      kExtensionIdTwo, focus_restriced_event));
}

}  // namespace chromeos

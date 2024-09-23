// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_keyboard_event.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service_factory.h"
#include "chrome/browser/profiles/profile.h"         // nogncheck
#include "chrome/browser/ui/browser_list.h"          // nogncheck
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck
#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kKeyboardDiagnosticsUrl[] =
    "chrome://diagnostics?input&showDefaultKeyboardTester";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

class TelemetryExtensionEventsApiBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    BaseTelemetryExtensionBrowserTest::SetUpOnMainThread();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_events_service_impl_ = new FakeEventsService();
    // SAFETY: We hand over ownership over the destruction of this pointer to
    // the first caller of `TelemetryEventsServiceAsh::Create`. The only
    // consumer of this is the `EventManager`, that lives as long as the profile
    // and therefore longer than this test, so we are safe to access
    // fake_events_service_impl_ in the test body.
    fake_events_service_factory_.SetCreateInstanceResponse(
        std::unique_ptr<FakeEventsService>(fake_events_service_impl_));
    ash::TelemetryEventServiceAsh::Factory::SetForTesting(
        &fake_events_service_factory_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    fake_events_service_impl_ = std::make_unique<FakeEventsService>();
    // Replace the production TelemetryEventsService with a fake for testing.
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        fake_events_service_impl_->BindNewPipeAndPassRemote());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_events_service_impl_ = nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Since one of tests opens browser window UI in Ash, it should close the
    // UI so that it won't pollute other tests running against the shared Ash.
    CloseAllAshBrowserWindows();
#endif
    BaseTelemetryExtensionBrowserTest::TearDownOnMainThread();
  }

 protected:
  void CheckIsEventSupported(const std::vector<std::string>& events,
                             const std::string& status);

  FakeEventsService* GetFakeService() {
    return fake_events_service_impl_.get();
  }

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // SAFETY: This pointer is owned in a unique_ptr by the EventManager. Since
  // the EventManager lives longer than this test, it is always safe to access
  // the fake in the test body.
  raw_ptr<FakeEventsService> fake_events_service_impl_;
  FakeEventsServiceFactory fake_events_service_factory_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<FakeEventsService> fake_events_service_impl_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

void TelemetryExtensionEventsApiBrowserTest::CheckIsEventSupported(
    const std::vector<std::string>& events,
    const std::string& status) {
  if (events.empty()) {
    return;
  }

  std::string event_str;
  for (const auto& event : events) {
    if (event_str.empty()) {
      event_str.append("[");
    } else {
      event_str.append(",");
    }
    event_str.append("'");
    event_str.append(event);
    event_str.append("'");
  }
  event_str.append("]");

  // Don't use array.forEach because it doesn't support await.
  CreateExtensionAndRunServiceWorker(base::StringPrintf(R"(
    chrome.test.runTests([
      async function isEventSupported() {
        const events = %s;
        for (let i = 0; i < events.length; i++) {
          const result = await chrome.os.events.isEventSupported(events[i]);
          chrome.test.assertEq(result, {
            status: '%s'
          });
        }
        chrome.test.succeed();
      }
    ]);
    )",
                                                        event_str.c_str(),
                                                        status.c_str()));
}

// Checks the event supportability.
IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       IsEventSupported) {
  auto supported = crosapi::TelemetryExtensionSupportStatus::NewSupported(
      crosapi::TelemetryExtensionSupported::New());
  GetFakeService()->SetIsEventSupportedResponse(std::move(supported));

  std::vector<std::string> unsupported_events;
  std::vector<std::string> supported_events;
  crosapi::TelemetryEventCategoryEnum category =
      crosapi::TelemetryEventCategoryEnum::kUnmappedEnumField;
  switch (category) {
    // Features behind a feature flag.
    case crosapi::TelemetryEventCategoryEnum::kUnmappedEnumField:
      [[fallthrough]];
    // Features without a feature flag.
    case crosapi::TelemetryEventCategoryEnum::kAudioJack:
      supported_events.push_back("audio_jack");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kLid:
      supported_events.push_back("lid");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kUsb:
      supported_events.push_back("usb");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kExternalDisplay:
      supported_events.push_back("external_display");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kSdCard:
      supported_events.push_back("sd_card");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kPower:
      supported_events.push_back("power");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kKeyboardDiagnostic:
      supported_events.push_back("keyboard_diagnostic");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kStylusGarage:
      supported_events.push_back("stylus_garage");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kTouchpadButton:
      supported_events.push_back("touchpad_button");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kTouchpadTouch:
      supported_events.push_back("touchpad_touch");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kTouchpadConnected:
      supported_events.push_back("touchpad_connected");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kStylusTouch:
      supported_events.push_back("stylus_touch");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kStylusConnected:
      supported_events.push_back("stylus_connected");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kTouchscreenTouch:
      supported_events.push_back("touchscreen_touch");
      [[fallthrough]];
    case crosapi::TelemetryEventCategoryEnum::kTouchscreenConnected:
      supported_events.push_back("touchscreen_connected");
      break;
  }

  CheckIsEventSupported(unsupported_events, "unsupported");
  CheckIsEventSupported(supported_events, "supported");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       IsEventSupported_Error) {
  auto exception = crosapi::TelemetryExtensionException::New();
  exception->reason = crosapi::TelemetryExtensionException::Reason::kUnexpected;
  exception->debug_message = "My test message";

  auto input = crosapi::TelemetryExtensionSupportStatus::NewException(
      std::move(exception));

  GetFakeService()->SetIsEventSupportedResponse(std::move(input));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isEventSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.events.isEventSupported("audio_jack"),
            'Error: My test message'
        );

        chrome.test.succeed();
      }
    ]);
    )");

  auto unmapped =
      crosapi::TelemetryExtensionSupportStatus::NewUnmappedUnionField(0);
  GetFakeService()->SetIsEventSupportedResponse(std::move(unmapped));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isEventSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.events.isEventSupported("audio_jack"),
            'Error: API internal error.'
        );

        chrome.test.succeed();
      }
    ]);
    )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       IsEventSupported_Success) {
  auto supported = crosapi::TelemetryExtensionSupportStatus::NewSupported(
      crosapi::TelemetryExtensionSupported::New());

  GetFakeService()->SetIsEventSupportedResponse(std::move(supported));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isEventSupported() {
        const result = await chrome.os.events.isEventSupported("audio_jack");
        chrome.test.assertEq(result, {
          status: 'supported'
        });

        chrome.test.succeed();
      }
    ]);
    )");

  auto unsupported = crosapi::TelemetryExtensionSupportStatus::NewUnsupported(
      crosapi::TelemetryExtensionUnsupported::New());

  GetFakeService()->SetIsEventSupportedResponse(std::move(unsupported));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isEventSupported() {
        const result = await chrome.os.events.isEventSupported("audio_jack");
        chrome.test.assertEq(result, {
          status: 'unsupported'
        });

        chrome.test.succeed();
      }
    ]);
    )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       StartListeningToEvents_Success) {
  OpenAppUiAndMakeItSecure();

  // Emit an event as soon as the subscription is registered with the fake.
  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto audio_jack_info = crosapi::TelemetryAudioJackEventInfo::New();
        audio_jack_info->state =
            crosapi::TelemetryAudioJackEventInfo::State::kAdd;
        audio_jack_info->device_type =
            crosapi::TelemetryAudioJackEventInfo::DeviceType::kHeadphone;

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kAudioJack,
            crosapi::TelemetryEventInfo::NewAudioJackEventInfo(
                std::move(audio_jack_info)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onAudioJackEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            event: 'connected',
            deviceType: 'headphone'
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("audio_jack");
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       StartListeningToEvents_ErrorPwaClosed) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        await chrome.test.assertPromiseRejects(
            chrome.os.events.startCapturingEvents("audio_jack"),
            'Error: Companion app UI is not open.'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       StartListeningToRegularEvents_SuccessPwaUnfocused) {
  OpenAppUiAndMakeItSecure();
  AddBlankTabAndShow(browser());

  // Emit an event as soon as the subscription is registered with the fake.
  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto audio_jack_info = crosapi::TelemetryAudioJackEventInfo::New();
        audio_jack_info->state =
            crosapi::TelemetryAudioJackEventInfo::State::kAdd;
        audio_jack_info->device_type =
            crosapi::TelemetryAudioJackEventInfo::DeviceType::kHeadphone;

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kAudioJack,
            crosapi::TelemetryEventInfo::NewAudioJackEventInfo(
                std::move(audio_jack_info)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onAudioJackEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            event: 'connected',
            deviceType: 'headphone'
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("audio_jack");
      }
    ]);
  )");

  base::test::TestFuture<size_t> remote_set_size;
  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this, &remote_set_size]() {
        auto* remote_set = GetFakeService()->GetObserversByCategory(
            crosapi::TelemetryEventCategoryEnum::kAudioJack);
        ASSERT_TRUE(remote_set);

        remote_set->FlushForTesting();
        remote_set_size.SetValue(remote_set->size());
      }));

  // Calling `stopCapturingEvents` will result in the connection being cut.
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function stopCapturingEvents() {
        await chrome.os.events.stopCapturingEvents("audio_jack");
        chrome.test.succeed();
      }
    ]);
  )");

  EXPECT_EQ(remote_set_size.Get(), 0UL);
}

IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionEventsApiBrowserTest,
    StartListeningToFocusRestrictedEvents_ErrorPwaUnfocused) {
  OpenAppUiAndMakeItSecure();
  AddBlankTabAndShow(browser());
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        await chrome.test.assertPromiseRejects(
            chrome.os.events.startCapturingEvents("touchpad_connected"),
            'Error: Companion app UI is not focused.'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

// TODO(b/284428237): Add more browser tests regarding focus changes.
IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionEventsApiBrowserTest,
    StartListeningToRegularAndFocusRestrictedEvents_Success) {
  OpenAppUiAndMakeItSecure();

  // Emit an event as soon as the subscription is registered with the fake.
  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto audio_jack_info = crosapi::TelemetryAudioJackEventInfo::New();
        audio_jack_info->state =
            crosapi::TelemetryAudioJackEventInfo::State::kAdd;
        audio_jack_info->device_type =
            crosapi::TelemetryAudioJackEventInfo::DeviceType::kHeadphone;

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kAudioJack,
            crosapi::TelemetryEventInfo::NewAudioJackEventInfo(
                std::move(audio_jack_info)));
      }));

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        std::vector<crosapi::TelemetryInputTouchButton> buttons{
            crosapi::TelemetryInputTouchButton::kLeft,
            crosapi::TelemetryInputTouchButton::kMiddle,
            crosapi::TelemetryInputTouchButton::kRight};

        auto connected_event =
            crosapi::TelemetryTouchpadConnectedEventInfo::New(
                1, 2, 3, std::move(buttons));

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kTouchpadConnected,
            crosapi::TelemetryEventInfo::NewTouchpadConnectedEventInfo(
                std::move(connected_event)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onAudioJackEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            event: 'connected',
            deviceType: 'headphone'
          });
        });

        chrome.os.events.onTouchpadConnectedEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            maxX: 1,
            maxY: 2,
            maxPressure: 3,
            buttons: [
              'left',
              'middle',
              'right'
            ]
          });
        });

        await chrome.os.events.startCapturingEvents("audio_jack");
        await chrome.os.events.startCapturingEvents("touchpad_connected");

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       StopListeningToEvents) {
  OpenAppUiAndMakeItSecure();

  // Emit an event as soon as the subscription is registered with the fake.
  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto audio_jack_info = crosapi::TelemetryAudioJackEventInfo::New();
        audio_jack_info->state =
            crosapi::TelemetryAudioJackEventInfo::State::kAdd;
        audio_jack_info->device_type =
            crosapi::TelemetryAudioJackEventInfo::DeviceType::kHeadphone;

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kAudioJack,
            crosapi::TelemetryEventInfo::NewAudioJackEventInfo(
                std::move(audio_jack_info)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onAudioJackEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            event: 'connected',
            deviceType: 'headphone'
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("audio_jack");
      }
    ]);
  )");

  base::test::TestFuture<size_t> remote_set_size;
  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this, &remote_set_size]() {
        auto* remote_set = GetFakeService()->GetObserversByCategory(
            crosapi::TelemetryEventCategoryEnum::kAudioJack);
        ASSERT_TRUE(remote_set);

        remote_set->FlushForTesting();
        remote_set_size.SetValue(remote_set->size());
      }));

  // Calling `stopCapturingEvents` will result in the connection being cut.
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function stopCapturingEvents() {
        await chrome.os.events.stopCapturingEvents("audio_jack");
        chrome.test.succeed();
      }
    ]);
  )");

  EXPECT_EQ(remote_set_size.Get(), 0UL);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       ClosePwaConnection) {
  OpenAppUiAndMakeItSecure();

  // Emit an event as soon as the subscription is registered with the fake.
  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto audio_jack_info = crosapi::TelemetryAudioJackEventInfo::New();
        audio_jack_info->state =
            crosapi::TelemetryAudioJackEventInfo::State::kAdd;
        audio_jack_info->device_type =
            crosapi::TelemetryAudioJackEventInfo::DeviceType::kHeadphone;

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kAudioJack,
            crosapi::TelemetryEventInfo::NewAudioJackEventInfo(
                std::move(audio_jack_info)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onAudioJackEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            event: 'connected',
            deviceType: 'headphone'
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("audio_jack");
      }
    ]);
  )");

  base::test::TestFuture<size_t> remote_set_size;
  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this, &remote_set_size]() {
        auto* remote_set = GetFakeService()->GetObserversByCategory(
            crosapi::TelemetryEventCategoryEnum::kAudioJack);
        ASSERT_TRUE(remote_set);

        remote_set->FlushForTesting();
        remote_set_size.SetValue(remote_set->size());
      }));

  // Closing the PWA will result in the connection being cut.
  browser()->tab_strip_model()->CloseSelectedTabs();

  EXPECT_EQ(remote_set_size.Get(), 0UL);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnKeyboardDiagnosticEvent_Success) {
  OpenAppUiAndMakeItSecure();

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto keyboard_info = crosapi::TelemetryKeyboardInfo::New();
        keyboard_info->id = crosapi::UInt32Value::New(1);
        keyboard_info->connection_type =
            crosapi::TelemetryKeyboardConnectionType::kBluetooth;
        keyboard_info->name = "TestName";
        keyboard_info->physical_layout =
            crosapi::TelemetryKeyboardPhysicalLayout::kChromeOS;
        keyboard_info->mechanical_layout =
            crosapi::TelemetryKeyboardMechanicalLayout::kAnsi;
        keyboard_info->region_code = "de";
        keyboard_info->number_pad_present =
            crosapi::TelemetryKeyboardNumberPadPresence::kPresent;

        auto info = crosapi::TelemetryKeyboardDiagnosticEventInfo::New();
        info->keyboard_info = std::move(keyboard_info);
        info->tested_keys = {1, 2, 3};
        info->tested_top_row_keys = {4, 5, 6};

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kKeyboardDiagnostic,
            crosapi::TelemetryEventInfo::NewKeyboardDiagnosticEventInfo(
                std::move(info)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onKeyboardDiagnosticEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            "keyboardInfo": {
              "connectionType":"bluetooth",
              "id":1,
              "mechanicalLayout":"ansi",
              "name":"TestName",
              "numberPadPresent":"present",
              "physicalLayout":"chrome_os",
              "regionCode":"de",
              "topRowKeys":[]
            },
            "testedKeys":[1,2,3],
            "testedTopRowKeys":[4,5,6]
            }
          );

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("keyboard_diagnostic");
      }
    ]);
  )");

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // This test opens a browser window UI in Ash.
  WaitUntilAtLeastOneAshBrowserWindowOpen();
#endif
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_KeyboardDiagnosticEventOpensDiagnosticApp \
  DISABLED_KeyboardDiagnosticEventOpensDiagnosticApp
#else
#define MAYBE_KeyboardDiagnosticEventOpensDiagnosticApp \
  KeyboardDiagnosticEventOpensDiagnosticApp
#endif
IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       MAYBE_KeyboardDiagnosticEventOpensDiagnosticApp) {
  OpenAppUiAndMakeItSecure();

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto keyboard_info = crosapi::TelemetryKeyboardInfo::New();
        keyboard_info->id = crosapi::UInt32Value::New(1);
        keyboard_info->connection_type =
            crosapi::TelemetryKeyboardConnectionType::kBluetooth;
        keyboard_info->name = "TestName";
        keyboard_info->physical_layout =
            crosapi::TelemetryKeyboardPhysicalLayout::kChromeOS;
        keyboard_info->mechanical_layout =
            crosapi::TelemetryKeyboardMechanicalLayout::kAnsi;
        keyboard_info->region_code = "de";
        keyboard_info->number_pad_present =
            crosapi::TelemetryKeyboardNumberPadPresence::kPresent;

        auto info = crosapi::TelemetryKeyboardDiagnosticEventInfo::New();
        info->keyboard_info = std::move(keyboard_info);
        info->tested_keys = {1, 2, 3};
        info->tested_top_row_keys = {4, 5, 6};

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kKeyboardDiagnostic,
            crosapi::TelemetryEventInfo::NewKeyboardDiagnosticEventInfo(
                std::move(info)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onKeyboardDiagnosticEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            "keyboardInfo": {
              "connectionType":"bluetooth",
              "id":1,
              "mechanicalLayout":"ansi",
              "name":"TestName",
              "numberPadPresent":"present",
              "physicalLayout":"chrome_os",
              "regionCode":"de",
              "topRowKeys":[]
            },
            "testedKeys":[1,2,3],
            "testedTopRowKeys":[4,5,6]
            }
          );

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("keyboard_diagnostic");
      }
    ]);
  )");

// If this is executed in Lacros we can stop the test here. If the above
// call succeeded, a request for opening the diagnostics application was
// sent to Ash. Since we only test Lacros, we stop the test here instead
// of checking if Ash opened the UI correctly.
// If we run in Ash however, we can check that the UI was correctly open.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_diagnostic_app_open = false;
  for (Browser* target_browser : *BrowserList::GetInstance()) {
    TabStripModel* target_tab_strip = target_browser->tab_strip_model();
    for (int i = 0; i < target_tab_strip->count(); ++i) {
      content::WebContents* target_contents =
          target_tab_strip->GetWebContentsAt(i);

      if (target_contents->GetLastCommittedURL() ==
          GURL(kKeyboardDiagnosticsUrl)) {
        is_diagnostic_app_open = true;
      }
    }
  }

  EXPECT_TRUE(is_diagnostic_app_open);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnSdCardEvent_Success) {
  OpenAppUiAndMakeItSecure();

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto sd_card_info = crosapi::TelemetrySdCardEventInfo::New();
        sd_card_info->state = crosapi::TelemetrySdCardEventInfo::State::kAdd;

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kSdCard,
            crosapi::TelemetryEventInfo::NewSdCardEventInfo(
                std::move(sd_card_info)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onSdCardEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            event: 'connected'
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("sd_card");
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnPowerEvent_Success) {
  OpenAppUiAndMakeItSecure();

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto power_info = crosapi::TelemetryPowerEventInfo::New();
        power_info->state =
            crosapi::TelemetryPowerEventInfo::State::kAcInserted;

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kPower,
            crosapi::TelemetryEventInfo::NewPowerEventInfo(
                std::move(power_info)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onPowerEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            event: 'ac_inserted'
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("power");
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnStylusGarageEvent_Success) {
  OpenAppUiAndMakeItSecure();

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto stylus_garage_info =
            crosapi::TelemetryStylusGarageEventInfo::New();
        stylus_garage_info->state =
            crosapi::TelemetryStylusGarageEventInfo::State::kInserted;

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kStylusGarage,
            crosapi::TelemetryEventInfo::NewStylusGarageEventInfo(
                std::move(stylus_garage_info)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onStylusGarageEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            event: 'inserted'
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("stylus_garage");
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnTouchpadButtonEvent_Success) {
  OpenAppUiAndMakeItSecure();

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto button_event = crosapi::TelemetryTouchpadButtonEventInfo::New();
        button_event->state =
            crosapi::TelemetryTouchpadButtonEventInfo_State::kPressed;
        button_event->button = crosapi::TelemetryInputTouchButton::kLeft;

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kTouchpadButton,
            crosapi::TelemetryEventInfo::NewTouchpadButtonEventInfo(
                std::move(button_event)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onTouchpadButtonEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            button: 'left',
            state: 'pressed'
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("touchpad_button");
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnTouchpadTouchEvent_Success) {
  OpenAppUiAndMakeItSecure();

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        std::vector<crosapi::TelemetryTouchPointInfoPtr> touch_points;
        touch_points.push_back(crosapi::TelemetryTouchPointInfo::New(
            1, 2, 3, crosapi::UInt32Value::New(4), crosapi::UInt32Value::New(5),
            crosapi::UInt32Value::New(6)));
        touch_points.push_back(crosapi::TelemetryTouchPointInfo::New(
            7, 8, 9, nullptr, nullptr, nullptr));

        auto touch_event = crosapi::TelemetryTouchpadTouchEventInfo::New(
            std::move(touch_points));

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kTouchpadTouch,
            crosapi::TelemetryEventInfo::NewTouchpadTouchEventInfo(
                std::move(touch_event)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onTouchpadTouchEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            touchPoints: [{
              trackingId: 1,
              x: 2,
              y: 3,
              pressure: 4,
              touchMajor: 5,
              touchMinor: 6
            },{
              trackingId: 7,
              x: 8,
              y: 9,
            }]
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("touchpad_touch");
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnTouchpadConnectedEvent_Success) {
  OpenAppUiAndMakeItSecure();

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        std::vector<crosapi::TelemetryInputTouchButton> buttons{
            crosapi::TelemetryInputTouchButton::kLeft,
            crosapi::TelemetryInputTouchButton::kMiddle,
            crosapi::TelemetryInputTouchButton::kRight};

        auto connected_event =
            crosapi::TelemetryTouchpadConnectedEventInfo::New(
                1, 2, 3, std::move(buttons));

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kTouchpadConnected,
            crosapi::TelemetryEventInfo::NewTouchpadConnectedEventInfo(
                std::move(connected_event)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onTouchpadConnectedEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            maxX: 1,
            maxY: 2,
            maxPressure: 3,
            buttons: [
              'left',
              'middle',
              'right'
            ]
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("touchpad_connected");
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnTouchscreenTouchEvent_Success) {
  OpenAppUiAndMakeItSecure();

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        std::vector<crosapi::TelemetryTouchPointInfoPtr> touch_points;
        touch_points.push_back(crosapi::TelemetryTouchPointInfo::New(
            1, 2, 3, crosapi::UInt32Value::New(4), crosapi::UInt32Value::New(5),
            crosapi::UInt32Value::New(6)));
        touch_points.push_back(crosapi::TelemetryTouchPointInfo::New(
            7, 8, 9, nullptr, nullptr, nullptr));

        auto touch_event = crosapi::TelemetryTouchscreenTouchEventInfo::New(
            std::move(touch_points));

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kTouchscreenTouch,
            crosapi::TelemetryEventInfo::NewTouchscreenTouchEventInfo(
                std::move(touch_event)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onTouchscreenTouchEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            touchPoints: [{
              trackingId: 1,
              x: 2,
              y: 3,
              pressure: 4,
              touchMajor: 5,
              touchMinor: 6
            },{
              trackingId: 7,
              x: 8,
              y: 9,
            }]
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("touchscreen_touch");
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnTouchscreenConnectedEvent_Success) {
  OpenAppUiAndMakeItSecure();

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto connected_event =
            crosapi::TelemetryTouchscreenConnectedEventInfo::New(1, 2, 3);

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kTouchscreenConnected,
            crosapi::TelemetryEventInfo::NewTouchscreenConnectedEventInfo(
                std::move(connected_event)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onTouchscreenConnectedEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            maxX: 1,
            maxY: 2,
            maxPressure: 3
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("touchscreen_connected");
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnExternalDisplayEvent_Success) {
  OpenAppUiAndMakeItSecure();

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto external_display_info =
            crosapi::TelemetryExternalDisplayEventInfo::New();
        external_display_info->state =
            crosapi::TelemetryExternalDisplayEventInfo::State::kAdd;

        auto display_info = crosapi::ProbeExternalDisplayInfo::New();
        display_info->display_width = 1;
        display_info->display_height = 2;
        display_info->resolution_horizontal = 3;
        display_info->resolution_vertical = 4;
        display_info->refresh_rate = 5;
        display_info->manufacturer = "manufacturer";
        display_info->model_id = 6;
        display_info->serial_number = 7;
        display_info->manufacture_week = 8;
        display_info->manufacture_year = 9;
        display_info->edid_version = "1.4";
        display_info->input_type = crosapi::ProbeDisplayInputType::kAnalog;
        display_info->display_name = "display";

        external_display_info->display_info = std::move(display_info);

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kExternalDisplay,
            crosapi::TelemetryEventInfo::NewExternalDisplayEventInfo(
                std::move(external_display_info)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onExternalDisplayEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            event: 'connected',
            displayInfo: {
                "displayHeight": 2,
                "displayName": "display",
                "displayWidth": 1,
                "edidVersion": "1.4",
                "inputType": "analog",
                "manufactureWeek": 8,
                "manufactureYear": 9,
                "manufacturer": "manufacturer",
                "modelId": 6,
                "refreshRate": 5,
                "resolutionHorizontal": 3,
                "resolutionVertical": 4
              }
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("external_display");
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnStylusConnectedEvent_Success) {
  OpenAppUiAndMakeItSecure();

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto connected_event =
            crosapi::TelemetryStylusConnectedEventInfo::New(1, 2, 3);

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kStylusConnected,
            crosapi::TelemetryEventInfo::NewStylusConnectedEventInfo(
                std::move(connected_event)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onStylusConnectedEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            max_x: 1,
            max_y: 2,
            max_pressure: 3
          });

          chrome.test.succeed();
        });

        await chrome.os.events.startCapturingEvents("stylus_connected");
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnStylusTouchEvent_Success) {
  OpenAppUiAndMakeItSecure();

  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        crosapi::TelemetryStylusTouchPointInfoPtr touch_point =
            crosapi::TelemetryStylusTouchPointInfo::New(1, 2, 3);

        auto touch_event =
            crosapi::TelemetryStylusTouchEventInfo::New(std::move(touch_point));

        GetFakeService()->EmitEventForCategory(
            crosapi::TelemetryEventCategoryEnum::kStylusTouch,
            crosapi::TelemetryEventInfo::NewStylusTouchEventInfo(
                std::move(touch_event)));
      }));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onStylusTouchEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            "touch_point": {
              x: 1,
              y: 2,
              pressure: 3
            }
          });

          chrome.test.succeed();
        });
        await chrome.os.events.startCapturingEvents("stylus_touch");
      }
    ]);
  )");
}

}  // namespace chromeos

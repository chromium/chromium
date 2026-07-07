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
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_exception.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

constexpr char kKeyboardDiagnosticsUrl[] =
    "chrome://diagnostics?input&showDefaultKeyboardTester";

}  // namespace

class TelemetryExtensionEventsApiBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 protected:
  void CheckIsEventSupported(const std::vector<std::string>& events,
                             const std::string& status);
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
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsEventSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewSupported(
              ash::cros_healthd::mojom::Supported::New()));

  std::vector<std::string> unsupported_events;
  std::vector<std::string> supported_events;

  ash::cros_healthd::mojom::EventCategoryEnum category =
      ash::cros_healthd::mojom::EventCategoryEnum::kUnmappedEnumField;
  switch (category) {
    // Features behind a feature flag.
    case ash::cros_healthd::mojom::EventCategoryEnum::kUnmappedEnumField:
    // Unused categories.
    case ash::cros_healthd::mojom::EventCategoryEnum::kAudio:
    case ash::cros_healthd::mojom::EventCategoryEnum::kBluetooth:
    case ash::cros_healthd::mojom::EventCategoryEnum::kCrash:
    case ash::cros_healthd::mojom::EventCategoryEnum::kNetwork:
    case ash::cros_healthd::mojom::EventCategoryEnum::kThunderbolt:
      [[fallthrough]];
    // Features without a feature flag.
    case ash::cros_healthd::mojom::EventCategoryEnum::kAudioJack:
      supported_events.push_back("audio_jack");
      [[fallthrough]];
    case ash::cros_healthd::mojom::EventCategoryEnum::kLid:
      supported_events.push_back("lid");
      [[fallthrough]];
    case ash::cros_healthd::mojom::EventCategoryEnum::kUsb:
      supported_events.push_back("usb");
      [[fallthrough]];
    case ash::cros_healthd::mojom::EventCategoryEnum::kExternalDisplay:
      supported_events.push_back("external_display");
      [[fallthrough]];
    case ash::cros_healthd::mojom::EventCategoryEnum::kSdCard:
      supported_events.push_back("sd_card");
      [[fallthrough]];
    case ash::cros_healthd::mojom::EventCategoryEnum::kPower:
      supported_events.push_back("power");
      [[fallthrough]];
    case ash::cros_healthd::mojom::EventCategoryEnum::kKeyboardDiagnostic:
      supported_events.push_back("keyboard_diagnostic");
      [[fallthrough]];
    case ash::cros_healthd::mojom::EventCategoryEnum::kStylusGarage:
      supported_events.push_back("stylus_garage");
      [[fallthrough]];
    case ash::cros_healthd::mojom::EventCategoryEnum::kTouchpad:
      supported_events.push_back("touchpad_button");
      supported_events.push_back("touchpad_touch");
      supported_events.push_back("touchpad_connected");
      [[fallthrough]];
    case ash::cros_healthd::mojom::EventCategoryEnum::kStylus:
      supported_events.push_back("stylus_touch");
      supported_events.push_back("stylus_connected");
      [[fallthrough]];
    case ash::cros_healthd::mojom::EventCategoryEnum::kTouchscreen:
      supported_events.push_back("touchscreen_touch");
      supported_events.push_back("touchscreen_connected");
      break;
  }

  CheckIsEventSupported(unsupported_events, "unsupported");
  CheckIsEventSupported(supported_events, "supported");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       IsEventSupported_Error) {
  auto exception = ash::cros_healthd::mojom::Exception::New();
  exception->reason = ash::cros_healthd::mojom::Exception::Reason::kUnexpected;
  exception->debug_message = "My test message";
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsEventSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewException(
              std::move(exception)));

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

  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsEventSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewUnmappedUnionField(0));

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
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsEventSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewSupported(
              ash::cros_healthd::mojom::Supported::New()));

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

  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsEventSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewUnsupported(
              ash::cros_healthd::mojom::Unsupported::New()));

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
  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      auto audio_jack_info =
          ash::cros_healthd::mojom::AudioJackEventInfo::New();
      audio_jack_info->state =
          ash::cros_healthd::mojom::AudioJackEventInfo::State::kAdd;
      audio_jack_info->device_type =
          ash::cros_healthd::mojom::AudioJackEventInfo::DeviceType::kHeadphone;

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kAudioJack,
          ash::cros_healthd::mojom::EventInfo::NewAudioJackEventInfo(
              std::move(audio_jack_info)));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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
  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      auto audio_jack_info =
          ash::cros_healthd::mojom::AudioJackEventInfo::New();
      audio_jack_info->state =
          ash::cros_healthd::mojom::AudioJackEventInfo::State::kAdd;
      audio_jack_info->device_type =
          ash::cros_healthd::mojom::AudioJackEventInfo::DeviceType::kHeadphone;

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kAudioJack,
          ash::cros_healthd::mojom::EventInfo::NewAudioJackEventInfo(
              std::move(audio_jack_info)));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

  // Calling `stopCapturingEvents` will result in the connection being cut.
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function stopCapturingEvents() {
        await chrome.os.events.stopCapturingEvents("audio_jack");
        chrome.test.succeed();
      }
    ]);
  )");

  auto* remote_set =
      ash::cros_healthd::FakeCrosHealthd::Get()->GetObserversByCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kAudioJack);
  ASSERT_TRUE(remote_set);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return remote_set->size() == 0UL; }));
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

IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionEventsApiBrowserTest,
    StartListeningToRegularAndFocusRestrictedEvents_Success) {
  OpenAppUiAndMakeItSecure();

  // Emit an event as soon as the subscription is registered with the fake.
  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      // Wait for both observers are ready.
      if (auto* remote_set =
              ash::cros_healthd::FakeCrosHealthd::Get()->GetObserversByCategory(
                  ash::cros_healthd::mojom::EventCategoryEnum::kAudioJack);
          !remote_set || remote_set->size() == 0) {
        return;
      }
      if (auto* remote_set =
              ash::cros_healthd::FakeCrosHealthd::Get()->GetObserversByCategory(
                  ash::cros_healthd::mojom::EventCategoryEnum::kTouchpad);
          !remote_set || remote_set->size() == 0) {
        return;
      }

      auto audio_jack_info =
          ash::cros_healthd::mojom::AudioJackEventInfo::New();
      audio_jack_info->state =
          ash::cros_healthd::mojom::AudioJackEventInfo::State::kAdd;
      audio_jack_info->device_type =
          ash::cros_healthd::mojom::AudioJackEventInfo::DeviceType::kHeadphone;

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kAudioJack,
          ash::cros_healthd::mojom::EventInfo::NewAudioJackEventInfo(
              std::move(audio_jack_info)));

      std::vector<ash::cros_healthd::mojom::InputTouchButton> buttons{
          ash::cros_healthd::mojom::InputTouchButton::kLeft,
          ash::cros_healthd::mojom::InputTouchButton::kMiddle,
          ash::cros_healthd::mojom::InputTouchButton::kRight};

      auto connected_event =
          ash::cros_healthd::mojom::TouchpadConnectedEvent::New(
              1, 2, 3, std::move(buttons));

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kTouchpad,
          ash::cros_healthd::mojom::EventInfo::NewTouchpadEventInfo(
              ash::cros_healthd::mojom::TouchpadEventInfo::NewConnectedEvent(
                  std::move(connected_event))));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        let audioJackWaiter = new Promise((resolve, reject) => {
          chrome.os.events.onAudioJackEvent.addListener((event) => {
            chrome.test.assertEq(event, {
              event: 'connected',
              deviceType: 'headphone'
            });
            resolve();
          });
        });

        let touchpadWaiter = new Promise((resolve, reject) => {
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
            resolve();
          });
        });

        await chrome.os.events.startCapturingEvents("audio_jack");
        await chrome.os.events.startCapturingEvents("touchpad_connected");

        await Promise.all([audioJackWaiter, touchpadWaiter]);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       StopListeningToEvents) {
  OpenAppUiAndMakeItSecure();

  // Emit an event as soon as the subscription is registered with the fake.
  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      auto audio_jack_info =
          ash::cros_healthd::mojom::AudioJackEventInfo::New();
      audio_jack_info->state =
          ash::cros_healthd::mojom::AudioJackEventInfo::State::kAdd;
      audio_jack_info->device_type =
          ash::cros_healthd::mojom::AudioJackEventInfo::DeviceType::kHeadphone;

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kAudioJack,
          ash::cros_healthd::mojom::EventInfo::NewAudioJackEventInfo(
              std::move(audio_jack_info)));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

  // Calling `stopCapturingEvents` will result in the connection being cut.
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function stopCapturingEvents() {
        await chrome.os.events.stopCapturingEvents("audio_jack");
        chrome.test.succeed();
      }
    ]);
  )");

  auto* remote_set =
      ash::cros_healthd::FakeCrosHealthd::Get()->GetObserversByCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kAudioJack);
  ASSERT_TRUE(remote_set);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return remote_set->size() == 0UL; }));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       ClosePwaConnection) {
  OpenAppUiAndMakeItSecure();

  // Emit an event as soon as the subscription is registered with the fake.
  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      auto audio_jack_info =
          ash::cros_healthd::mojom::AudioJackEventInfo::New();
      audio_jack_info->state =
          ash::cros_healthd::mojom::AudioJackEventInfo::State::kAdd;
      audio_jack_info->device_type =
          ash::cros_healthd::mojom::AudioJackEventInfo::DeviceType::kHeadphone;

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kAudioJack,
          ash::cros_healthd::mojom::EventInfo::NewAudioJackEventInfo(
              std::move(audio_jack_info)));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

  // Closing the PWA will result in the connection being cut.
  browser()->tab_strip_model()->CloseSelectedTabs();

  auto* remote_set =
      ash::cros_healthd::FakeCrosHealthd::Get()->GetObserversByCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kAudioJack);
  ASSERT_TRUE(remote_set);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return remote_set->size() == 0UL; }));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnKeyboardDiagnosticEvent_Success) {
  OpenAppUiAndMakeItSecure();

  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      auto keyboard_info = ash::diagnostics::mojom::KeyboardInfo::New();
      keyboard_info->id = 1;
      keyboard_info->connection_type =
          ash::diagnostics::mojom::ConnectionType::kBluetooth;
      keyboard_info->name = "TestName";
      keyboard_info->physical_layout =
          ash::diagnostics::mojom::PhysicalLayout::kChromeOS;
      keyboard_info->mechanical_layout =
          ash::diagnostics::mojom::MechanicalLayout::kAnsi;
      keyboard_info->region_code = "de";
      keyboard_info->number_pad_present =
          ash::diagnostics::mojom::NumberPadPresence::kPresent;

      auto info = ash::diagnostics::mojom::KeyboardDiagnosticEventInfo::New();
      info->keyboard_info = std::move(keyboard_info);
      info->tested_keys = {1, 2, 3};
      info->tested_top_row_keys = {4, 5, 6};

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kKeyboardDiagnostic,
          ash::cros_healthd::mojom::EventInfo::NewKeyboardDiagnosticEventInfo(
              std::move(info)));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startCapturingEvents() {
        chrome.os.events.onKeyboardDiagnosticEvent.addListener((event) => {
          chrome.test.assertEq(event, {
            "keyboardInfo": {
              "connectionType":"bluetooth",
              "hasAssistantKey":false,
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

  // Check that the UI was correctly open.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    bool is_diagnostic_app_open = false;
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&is_diagnostic_app_open](
            BrowserWindowInterface* browser_window_interface) {
          TabStripModel* target_tab_strip =
              browser_window_interface->GetTabStripModel();
          for (int i = 0; i < target_tab_strip->count(); ++i) {
            content::WebContents* const target_contents =
                target_tab_strip->GetWebContentsAt(i);

            if (target_contents->GetLastCommittedURL() ==
                GURL(kKeyboardDiagnosticsUrl)) {
              is_diagnostic_app_open = true;
              return false;
            }
          }
          return true;
        });
    return is_diagnostic_app_open;
  }));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       OnSdCardEvent_Success) {
  OpenAppUiAndMakeItSecure();

  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      auto sd_card_info = ash::cros_healthd::mojom::SdCardEventInfo::New();
      sd_card_info->state =
          ash::cros_healthd::mojom::SdCardEventInfo::State::kAdd;

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kSdCard,
          ash::cros_healthd::mojom::EventInfo::NewSdCardEventInfo(
              std::move(sd_card_info)));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      auto power_info = ash::cros_healthd::mojom::PowerEventInfo::New();
      power_info->state =
          ash::cros_healthd::mojom::PowerEventInfo::State::kAcInserted;

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kPower,
          ash::cros_healthd::mojom::EventInfo::NewPowerEventInfo(
              std::move(power_info)));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      auto stylus_garage_info =
          ash::cros_healthd::mojom::StylusGarageEventInfo::New();
      stylus_garage_info->state =
          ash::cros_healthd::mojom::StylusGarageEventInfo::State::kInserted;

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kStylusGarage,
          ash::cros_healthd::mojom::EventInfo::NewStylusGarageEventInfo(
              std::move(stylus_garage_info)));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      auto button_event = ash::cros_healthd::mojom::TouchpadButtonEvent::New();
      button_event->button = ash::cros_healthd::mojom::InputTouchButton::kLeft;
      button_event->pressed = true;

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kTouchpad,
          ash::cros_healthd::mojom::EventInfo::NewTouchpadEventInfo(
              ash::cros_healthd::mojom::TouchpadEventInfo::NewButtonEvent(
                  std::move(button_event))));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      std::vector<ash::cros_healthd::mojom::TouchPointInfoPtr> touch_points;
      touch_points.push_back(ash::cros_healthd::mojom::TouchPointInfo::New(
          1, 2, 3, ash::cros_healthd::mojom::NullableUint32::New(4),
          ash::cros_healthd::mojom::NullableUint32::New(5),
          ash::cros_healthd::mojom::NullableUint32::New(6)));
      touch_points.push_back(ash::cros_healthd::mojom::TouchPointInfo::New(
          7, 8, 9, nullptr, nullptr, nullptr));

      auto touch_event = ash::cros_healthd::mojom::TouchpadTouchEvent::New(
          std::move(touch_points));

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kTouchpad,
          ash::cros_healthd::mojom::EventInfo::NewTouchpadEventInfo(
              ash::cros_healthd::mojom::TouchpadEventInfo::NewTouchEvent(
                  std::move(touch_event))));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      std::vector<ash::cros_healthd::mojom::InputTouchButton> buttons{
          ash::cros_healthd::mojom::InputTouchButton::kLeft,
          ash::cros_healthd::mojom::InputTouchButton::kMiddle,
          ash::cros_healthd::mojom::InputTouchButton::kRight};

      auto connected_event =
          ash::cros_healthd::mojom::TouchpadConnectedEvent::New(
              1, 2, 3, std::move(buttons));

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kTouchpad,
          ash::cros_healthd::mojom::EventInfo::NewTouchpadEventInfo(
              ash::cros_healthd::mojom::TouchpadEventInfo::NewConnectedEvent(
                  std::move(connected_event))));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      std::vector<ash::cros_healthd::mojom::TouchPointInfoPtr> touch_points;
      touch_points.push_back(ash::cros_healthd::mojom::TouchPointInfo::New(
          1, 2, 3, ash::cros_healthd::mojom::NullableUint32::New(4),
          ash::cros_healthd::mojom::NullableUint32::New(5),
          ash::cros_healthd::mojom::NullableUint32::New(6)));
      touch_points.push_back(ash::cros_healthd::mojom::TouchPointInfo::New(
          7, 8, 9, nullptr, nullptr, nullptr));

      auto touch_event = ash::cros_healthd::mojom::TouchscreenTouchEvent::New(
          std::move(touch_points));

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kTouchscreen,
          ash::cros_healthd::mojom::EventInfo::NewTouchscreenEventInfo(
              ash::cros_healthd::mojom::TouchscreenEventInfo::NewTouchEvent(
                  std::move(touch_event))));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      auto connected_event =
          ash::cros_healthd::mojom::TouchscreenConnectedEvent::New(1, 2, 3);

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kTouchscreen,
          ash::cros_healthd::mojom::EventInfo::NewTouchscreenEventInfo(
              ash::cros_healthd::mojom::TouchscreenEventInfo::NewConnectedEvent(
                  std::move(connected_event))));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      auto external_display_info =
          ash::cros_healthd::mojom::ExternalDisplayEventInfo::New();
      external_display_info->state =
          ash::cros_healthd::mojom::ExternalDisplayEventInfo::State::kAdd;

      auto display_info = ash::cros_healthd::mojom::ExternalDisplayInfo::New();
      display_info->display_width =
          ash::cros_healthd::mojom::NullableUint32::New(1);
      display_info->display_height =
          ash::cros_healthd::mojom::NullableUint32::New(2);
      display_info->resolution_horizontal =
          ash::cros_healthd::mojom::NullableUint32::New(3);
      display_info->resolution_vertical =
          ash::cros_healthd::mojom::NullableUint32::New(4);
      display_info->refresh_rate =
          ash::cros_healthd::mojom::NullableDouble::New(5);
      display_info->manufacturer = "manufacturer";
      display_info->model_id = ash::cros_healthd::mojom::NullableUint16::New(6);
      display_info->serial_number =
          ash::cros_healthd::mojom::NullableUint32::New(7);
      display_info->manufacture_week =
          ash::cros_healthd::mojom::NullableUint8::New(8);
      display_info->manufacture_year =
          ash::cros_healthd::mojom::NullableUint16::New(9);
      display_info->edid_version = "1.4";
      display_info->input_type =
          ash::cros_healthd::mojom::DisplayInputType::kAnalog;
      display_info->display_name = "display";

      external_display_info->display_info = std::move(display_info);

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kExternalDisplay,
          ash::cros_healthd::mojom::EventInfo::NewExternalDisplayEventInfo(
              std::move(external_display_info)));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      auto connected_event =
          ash::cros_healthd::mojom::StylusConnectedEvent::New(1, 2, 3);

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kStylus,
          ash::cros_healthd::mojom::EventInfo::NewStylusEventInfo(
              ash::cros_healthd::mojom::StylusEventInfo::NewConnectedEvent(
                  std::move(connected_event))));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnEventObserverAdded() override {
      ash::cros_healthd::mojom::StylusTouchPointInfoPtr touch_point =
          ash::cros_healthd::mojom::StylusTouchPointInfo::New(
              1, 2, ash::cros_healthd::mojom::NullableUint32::New(3));

      auto touch_event = ash::cros_healthd::mojom::StylusTouchEvent::New(
          std::move(touch_point));

      ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
          ash::cros_healthd::mojom::EventCategoryEnum::kStylus,
          ash::cros_healthd::mojom::EventInfo::NewStylusEventInfo(
              ash::cros_healthd::mojom::StylusEventInfo::NewTouchEvent(
                  std::move(touch_event))));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation{&observer};
  observation.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

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

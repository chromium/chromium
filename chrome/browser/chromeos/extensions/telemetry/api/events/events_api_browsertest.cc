// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/location.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service.h"
#include "chrome/test/base/ui_test_utils.h"
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
#include "chrome/browser/ash/telemetry_extension/events/telemetry_event_service_ash.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service_factory.h"
#include "chrome/browser/profiles/profile.h"         // nogncheck
#include "chrome/browser/ui/browser_list.h"          // nogncheck
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kKeyboardDiagnosticsUrl[] = "chrome://diagnostics?input";
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

 protected:
  FakeEventsService* GetFakeService() {
    return fake_events_service_impl_.get();
  }

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // SAFETY: This pointer is owned in a unique_ptr by the EventManager. Since
  // the EventManager lives longer than this test, it is always safe to access
  // the fake in the test body.
  raw_ptr<FakeEventsService, base::RawPtrTraits::kMayDangle>
      fake_events_service_impl_;
  FakeEventsServiceFactory fake_events_service_factory_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<FakeEventsService> fake_events_service_impl_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

// Checks that the correct events are available. This checks all released events
// that are not behind a feature flag.
IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       CheckCorrectEventsAvailable) {
  constexpr char kEnabledEvents[] =
      "['onAudioJackEvent', 'onLidEvent', 'onUsbEvent']";

  CreateExtensionAndRunServiceWorker(base::StringPrintf(R"(
    chrome.test.runTests([
      function checkSupportedEvents() {
        const methods = Object.getOwnPropertyNames(chrome.os.events)
            .filter(item =>
               typeof chrome.os.events[item].addListener === 'function');

        chrome.test.assertEq(methods.sort(), %s.sort());
        chrome.test.succeed();
      }
    ]);
    )",
                                                        kEnabledEvents));
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
  // Open the PWA.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url())));

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
            'Error: Companion PWA UI is not open.'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       StopListeningToEvents) {
  // Open the PWA.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url())));

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
  // Open the PWA.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url())));

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
                       CheckSdCardApiWithoutFeatureFlagFail) {
  // Open the PWA.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url())));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      function sdCardNotWorking() {
        chrome.test.assertThrows(() => {
          chrome.os.events.onSdCardEvent.addListener((event) => {
            // unreachable.
          });
        }, [],
          'Cannot read properties of undefined (reading \'addListener\')'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       CheckPowerApiWithoutFeatureFlagFail) {
  // Open the PWA.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url())));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      function powerNotWorking() {
        chrome.test.assertThrows(() => {
          chrome.os.events.onPowerEvent.addListener((event) => {
            // unreachable.
          });
        }, [],
          'Cannot read properties of undefined (reading \'addListener\')'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionEventsApiBrowserTest,
                       CheckKeyboardDiagnosticApiWithoutFeatureFlagFail) {
  // Open the PWA.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url())));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      function sdCardNotWorking() {
        chrome.test.assertThrows(() => {
          chrome.os.events.onKeyboardDiagnosticEvent.addListener((event) => {
            // unreachable.
          });
        }, [],
          'Cannot read properties of undefined (reading \'addListener\')'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

class PendingApprovalTelemetryExtensionEventsApiBrowserTest
    : public TelemetryExtensionEventsApiBrowserTest {
 public:
  PendingApprovalTelemetryExtensionEventsApiBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kTelemetryExtensionPendingApprovalApi);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PendingApprovalTelemetryExtensionEventsApiBrowserTest,
                       CheckSdCardApiWithFeatureFlagWork) {
  // Open the PWA.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url())));

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

IN_PROC_BROWSER_TEST_F(PendingApprovalTelemetryExtensionEventsApiBrowserTest,
                       CheckPowerApiWithFeatureFlagWork) {
  // Open the PWA.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url())));

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

IN_PROC_BROWSER_TEST_F(PendingApprovalTelemetryExtensionEventsApiBrowserTest,
                       CheckKeyboardDiagnosticApiWithFeatureFlagWork) {
  // Open the PWA.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url())));

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
}

IN_PROC_BROWSER_TEST_F(PendingApprovalTelemetryExtensionEventsApiBrowserTest,
                       KeyboardDiagnosticEventOpensDiagnosticApp) {
  // Open the PWA.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url())));

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
  for (auto* target_browser : *BrowserList::GetInstance()) {
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

}  // namespace chromeos

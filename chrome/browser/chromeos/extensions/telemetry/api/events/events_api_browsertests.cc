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
#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/telemetry_extension/telemetry_event_service_ash.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

class PendingApprovalTelemetryExtensionEventsApiBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  PendingApprovalTelemetryExtensionEventsApiBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kTelemetryExtensionPendingApprovalApi);
  }

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
  std::string GetManifestFile(const std::string& matches_origin) override {
    return base::StringPrintf(R"(
      {
        "key": "%s",
        "name": "Test Telemetry Extension",
        "version": "1",
        "manifest_version": 3,
        "chromeos_system_extension": {},
        "background": {
          "service_worker": "sw.js"
        },
        "permissions": [
          "os.diagnostics",
          "os.events",
          "os.telemetry",
          "os.telemetry.serial_number",
          "os.telemetry.network_info"
        ],
        "externally_connectable": {
          "matches": [
            "%s"
          ]
        },
        "options_page": "options.html"
      }
    )",
                              public_key().c_str(), matches_origin.c_str());
  }

  FakeEventsService* GetFakeService() {
    return fake_events_service_impl_.get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

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

IN_PROC_BROWSER_TEST_F(PendingApprovalTelemetryExtensionEventsApiBrowserTest,
                       IsEventSupported_Error) {
  auto exception = crosapi::mojom::TelemetryExtensionException::New();
  exception->reason =
      crosapi::mojom::TelemetryExtensionException::Reason::kUnexpected;
  exception->debug_message = "My test message";

  auto input = crosapi::mojom::TelemetryExtensionSupportStatus::NewException(
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
      crosapi::mojom::TelemetryExtensionSupportStatus::NewUnmappedUnionField(0);
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

IN_PROC_BROWSER_TEST_F(PendingApprovalTelemetryExtensionEventsApiBrowserTest,
                       IsEventSupported_Success) {
  auto supported =
      crosapi::mojom::TelemetryExtensionSupportStatus::NewSupported(
          crosapi::mojom::TelemetryExtensionSupported::New());

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

  auto unsupported =
      crosapi::mojom::TelemetryExtensionSupportStatus::NewUnsupported(
          crosapi::mojom::TelemetryExtensionUnsupported::New());

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

IN_PROC_BROWSER_TEST_F(PendingApprovalTelemetryExtensionEventsApiBrowserTest,
                       StartListeningToEvents_Success) {
  // Open the PWA.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url())));

  // Emit an event as soon as the subscription is registered with the fake.
  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto audio_jack_info =
            crosapi::mojom::TelemetryAudioJackEventInfo::New();
        audio_jack_info->state =
            crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd;
        audio_jack_info->device_type =
            crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::kHeadphone;

        GetFakeService()->EmitEventForCategory(
            crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack,
            crosapi::mojom::TelemetryEventInfo::NewAudioJackEventInfo(
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

IN_PROC_BROWSER_TEST_F(PendingApprovalTelemetryExtensionEventsApiBrowserTest,
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

IN_PROC_BROWSER_TEST_F(PendingApprovalTelemetryExtensionEventsApiBrowserTest,
                       StopListeningToEvents) {
  // Open the PWA.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url())));

  // Emit an event as soon as the subscription is registered with the fake.
  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto audio_jack_info =
            crosapi::mojom::TelemetryAudioJackEventInfo::New();
        audio_jack_info->state =
            crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd;
        audio_jack_info->device_type =
            crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::kHeadphone;

        GetFakeService()->EmitEventForCategory(
            crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack,
            crosapi::mojom::TelemetryEventInfo::NewAudioJackEventInfo(
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
            crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack);
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

IN_PROC_BROWSER_TEST_F(PendingApprovalTelemetryExtensionEventsApiBrowserTest,
                       ClosePwaConnection) {
  // Open the PWA.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(pwa_page_url())));

  // Emit an event as soon as the subscription is registered with the fake.
  GetFakeService()->SetOnSubscriptionChange(
      base::BindLambdaForTesting([this]() {
        auto audio_jack_info =
            crosapi::mojom::TelemetryAudioJackEventInfo::New();
        audio_jack_info->state =
            crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd;
        audio_jack_info->device_type =
            crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::kHeadphone;

        GetFakeService()->EmitEventForCategory(
            crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack,
            crosapi::mojom::TelemetryEventInfo::NewAudioJackEventInfo(
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
            crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack);
        ASSERT_TRUE(remote_set);

        remote_set->FlushForTesting();
        remote_set_size.SetValue(remote_set->size());
      }));

  // Closing the PWA will result in the connection being cut.
  browser()->tab_strip_model()->CloseSelectedTabs();

  EXPECT_EQ(remote_set_size.Get(), 0UL);
}

}  // namespace chromeos

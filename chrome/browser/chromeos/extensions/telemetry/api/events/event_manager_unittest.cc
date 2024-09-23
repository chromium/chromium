// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/app_ui_observer.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_router.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service_factory.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

}

class TelemetryExtensionEventManagerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_events_service_factory_.SetCreateInstanceResponse(
        std::make_unique<FakeEventsService>());
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
  void OpenAppUiUrlAndSetCertificateWithStatus(const GURL& url,
                                               net::CertStatus cert_status) {
    AddTab(browser(), url);

    // AddTab() adds a new tab at index 0.
    auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
    SetCertificateWithStatus(web_contents, cert_status);
  }

  void SetCertificateWithStatus(content::WebContents* web_contents,
                                net::CertStatus cert_status) {
    const base::FilePath certs_dir = net::GetTestCertsDirectory();
    scoped_refptr<net::X509Certificate> test_cert(
        net::ImportCertFromFile(certs_dir, "ok_cert.pem"));
    ASSERT_TRUE(test_cert);

    auto* entry = web_contents->GetController().GetVisibleEntry();
    content::SSLStatus& ssl = entry->GetSSL();
    ssl.certificate = test_cert;
    ssl.cert_status = cert_status;
  }

  scoped_refptr<const extensions::Extension> CreateExtension(
      const std::string& extension_id,
      const std::vector<std::string> external_connectables) {
    base::Value::List matches;
    for (const auto& match : external_connectables) {
      matches.Append(match);
    }
    auto extension =
        extensions::ExtensionBuilder("Test ChromeOS System Extension")
            .SetManifestVersion(3)
            .SetManifestKey("chromeos_system_extension", base::Value::Dict())
            .SetManifestKey(
                "externally_connectable",
                base::Value::Dict().Set("matches", std::move(matches)))
            .SetID(extension_id)
            .SetLocation(extensions::mojom::ManifestLocation::kInternal)
            .Build();
    extensions::ExtensionRegistry::Get(profile())->AddEnabled(extension);

    return extension;
  }

  void SimulateFocusEvent(const std::string& extension_id, bool is_focused) {
    // Change active tab with `browser()->tab_strip_model()->ActivateTabAt()` or
    // `AddPage()` will not trigger focus events. Fire those manually instead.
    if (is_focused) {
      event_manager()->app_ui_observers_[extension_id]->OnWebContentsFocused(
          nullptr);
    } else {
      event_manager()->app_ui_observers_[extension_id]->OnWebContentsLostFocus(
          nullptr);
    }
  }

  EventManager* event_manager() { return EventManager::Get(profile()); }

  base::flat_map<extensions::ExtensionId, std::unique_ptr<AppUiObserver>>&
  app_ui_observers() {
    return event_manager()->app_ui_observers_;
  }

  EventRouter& event_router() { return event_manager()->event_router_; }

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  FakeEventsServiceFactory fake_events_service_factory_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<FakeEventsService> fake_events_service_impl_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};      // namespace chromeos

TEST_F(TelemetryExtensionEventManagerTest, RegisterEventNoExtension) {
  EXPECT_EQ(EventManager::kAppUiClosed,
            event_manager()->RegisterExtensionForEvent(
                "gogonhoemckpdpadfnjnpgbjpbjnodgc",
                crosapi::TelemetryEventCategoryEnum::kAudioJack));
}

TEST_F(TelemetryExtensionEventManagerTest, RegisterEventAppUiClosed) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  EXPECT_EQ(EventManager::kAppUiClosed,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
}

TEST_F(TelemetryExtensionEventManagerTest, RegisterEventSuccess) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));

  // Closing the tab cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularEventAppUiOpenButUnfocusByNewTabSuccess) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  AddTab(browser(), GURL("https://example.com/"));
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Closing the tab cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularEventAppUiOpenButUnfocusByNewWindowSuccess) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  auto new_window = CreateBrowserWindow();
  auto new_browser = CreateBrowser(GetProfile(), Browser::Type::TYPE_NORMAL,
                                   false, new_window.get());
  BrowserList::SetLastActive(new_browser.get());

  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Closing the tab cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));

  new_browser.reset();
  new_window.reset();
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestrictedEventAppUiOpenButUnfocusByNewTabFail) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  AddTab(browser(), GURL("https://example.com/"));
  EXPECT_EQ(EventManager::kAppUiNotFocused,
            event_manager()->RegisterExtensionForEvent(
                extension_id,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));

  // Closing the tab should not change the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestrictedEventAppUiOpenButUnfocusByNewWindowFail) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  auto new_window = CreateBrowserWindow();
  auto new_browser = CreateBrowser(GetProfile(), Browser::Type::TYPE_NORMAL,
                                   false, new_window.get());
  BrowserList::SetLastActive(new_browser.get());

  EXPECT_EQ(EventManager::kAppUiNotFocused,
            event_manager()->RegisterExtensionForEvent(
                extension_id,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));

  // Closing the tab should not change the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));

  new_browser.reset();
  new_window.reset();
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularEventAppUiSwitchFocusSuccess) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Regular events are not affected by focus changes.
  SimulateFocusEvent(extension_id, /*is_focused=*/false);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  SimulateFocusEvent(extension_id, /*is_focused=*/true);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Closing the tab cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestrictedEventAppUiSwitchFocusSuccess) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  SimulateFocusEvent(extension_id, /*is_focused=*/false);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_FALSE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  SimulateFocusEvent(extension_id, /*is_focused=*/true);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Closing the tab cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularAndFocusRestrictedEventWithAppUiSwitchFocusSuccess) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  auto regular_event_type = crosapi::TelemetryEventCategoryEnum::kAudioJack;
  auto restricted_event_type =
      crosapi::TelemetryEventCategoryEnum::kTouchpadConnected;
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess, event_manager()->RegisterExtensionForEvent(
                                        extension_id, regular_event_type));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(extension_id,
                                                           regular_event_type));

  EXPECT_EQ(EventManager::kSuccess, event_manager()->RegisterExtensionForEvent(
                                        extension_id, restricted_event_type));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, restricted_event_type));

  SimulateFocusEvent(extension_id, /*is_focused=*/false);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(extension_id,
                                                           regular_event_type));
  EXPECT_FALSE(event_router().IsExtensionAllowedForCategory(
      extension_id, restricted_event_type));

  SimulateFocusEvent(extension_id, /*is_focused=*/true);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(extension_id,
                                                           regular_event_type));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, restricted_event_type));

  // Closing the tab cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularEventTwoTimesSuccess) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Second register will still succeed.
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestrictedEventTwoTimesSuccess) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Second register will still succeed.
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularEventMultipleTabsOpenSuccess) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));

  // Open second tab.
  SimulateFocusEvent(extension_id, /*is_focused=*/false);
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Close the first tab (index 1). The observer shouldn't be cut.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Closing the second tab (the last one) cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestricedEventMultipleTabsOpenSuccess) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Open second tab. As the focus-restricted event is originated from the first
  // tab, the event is now blocked.
  SimulateFocusEvent(extension_id, /*is_focused=*/false);
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_FALSE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Try to observe the same event in the second tab. The event should now be
  // unblocked.
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Close the first tab (index 1). The observer shouldn't be cut.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Closing the second tab (the last one) cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularAndFocusRestricedEventMultipleTabsOpenSuccess) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  auto regular_event_type = crosapi::TelemetryEventCategoryEnum::kAudioJack;
  auto restricted_event_type =
      crosapi::TelemetryEventCategoryEnum::kTouchpadConnected;
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess, event_manager()->RegisterExtensionForEvent(
                                        extension_id, regular_event_type));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(extension_id,
                                                           regular_event_type));

  // Open second tab.
  SimulateFocusEvent(extension_id, /*is_focused=*/false);
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(extension_id,
                                                           regular_event_type));

  // Try to observe a focus-restricted event in the second tab.
  EXPECT_EQ(EventManager::kSuccess, event_manager()->RegisterExtensionForEvent(
                                        extension_id, restricted_event_type));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(extension_id,
                                                           regular_event_type));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, restricted_event_type));

  // Close the first tab (index 1). The observer shouldn't be cut.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, restricted_event_type));

  // Closing the second tab (the last one) cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest, RegisterEventAppUiNotSecure) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  // This not secure page shouldn't allow the event to be observed.
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::CERT_STATUS_INVALID);
  EXPECT_EQ(EventManager::kAppUiClosed,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));

  // Add a valid tab.
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Close the secure one will cause the EventManager stop observing events.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest, RegisterRegularEventNavigateOut) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Navigation in the same domain shouldn't affect the observation.
  NavigateAndCommitActiveTab(
      GURL("https://googlechromelabs.github.io/example/path"));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Navigation to other URL should cut the observation.
  NavigateAndCommitActiveTab(GURL("https://example.com/"));
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestrictedEventNavigateOut) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Navigation in the same domain shouldn't affect the observation.
  NavigateAndCommitActiveTab(
      GURL("https://googlechromelabs.github.io/example/path"));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Navigation to other URL should cut the observation.
  NavigateAndCommitActiveTab(GURL("https://example.com/"));
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest, RegisterRegularEventTwoExtension) {
  const extensions::ExtensionId extension_id_1 =
      "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id_1, {"*://googlechromelabs.github.io/*"});
  const extensions::ExtensionId extension_id_2 =
      "alnedpmllcfpgldkagbfbjkloonjlfjb";
  CreateExtension(extension_id_2, {"https://hpcs-appschr.hpcloud.hp.com/*"});

  // Open app UI for extension 1.
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          extension_id_1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_EQ(
      EventManager::kAppUiClosed,
      event_manager()->RegisterExtensionForEvent(
          extension_id_2, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id_1));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id_1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id_1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_FALSE(app_ui_observers().contains(extension_id_2));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id_2));

  // Open app UI for extension 2.
  SimulateFocusEvent(extension_id_1, /*is_focused=*/false);
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://hpcs-appschr.hpcloud.hp.com"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          extension_id_2, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id_1));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id_1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id_1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id_2));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id_2));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id_2, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Close the app UI of extension 1.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id_1));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id_1));
  EXPECT_TRUE(app_ui_observers().contains(extension_id_2));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id_2));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id_2, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Close the app UI of extension 2.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id_1));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id_1));
  EXPECT_FALSE(app_ui_observers().contains(extension_id_2));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id_2));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestrictedEventTwoExtension) {
  const extensions::ExtensionId extension_id_1 =
      "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(extension_id_1, {"*://googlechromelabs.github.io/*"});
  const extensions::ExtensionId extension_id_2 =
      "alnedpmllcfpgldkagbfbjkloonjlfjb";
  CreateExtension(extension_id_2, {"https://hpcs-appschr.hpcloud.hp.com/*"});

  // Open app UI for extension 1.
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id_1,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_EQ(EventManager::kAppUiNotFocused,
            event_manager()->RegisterExtensionForEvent(
                extension_id_2,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(extension_id_1));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id_1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id_1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_FALSE(app_ui_observers().contains(extension_id_2));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id_2));

  // Open app UI for extension 2.
  SimulateFocusEvent(extension_id_1, /*is_focused=*/false);
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://hpcs-appschr.hpcloud.hp.com"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id_2,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(extension_id_1));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id_1));
  EXPECT_FALSE(event_router().IsExtensionAllowedForCategory(
      extension_id_1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(extension_id_2));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id_2));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id_2, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Close the app UI of extension 1.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id_1));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id_1));
  EXPECT_TRUE(app_ui_observers().contains(extension_id_2));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id_2));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id_2, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Close the app UI of extension 2.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id_1));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id_1));
  EXPECT_FALSE(app_ui_observers().contains(extension_id_2));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id_2));
}

TEST_F(TelemetryExtensionEventManagerTest, RemoveExtensionCutsConnection) {
  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  auto extension =
      CreateExtension(extension_id, {"*://googlechromelabs.github.io/*"});

  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));

  EXPECT_TRUE(extensions::ExtensionRegistry::Get(profile())->RemoveEnabled(
      extension_id));
  extensions::ExtensionRegistry::Get(profile())->TriggerOnUnloaded(
      extension.get(), extensions::UnloadedExtensionReason::TERMINATE);

  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(TelemetryExtensionEventManagerTest, RegisterEventIWASuccess) {
  auto info = ScopedChromeOSSystemExtensionInfo::CreateForTesting();
  // TODO(b/293560424): Remove this override after we add some valid IWA id to
  // the allowlist.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kTelemetryExtensionIwaIdOverrideForTesting,
      "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic");
  info->ApplyCommandLineSwitchesForTesting();

  const std::string extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  CreateExtension(
      extension_id,
      {"*://googlechromelabs.github.io/*",
       "isolated-app://"
       "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic/*"});

  // Open PWA and start observing events.
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Open IWA.
  AddTab(browser(), GURL("about:blank"));
  auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  web_app::SimulateIsolatedWebAppNavigation(
      web_contents,
      GURL("isolated-app://"
           "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic"));
  SetCertificateWithStatus(web_contents, net::OK);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Close the PWA. This shouldn't affect the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Close the IWA (last tab) should cut the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace chromeos

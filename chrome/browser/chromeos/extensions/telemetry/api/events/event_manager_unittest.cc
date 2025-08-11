// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/app_ui_observer.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_router.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_ash.h"
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

namespace chromeos {

namespace {
namespace crosapi = ::crosapi::mojom;

constexpr char kExtensionId1[] = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
constexpr char kPwaPattern1[] =
    "*://googlechromelabs.github.io/cros-sample-telemetry-extension/test-page/"
    "*";
constexpr char kPwaUrl1[] =
    "https://googlechromelabs.github.io/cros-sample-telemetry-extension/"
    "test-page";
constexpr char kPwaUrl1SameDomain[] =
    "https://googlechromelabs.github.io/cros-sample-telemetry-extension/"
    "test-page";

constexpr char kExtensionId2[] = "alnedpmllcfpgldkagbfbjkloonjlfjb";
constexpr char kPwaPattern2[] = "https://hpcs-appschr.hpcloud.hp.com/*";
constexpr char kPwaUrl2[] = "https://hpcs-appschr.hpcloud.hp.com";

constexpr char kNotMatchedPwaUrl[] = "https://example.com";

}  // namespace

class TelemetryExtensionEventManagerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    fake_events_service_factory_.SetCreateInstanceResponse(
        std::make_unique<FakeEventsService>());
    ash::TelemetryEventServiceAsh::Factory::SetForTesting(
        &fake_events_service_factory_);
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
  FakeEventsServiceFactory fake_events_service_factory_;
};

TEST_F(TelemetryExtensionEventManagerTest, RegisterEventNoExtension) {
  EXPECT_EQ(
      EventManager::kAppUiClosed,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
}

TEST_F(TelemetryExtensionEventManagerTest, RegisterEventAppUiClosed) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  EXPECT_EQ(
      EventManager::kAppUiClosed,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
}

TEST_F(TelemetryExtensionEventManagerTest, RegisterEventSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));

  // Closing the tab cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularEventAppUiOpenButUnfocusByNewTabSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  AddTab(browser(), GURL(kNotMatchedPwaUrl));
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Closing the tab cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularEventAppUiOpenButUnfocusByNewWindowSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  auto new_browser =
      CreateBrowser(GetProfile(), Browser::Type::TYPE_NORMAL, false);
  BrowserList::SetLastActive(new_browser.get());

  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Closing the tab cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));

  new_browser.reset();
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestrictedEventAppUiOpenButUnfocusByNewTabFail) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  AddTab(browser(), GURL(kNotMatchedPwaUrl));
  EXPECT_EQ(EventManager::kAppUiNotFocused,
            event_manager()->RegisterExtensionForEvent(
                kExtensionId1,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));

  // Closing the tab should not change the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestrictedEventAppUiOpenButUnfocusByNewWindowFail) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  auto new_browser =
      CreateBrowser(GetProfile(), Browser::Type::TYPE_NORMAL, false);
  BrowserList::SetLastActive(new_browser.get());

  EXPECT_EQ(EventManager::kAppUiNotFocused,
            event_manager()->RegisterExtensionForEvent(
                kExtensionId1,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));

  // Closing the tab should not change the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));

  new_browser.reset();
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularEventAppUiSwitchFocusSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Regular events are not affected by focus changes.
  SimulateFocusEvent(kExtensionId1, /*is_focused=*/false);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  SimulateFocusEvent(kExtensionId1, /*is_focused=*/true);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Closing the tab cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestrictedEventAppUiSwitchFocusSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                kExtensionId1,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  SimulateFocusEvent(kExtensionId1, /*is_focused=*/false);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  SimulateFocusEvent(kExtensionId1, /*is_focused=*/true);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Closing the tab cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularAndFocusRestrictedEventWithAppUiSwitchFocusSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  auto regular_event_type = crosapi::TelemetryEventCategoryEnum::kAudioJack;
  auto restricted_event_type =
      crosapi::TelemetryEventCategoryEnum::kTouchpadConnected;
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess, event_manager()->RegisterExtensionForEvent(
                                        kExtensionId1, regular_event_type));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(kExtensionId1,
                                                           regular_event_type));

  EXPECT_EQ(EventManager::kSuccess, event_manager()->RegisterExtensionForEvent(
                                        kExtensionId1, restricted_event_type));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, restricted_event_type));

  SimulateFocusEvent(kExtensionId1, /*is_focused=*/false);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(kExtensionId1,
                                                           regular_event_type));
  EXPECT_FALSE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, restricted_event_type));

  SimulateFocusEvent(kExtensionId1, /*is_focused=*/true);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(kExtensionId1,
                                                           regular_event_type));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, restricted_event_type));

  // Closing the tab cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularEventTwoTimesSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Second register will still succeed.
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestrictedEventTwoTimesSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                kExtensionId1,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Second register will still succeed.
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                kExtensionId1,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularEventMultipleTabsOpenSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));

  // Open second tab.
  SimulateFocusEvent(kExtensionId1, /*is_focused=*/false);
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Close the first tab (index 1). The observer shouldn't be cut.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Closing the second tab (the last one) cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestricedEventMultipleTabsOpenSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                kExtensionId1,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Open second tab. As the focus-restricted event is originated from the first
  // tab, the event is now blocked.
  SimulateFocusEvent(kExtensionId1, /*is_focused=*/false);
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Try to observe the same event in the second tab. The event should now be
  // unblocked.
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                kExtensionId1,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Close the first tab (index 1). The observer shouldn't be cut.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Closing the second tab (the last one) cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterRegularAndFocusRestricedEventMultipleTabsOpenSuccess) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  auto regular_event_type = crosapi::TelemetryEventCategoryEnum::kAudioJack;
  auto restricted_event_type =
      crosapi::TelemetryEventCategoryEnum::kTouchpadConnected;
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess, event_manager()->RegisterExtensionForEvent(
                                        kExtensionId1, regular_event_type));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(kExtensionId1,
                                                           regular_event_type));

  // Open second tab.
  SimulateFocusEvent(kExtensionId1, /*is_focused=*/false);
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(kExtensionId1,
                                                           regular_event_type));

  // Try to observe a focus-restricted event in the second tab.
  EXPECT_EQ(EventManager::kSuccess, event_manager()->RegisterExtensionForEvent(
                                        kExtensionId1, restricted_event_type));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(kExtensionId1,
                                                           regular_event_type));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, restricted_event_type));

  // Close the first tab (index 1). The observer shouldn't be cut.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, restricted_event_type));

  // Closing the second tab (the last one) cuts the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

TEST_F(TelemetryExtensionEventManagerTest, RegisterEventAppUiNotSecure) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  // This not secure page shouldn't allow the event to be observed.
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL(kPwaUrl1),
      /*cert_status=*/net::CERT_STATUS_INVALID);
  EXPECT_EQ(
      EventManager::kAppUiClosed,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));

  // Add a valid tab.
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Close the secure one will cause the EventManager stop observing events.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

TEST_F(TelemetryExtensionEventManagerTest, RegisterRegularEventNavigateOut) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Navigation in the same domain shouldn't affect the observation.
  NavigateAndCommitActiveTab(GURL(kPwaUrl1SameDomain));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Navigation to other URL should cut the observation.
  NavigateAndCommitActiveTab(GURL(kNotMatchedPwaUrl));
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestrictedEventNavigateOut) {
  CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                kExtensionId1,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Navigation in the same domain shouldn't affect the observation.
  NavigateAndCommitActiveTab(GURL(kPwaUrl1SameDomain));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Navigation to other URL should cut the observation.
  NavigateAndCommitActiveTab(GURL(kNotMatchedPwaUrl));
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

TEST_F(TelemetryExtensionEventManagerTest, RegisterRegularEventTwoExtension) {
  CreateExtension(kExtensionId1, {kPwaPattern1});
  CreateExtension(kExtensionId2, {kPwaPattern2});

  // Open app UI for extension 1.
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_EQ(
      EventManager::kAppUiClosed,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId2, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId2));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId2));

  // Open app UI for extension 2.
  SimulateFocusEvent(kExtensionId1, /*is_focused=*/false);
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl2),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId2, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId2));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId2));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId2, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Close the app UI of extension 1.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId2));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId2));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId2, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Close the app UI of extension 2.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId2));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId2));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterFocusRestrictedEventTwoExtension) {
  CreateExtension(kExtensionId1, {kPwaPattern1});
  CreateExtension(kExtensionId2, {kPwaPattern2});

  // Open app UI for extension 1.
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                kExtensionId1,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_EQ(EventManager::kAppUiNotFocused,
            event_manager()->RegisterExtensionForEvent(
                kExtensionId2,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId2));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId2));

  // Open app UI for extension 2.
  SimulateFocusEvent(kExtensionId1, /*is_focused=*/false);
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl2),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                kExtensionId2,
                crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId2));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId2));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId2, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Close the app UI of extension 1.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId2));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId2));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId2, crosapi::TelemetryEventCategoryEnum::kTouchpadConnected));

  // Close the app UI of extension 2.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId2));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId2));
}

TEST_F(TelemetryExtensionEventManagerTest, RemoveExtensionCutsConnection) {
  auto extension = CreateExtension(kExtensionId1, {kPwaPattern1});

  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));

  EXPECT_TRUE(extensions::ExtensionRegistry::Get(profile())->RemoveEnabled(
      kExtensionId1));
  extensions::ExtensionRegistry::Get(profile())->TriggerOnUnloaded(
      extension.get(), extensions::UnloadedExtensionReason::TERMINATE);

  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

TEST_F(TelemetryExtensionEventManagerTest, RegisterEventIWASuccess) {
  auto info = ScopedChromeOSSystemExtensionInfo::CreateForTesting();
  // TODO(b/293560424): Remove this override after we add some valid IWA id to
  // the allowlist.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kTelemetryExtensionIwaIdOverrideForTesting,
      "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic");
  info->ApplyCommandLineSwitchesForTesting();

  CreateExtension(
      kExtensionId1,
      {kPwaPattern1,
       "isolated-app://"
       "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic/*"});

  // Open PWA and start observing events.
  OpenAppUiUrlAndSetCertificateWithStatus(GURL(kPwaUrl1),
                                          /*cert_status=*/net::OK);
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Open IWA.
  AddTab(browser(), GURL("about:blank"));
  auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  web_app::SimulateIsolatedWebAppNavigation(
      web_contents,
      GURL("isolated-app://"
           "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic"));
  SetCertificateWithStatus(web_contents, net::OK);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Close the PWA. This shouldn't affect the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(app_ui_observers().contains(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionObserving(kExtensionId1));
  EXPECT_TRUE(event_router().IsExtensionAllowedForCategory(
      kExtensionId1, crosapi::TelemetryEventCategoryEnum::kAudioJack));

  // Close the IWA (last tab) should cut the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(kExtensionId1));
  EXPECT_FALSE(event_router().IsExtensionObserving(kExtensionId1));
}

}  // namespace chromeos

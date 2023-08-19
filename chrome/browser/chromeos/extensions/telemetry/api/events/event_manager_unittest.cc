// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_manager.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_router.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/telemetry_extension/events/telemetry_event_service_ash.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service_factory.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "chromeos/constants/chromeos_features.h"
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
    const base::FilePath certs_dir = net::GetTestCertsDirectory();
    scoped_refptr<net::X509Certificate> test_cert(
        net::ImportCertFromFile(certs_dir, "ok_cert.pem"));
    ASSERT_TRUE(test_cert);

    AddTab(browser(), url);

    // AddTab() adds a new tab at index 0.
    auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
    auto* entry = web_contents->GetController().GetVisibleEntry();
    content::SSLStatus& ssl = entry->GetSSL();
    ssl.certificate = test_cert;
    ssl.cert_status = cert_status;
  }

  void CreateExtension(const std::string& extension_id,
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

TEST_F(TelemetryExtensionEventManagerTest, RegisterEventSuccessSecondTimes) {
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

  // Second register will still success.
  EXPECT_EQ(EventManager::kSuccess,
            event_manager()->RegisterExtensionForEvent(
                extension_id, crosapi::TelemetryEventCategoryEnum::kAudioJack));
}

TEST_F(TelemetryExtensionEventManagerTest,
       RegisterEventSuccessMultipleTabsOpen) {
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
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://googlechromelabs.github.io/"),
      /*cert_status=*/net::OK);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));

  // Close the first tab (index 1). The observer shouldn't be cut.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));

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

  // Close the secure one will cause the EventManager stop observing events.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest, RegisterEventNavigateOut) {
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

  // Navigation in the same domain shouldn't affect the observation.
  NavigateAndCommitActiveTab(
      GURL("https://googlechromelabs.github.io/example/path"));
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));

  // Navigation to other URL should cut the observation.
  NavigateAndCommitActiveTab(GURL("https://example.com/"));
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}

TEST_F(TelemetryExtensionEventManagerTest, RegisterEventTwoExtension) {
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
  EXPECT_FALSE(app_ui_observers().contains(extension_id_2));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id_2));

  // Open app UI for extension 2.
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("https://hpcs-appschr.hpcloud.hp.com"),
      /*cert_status=*/net::OK);
  EXPECT_EQ(
      EventManager::kSuccess,
      event_manager()->RegisterExtensionForEvent(
          extension_id_2, crosapi::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(app_ui_observers().contains(extension_id_1));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id_1));
  EXPECT_TRUE(app_ui_observers().contains(extension_id_2));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id_2));

  // Close the app UI of extension 1.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id_1));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id_1));
  EXPECT_TRUE(app_ui_observers().contains(extension_id_2));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id_2));

  // Close the app UI of extension 2.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id_1));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id_1));
  EXPECT_FALSE(app_ui_observers().contains(extension_id_2));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id_2));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(TelemetryExtensionEventManagerTest, RegisterEventIWASuccess) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ::chromeos::features::kIWAForTelemetryExtensionAPI);

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

  // Open IWA.
  OpenAppUiUrlAndSetCertificateWithStatus(
      GURL("isolated-app://"
           "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic"),
      /*cert_status=*/net::OK);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));

  // Close the PWA. This shouldn't affect the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(app_ui_observers().contains(extension_id));
  EXPECT_TRUE(event_router().IsExtensionObserving(extension_id));

  // Close the IWA (last tab) should cut the observation.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(app_ui_observers().contains(extension_id));
  EXPECT_FALSE(event_router().IsExtensionObserving(extension_id));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace chromeos

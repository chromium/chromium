// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_waiter.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/dependency_graph.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/features.h"

namespace {

// Creates a Profile and its underlying OTR Profile for testing.
// Waits for all tasks to be done to get as many services created as possible.
// Returns the Original Profile.
Profile* CreateProfileAndWaitForAllTasks(const base::FilePath& profile_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileWaiter profile_waiter;
  profile_manager->CreateProfileAsync(profile_path, {});
  Profile* system_profile = profile_waiter.WaitForProfileAdded();
  // Wait for Profile creation, and potentially other services that will be
  // created after all tasks are done.
  content::RunAllTasksUntilIdle();
  return system_profile;
}

// Gets all the KeyedServices from the DependencyGraph.
std::vector<KeyedServiceBaseFactory*> GetKeyedServiceBaseFactories() {
  BrowserContextDependencyManager* dependency_manager =
      BrowserContextDependencyManager::GetInstance();
  DependencyGraph& dependency_graph =
      dependency_manager->GetDependencyGraphForTesting();
  std::vector<DependencyNode*> nodes;
  bool success = dependency_graph.GetConstructionOrder(&nodes);
  DCHECK(success);

  std::vector<KeyedServiceBaseFactory*> keyedServiceFactories;
  keyedServiceFactories.reserve(nodes.size());
  base::ranges::transform(nodes, std::back_inserter(keyedServiceFactories),
                          [](DependencyNode* node) {
                            return static_cast<KeyedServiceBaseFactory*>(node);
                          });
  return keyedServiceFactories;
}

// Returns a string representation of the elements of `set1` which are absent
// from `set2`.
std::string GetDifferenceString(const std::set<std::string>& set1,
                                const std::set<std::string>& set2) {
  std::vector<std::string> differences;
  base::ranges::set_difference(set1, set2, std::back_inserter(differences));

  return differences.empty() ? "None" : base::JoinString(differences, ", ");
}

// Helper function to properly display differences between expected and reached
// service names.
std::string DisplaySetDifference(
    const std::set<std::string>& expected_active_services_names,
    const std::set<std::string>& active_services_names) {
  std::stringstream error;
  error << "Differences between expected and reached services:" << std::endl;

  error << "-- Missing Expected Services:" << std::endl;
  error << GetDifferenceString(expected_active_services_names,
                               active_services_names)
        << std::endl;

  error << "-- Added Extra Services:" << std::endl;
  error << GetDifferenceString(active_services_names,
                               expected_active_services_names)
        << std::endl;

  return error.str();
}

// The test comparing expected vs reached keyed services for the given profile.
void TestKeyedProfileServicesActives(
    Profile* profile,
    const std::set<std::string>& expected_active_services_names) {
  const std::vector<KeyedServiceBaseFactory*> keyedServiceFactories =
      GetKeyedServiceBaseFactories();

  std::set<std::string> active_services_names;
  for (KeyedServiceBaseFactory* factory : keyedServiceFactories) {
    if (factory->IsServiceCreated(profile)) {
      active_services_names.emplace(factory->name());
    }
  }

  EXPECT_EQ(active_services_names, expected_active_services_names)
      << DisplaySetDifference(expected_active_services_names,
                              active_services_names);
}

}  // namespace

TEST(ProfileKeyedService_DisplaySetDifferenceTest, UnexpectedActiveService) {
  std::string message =
      DisplaySetDifference(/*expected_active_services_names=*/{},
                           /*active_services_names=*/{"unexpected"});
  EXPECT_THAT(message,
              testing::ContainsRegex("Missing Expected Services:\\s+None"));
  EXPECT_THAT(message,
              testing::ContainsRegex("Added Extra Services:\\s+unexpected"));
}

TEST(ProfileKeyedService_DisplaySetDifferenceTest, MissingExpectedService) {
  std::string message =
      DisplaySetDifference(/*expected_active_services_names=*/{"missing"},
                           /*active_services_names=*/{});
  EXPECT_THAT(message,
              testing::ContainsRegex("Missing Expected Services:\\s+missing"));
  EXPECT_THAT(message, testing::ContainsRegex("Added Extra Services:\\s+None"));
}

// If you are adding a new keyed service and this test fails:
// - determine if your service is intended to be created for the System profile
// - if yes, add it to the list of allowed services
// - if not, update your factory class so that the service is not created for
// the system profile.
//
// Note: if your service should not be used on the system profile, but still has
// to, because other services depend on it, add a comment explaining why.
// Example:
//   // FooService is required because BarService depends on it.
//   // TODO(crbug.com/12345): Stop creating BarService for the system profile.
class ProfileKeyedServiceBrowserTest : public InProcessBrowserTest {
 public:
  ProfileKeyedServiceBrowserTest() {
    // Force features activation to make sure the test is accurate as possible.
    // Also removes differences between official and non official run of the
    // tests. If a feature is integrated in the fieldtrial_testing_config.json,
    // it might not be considered under an official build. Adding it under a
    // InitWithFeatures to activate it would neglect that difference.
    //
    // Force init `kSystemProfileSelectionDefaultNone` to make sure that this
    // test catches all new creation of services for System Profile, even during
    // the transition period when the feature experiment is partially active.
    //
    // clang-format off
    feature_list_.InitWithFeatures(
        {
          kSystemProfileSelectionDefaultNone,
#if !BUILDFLAG(IS_ANDROID)
          features::kTrustSafetySentimentSurvey,
#endif  // !BUILDFLAG(IS_ANDROID)
          blink::features::kBrowsingTopics
        },
        {});
    // clang-format on
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProfileKeyedServiceBrowserTest,
                       SystemProfileOTR_NeededServices) {
  // clang-format off
  // List of services expected to be created for the System OTR Profile.
  std::set<std::string> system_otr_active_services {
    "SiteDataCacheFacadeFactory"
  };
  // clang-format on

  Profile* system_profile =
      CreateProfileAndWaitForAllTasks(ProfileManager::GetSystemProfilePath());
  ASSERT_TRUE(system_profile->HasAnyOffTheRecordProfile());
  Profile* system_profile_otr = system_profile->GetPrimaryOTRProfile(false);
  ASSERT_TRUE(system_profile_otr->IsOffTheRecord());
  ASSERT_TRUE(system_profile_otr->IsSystemProfile());
  TestKeyedProfileServicesActives(system_profile_otr,
                                  system_otr_active_services);
}

IN_PROC_BROWSER_TEST_F(ProfileKeyedServiceBrowserTest,
                       SystemProfileParent_NeededServices) {
  // clang-format off
  // List of services expected to be created for the Parent System Profile.
  std::set<std::string> system_active_services {
    "SiteDataCacheFacadeFactory"
  };
  // clang-format on

  Profile* system_profile =
      CreateProfileAndWaitForAllTasks(ProfileManager::GetSystemProfilePath());
  ASSERT_FALSE(system_profile->IsOffTheRecord());
  ASSERT_TRUE(system_profile->IsSystemProfile());
  TestKeyedProfileServicesActives(system_profile, system_active_services);
}

IN_PROC_BROWSER_TEST_F(ProfileKeyedServiceBrowserTest,
                       GuestProfileOTR_NeededServices) {
  // clang-format off
  std::set<std::string> guest_otr_active_services {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    "CleanupManagerLacros",
    "DownloadBubbleUpdateService",
    "DownloadCoreService",
#else
    "LiveCaptionController",
#endif // BUILDFLAG(IS_CHROMEOS_LACROS)
    "AlarmManager",
    "BackgroundContentsService",
    "BackgroundSyncService",
    "BluetoothApiAdvertisementManager",
    "BluetoothApiSocketManager",
    "BluetoothLowEnergyConnectionManager",
    "BluetoothLowEnergyNotifySessionManager",
    "BluetoothSocketEventDispatcher",
    "BrowsingDataLifetimeManager",
    "CookieSettings",
    "ExtensionSystem",
    "ExtensionURLLoaderFactory::BrowserContextShutdownNotifierFactory",
    "FeedbackPrivateAPI",
    "FileSystemAccessPermissionContext",
    "GeneratedPrefs",
    "HeavyAdService",
#if BUILDFLAG(ENABLE_EXTENSIONS)
    "HidConnectionResourceManager",
#endif
    "HidDeviceManager",
    "HostContentSettingsMap",
    "MediaRouterUIService",
    "NotificationDisplayService",
    "OneTimePermissionsTrackerKeyedService",
    "OptimizationGuideKeyedService",
#if BUILDFLAG(ENABLE_PDF)
    "PdfViewerPrivateEventRouter",
#endif  // BUILDFLAG(ENABLE_PDF)
    "PlatformNotificationService",
    "PrefWatcher",
    "PrivacySandboxSettings",
    "ProcessManager",
    "ProfileNetworkContextService",
    "RealtimeReportingClient",
    "RendererUpdater",
    "ResumableTCPServerSocketManager",
    "ResumableTCPSocketManager",
    "ResumableUDPSocketManager",
    "RulesRegistryService",
    "SafeBrowsingPrivateEventRouter",
    "SerialConnectionManager",
    "SerialPortManager",
    "SettingsPrivateEventRouter",
    "SiteDataCacheFacadeFactory",
    "SiteEngagementService",
    "SocketManager",
    "StorageNotificationService",
    "TCPServerSocketEventDispatcher",
    "TCPSocketEventDispatcher",
    "TabGroupsEventRouter",
    "ToolbarActionsModel",
    "UDPSocketEventDispatcher",
    "UkmBackgroundRecorderService",
    "UsbDeviceManager",
    "UsbDeviceResourceManager",
    "sct_reporting::Factory"
  };
  // clang-format on

  Profile* guest_profile =
      CreateProfileAndWaitForAllTasks(ProfileManager::GetGuestProfilePath());
  ASSERT_TRUE(guest_profile->HasAnyOffTheRecordProfile());
  Profile* guest_profile_otr = guest_profile->GetPrimaryOTRProfile(false);
  ASSERT_TRUE(guest_profile_otr->IsOffTheRecord());
  ASSERT_TRUE(guest_profile_otr->IsGuestSession());
  TestKeyedProfileServicesActives(guest_profile_otr, guest_otr_active_services);
}

IN_PROC_BROWSER_TEST_F(ProfileKeyedServiceBrowserTest,
                       GuestProfileParent_NeededServices) {
  // clang-format off
  std::set<std::string> guest_active_services {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    "CleanupManagerLacros",
    "ClipboardAPI",
    "ExternalLogoutRequestEventHandler",
    "ManualTestHeartbeatEvent",
    "SessionStateChangedEventDispatcher",
#else // !BUILDFLAG(IS_CHROMEOS_LACROS)
    "SystemIndicatorManager",
    "WebAppAdjustments",
    "WebAppProvider",
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    "SpellcheckService",
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    "AboutSigninInternals",
    "AboutThisSiteServiceFactory",
    "AccountInvestigator",
    "AccountReconcilor",
    "ActivityLog",
    "ActivityLogPrivateAPI",
    "AdaptiveQuietNotificationPermissionUiEnabler",
    "AdvancedProtectionStatusManager",
    "AlarmManager",
    "AnnouncementNotificationService",
    "AppLifetimeMonitor",
    "AppLoadService",
    "AppRestoreService",
    "AppServiceProxy",
    "AppSessionService",
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    "AppShortcutManager",
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    "AppTerminationObserver",
    "AppWindowRegistry",
    "AudioAPI",
    "AutocompleteScoringModelService",
    "AutofillImageFetcher",
    "AutofillPrivateEventRouter",
    "AutofillStrikeDatabase",
    "BackgroundContentsService",
    "BackgroundFetchService",
    "BackgroundSyncService",
    "Blocklist",
    "BluetoothAPI",
    "BluetoothApiSocketManager",
    "BluetoothApiAdvertisementManager",
    "BluetoothLowEnergyAPI",
    "BluetoothLowEnergyConnectionManager",
    "BluetoothLowEnergyNotifySessionManager",
    "BluetoothPrivateAPI",
    "BluetoothSocketEventDispatcher",
    "BookmarkManagerPrivateAPI",
#if defined(TOOLKIT_VIEWS)
    "BookmarkExpandedStateTracker",
#endif
    "BookmarkModel",
    "BookmarkSyncServiceFactory",
    "BookmarkUndoService",
    "BookmarksAPI",
    "BookmarksApiWatcher",
    "BrailleDisplayPrivateAPI",
    "BrowsingTopicsService",
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    "ChildAccountService",
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
    "ChromeSigninClient",
    "ClosedTabCacheService",
    "CommandService",
    "ConsentAuditor",
    "ContentIndexProvider",
    "ContentSettingsService",
    "CookieSettings",
    "CookiesAPI",
    "CWSInfoService",
    "DeveloperPrivateAPI",
    "DeviceInfoSyncService",
    "DownloadCoreService",
    "EventRouter",
    "ExtensionActionAPI",
    "ExtensionActionManager",
    "ExtensionCommandsGlobalRegistry",
    "ExtensionGCMAppHandler",
    "ExtensionGarbageCollector",
    "ExtensionHostRegistry",
    "ExtensionManagement",
    "ExtensionPrefValueMap",
    "ExtensionPrefs",
    "ExtensionRegistry",
    "ExtensionSyncService",
    "ExtensionSystem",
    "ExtensionSystemShared",
    "ExtensionURLLoaderFactory::BrowserContextShutdownNotifierFactory",
    "ExtensionWebUIOverrideRegistrar",
    "FaviconService",
    "FeedbackPrivateAPI",
    "FileSystemAccessPermissionContext",
    "FirstPartySetsPolicyService",
    "FontPrefChangeNotifier",
    "FontSettingsAPI",
    "GAIAInfoUpdateService",
    "GCMProfileService",
    "GeneratedPrefs",
    "HeavyAdService",
#if BUILDFLAG(ENABLE_EXTENSIONS)
    "HidConnectionResourceManager",
#endif
    "HidDeviceManager",
    "HistoryAPI",
    "HistoryService",
    "HostContentSettingsMap",
    "HttpEngagementKeyService",
    "IdentityAPI",
    "IdentityManager",
    "IdleManager",
    "InstallStageTracker",
    "InstallTracker",
    "InstallVerifier",
    "InstanceIDProfileService",
    "InvalidationService",
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    "KidsChromeManagementClient",
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
    "LanguageSettingsPrivateDelegate",
    "LazyBackgroundTaskQueue",
    "LoginUIServiceFactory",
    "MDnsAPI",
    "ManagedBookmarkService",
    "ManagedConfigurationAPI",
    "ManagementAPI",
    "MediaGalleriesAPI",
    "MediaRouter",
    "MediaRouterUIService",
    "MenuManager",
    "ModelTypeStoreService",
    "NavigationPredictorKeyedService",
    "NetworkingPrivateEventRouter",
    "NotificationDisplayService",
    "OmniboxAPI",
    "OneTimePermissionsTrackerKeyedService",
    "OperationManager",
    "OptimizationGuideKeyedService",
    "PageContentAnnotationsService",
    "PasswordsPrivateEventRouter",
#if BUILDFLAG(ENABLE_PDF)
    "PdfViewerPrivateEventRouter",
#endif  // BUILDFLAG(ENABLE_PDF)
    "PermissionHelper",
    "PermissionsManager",
    "PermissionsUpdaterShutdownFactory",
    "PersonalDataManager",
    "PinnedTabService",
    "PlatformNotificationService",
    "PluginManager",
    "PluginPrefs",
    "PowerBookmarkService",
    "PrefWatcher",
    "PreferenceAPI",
    "PrimaryAccountPolicyManager",
  #if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
    "PrintingMetricsService",
  #endif // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
    "PrinterProviderInternal",
    "PrivacySandboxService",
    "PrivacySandboxSettings",
    "ProcessManager",
    "ProcessMap",
    "ProcessesAPI",
    "ProfileNetworkContextService",
    "ProfileThemeUpdateServiceFactory",
    "ProtocolHandlerRegistry",
    "ReadingListModel",
    "RealtimeReportingClient",
    "RendererStartupHelper",
    "RendererUpdater",
    "ResumableTCPServerSocketManager",
    "ResumableTCPSocketManager",
    "ResumableUDPSocketManager",
    "RulesMonitorService",
    "RulesRegistryService",
    "RuntimeAPI",
    "SafeBrowsingMetricsCollector",
    "SafeBrowsingPrivateEventRouter",
    "SafeBrowsingTailoredSecurityService",
    "SecurityEventRecorder",
    "SendTabToSelfClientService",
    "SendTabToSelfSyncService",
    "SerialConnectionManager",
    "SerialPortManager",
    "SessionDataService",
    "SessionProtoDBFactory",
    "SessionSyncService",
    "SessionsAPI",
    "SettingsOverridesAPI",
    "SettingsPrivateEventRouter",
    "SharingMessageBridge",
    "SharingService",
    "ShoppingService",
    "SidePanelService",
    "SigninErrorController",
    "SigninManager",
    "SigninProfileAttributesUpdater",
    "SiteDataCacheFacadeFactory",
    "SiteEngagementService",
    "SocketManager",
    "StorageFrontend",
    "StorageNotificationService",
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    "SupervisedUserService",
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
    "SyncInvalidationsService",
    "SyncService",
    "SyncSessionsWebContentsRouter",
    "SystemInfoAPI",
    "TCPServerSocketEventDispatcher",
    "TCPSocketEventDispatcher",
    "TabGroupsEventRouter",
    "TabsWindowsAPI",
    "TemplateURLServiceFactory",
    "ThemeService",
    "ToolbarActionsModel",
    "TranslateRanker",
    "TriggeredProfileResetter",
    "TtsAPI",
    "UDPSocketEventDispatcher",
    "UkmBackgroundRecorderService",
    "UsbDeviceManager",
    "UsbDeviceResourceManager",
    "UserCloudPolicyInvalidator",
    "UserEventService",
    "UserPolicySigninService",
    "WarningBadgeService",
    "WarningService",
    "WebAuthenticationProxyAPI",
    "WebDataService",
    "WebNavigationAPI",
    "WebRequestAPI",
    "WebRtcEventLogManagerKeyedService",
    "WebrtcAudioPrivateEventService",
    "feedback::FeedbackUploaderChrome",
    "sct_reporting::Factory",
    "ZeroSuggestCacheServiceFactory",
  };
  // clang-format on

  Profile* guest_profile =
      CreateProfileAndWaitForAllTasks(ProfileManager::GetGuestProfilePath());
  ASSERT_FALSE(guest_profile->IsOffTheRecord());
  ASSERT_TRUE(guest_profile->IsGuestSession());
  TestKeyedProfileServicesActives(guest_profile, guest_active_services);
}

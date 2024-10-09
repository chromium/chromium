// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/containers/to_vector.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_waiter.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/dependency_graph.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/features.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ui_base_features.h"

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
  std::vector<raw_ptr<DependencyNode, VectorExperimental>> nodes;
  bool success = dependency_graph.GetConstructionOrder(&nodes);
  DCHECK(success);

  return base::ToVector(nodes, [](DependencyNode* node) {
    return static_cast<KeyedServiceBaseFactory*>(node);
  });
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
    const std::set<std::string>& expected_active_services_names,
    bool force_create_services = false,
    const base::Location& location = FROM_HERE) {
  const std::vector<KeyedServiceBaseFactory*> keyedServiceFactories =
      GetKeyedServiceBaseFactories();

  if (force_create_services) {
    for (KeyedServiceBaseFactory* factory : keyedServiceFactories) {
      factory->CreateServiceNowForTesting(profile);
    }
  }

  std::set<std::string> active_services_names;
  for (KeyedServiceBaseFactory* factory : keyedServiceFactories) {
    if (factory->IsServiceCreated(profile)) {
      active_services_names.emplace(factory->name());
    }
  }

  EXPECT_EQ(active_services_names, expected_active_services_names)
      << DisplaySetDifference(expected_active_services_names,
                              active_services_names)
      << ", expected at " << location.ToString();
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
//   // TODO(crbug.com/40781525): Stop creating BarService for the system
//   profile.
class ProfileKeyedServiceBrowserTest : public InProcessBrowserTest {
 public:
  ProfileKeyedServiceBrowserTest() {
    // Force features activation to make sure the test is accurate as possible.
    // Also removes differences between official and non official run of the
    // tests.
    //
    // If a feature is integrated in the fieldtrial_testing_config.json,
    // it might not be considered under an official build. Adding it under the
    // InitWithFeatures below, to activate it, will solve that difference.

    // clang-format off
    feature_list_.InitWithFeatures(
        {
          features::kTrustSafetySentimentSurvey,
#if BUILDFLAG(IS_WIN)
          switches::kEnableBoundSessionCredentials,
#endif  // BUILDFLAG(IS_WIN)
          blink::features::kBrowsingTopics,
          blink::features::kEnableBuiltInAIAPI,
          net::features::kTopLevelTpcdOriginTrial,
          net::features::kTpcdTrialSettings,
          net::features::kTopLevelTpcdTrialSettings,
          features::kPdfOcr,
          features::kPersistentOriginTrials,
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
          omnibox::kOnDeviceTailModel,
          omnibox::kOnDeviceHeadProviderNonIncognito,
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
        },
        {});
    // clang-format on
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProfileKeyedServiceBrowserTest,
                       SystemProfileOTR_NeededServices) {
  Profile* system_profile =
      CreateProfileAndWaitForAllTasks(ProfileManager::GetSystemProfilePath());
  ASSERT_TRUE(system_profile->HasAnyOffTheRecordProfile());
  Profile* system_profile_otr = system_profile->GetPrimaryOTRProfile(false);
  ASSERT_TRUE(system_profile_otr->IsOffTheRecord());
  ASSERT_TRUE(system_profile_otr->IsSystemProfile());
  TestKeyedProfileServicesActives(system_profile_otr,
                                  /*expected_active_services_names=*/{});
}

IN_PROC_BROWSER_TEST_F(ProfileKeyedServiceBrowserTest,
                       SystemProfileParent_NeededServices) {
  Profile* system_profile =
      CreateProfileAndWaitForAllTasks(ProfileManager::GetSystemProfilePath());
  ASSERT_FALSE(system_profile->IsOffTheRecord());
  ASSERT_TRUE(system_profile->IsSystemProfile());
  TestKeyedProfileServicesActives(system_profile,
                                  /*expected_active_services_names=*/{});
}

IN_PROC_BROWSER_TEST_F(ProfileKeyedServiceBrowserTest,
                       GuestProfileOTR_NeededServices) {
  // clang-format off
  std::set<std::string> guest_otr_active_services {
    "LiveCaptionController",
    "LiveTranslateController",
    "AIManagerKeyedService",
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
#if BUILDFLAG(IS_WIN)
    "BoundSessionCookieRefreshService",
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
    "ExtensionInstallEventRouter",
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
    "ExtensionSystem",
    "ExtensionURLLoaderFactory::BrowserContextShutdownNotifierFactory",
    "FederatedIdentityPermissionContext",
    "FederatedIdentityAutoReauthnPermissionContext",
    "FeedbackPrivateAPI",
    "FileSystemAccessPermissionContext",
    "GeneratedPrefs",
    "HeavyAdService",
#if BUILDFLAG(ENABLE_EXTENSIONS)
    "HidConnectionResourceManager",
#endif
    "HidDeviceManager",
    "HostContentSettingsMap",
    "MediaRouter",
    "MediaRouterUIService",
    "NotificationDisplayService",
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    "OnDeviceTailModelService",
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    "OneTimePermissionsTrackerKeyedService",
    "OptimizationGuideKeyedService",
    "PermissionDecisionAutoBlocker",
    "PinnedToolbarActionsModel",
    "PlatformNotificationService",
    "PredictionModelHandlerProvider",
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
    "TrackingProtectionSettings",
    "UDPSocketEventDispatcher",
    "UkmBackgroundRecorderService",
#if BUILDFLAG(IS_WIN)
    "UnexportableKeyService",
#endif  // BUILDFLAG(IS_WIN)
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
    "SystemIndicatorManager",
    "WebAppProvider",
    "AccountExtensionTracker",
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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)|| BUILDFLAG(IS_WIN)
    "ManualTestHeartbeatEvent",
#endif // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)|| BUILDFLAG(IS_WIN)
    "AppTerminationObserver",
    "AppWindowRegistry",
    "AudioAPI",
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    "AutocompleteScoringModelService",
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
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
    "BookmarkUndoService",
    "BookmarksAPI",
    "BrailleDisplayPrivateAPI",
    "BrowsingTopicsService",
    "ChildAccountService",
    "ChromeSigninClient",
    "CommandService",
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
    "ConnectorsService",
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
    "ContentIndexProvider",
    "ContentSettingsService",
    "CookieSettings",
    "CookiesAPI",
    "CWSInfoService",
    "DataTypeStoreService",
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
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
    "ExtensionInstallEventRouter",
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
    "ExtensionManagement",
    "ExtensionPrefValueMap",
    "ExtensionPrefs",
    "ExtensionRegistry",
    "ExtensionSyncService",
    "ExtensionSystem",
    "ExtensionSystemShared",
    "ExtensionURLLoaderFactory::BrowserContextShutdownNotifierFactory",
    "ExtensionWebUIOverrideRegistrar",
    "FederatedIdentityPermissionContext",
    "FederatedIdentityAutoReauthnPermissionContext",
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
#if BUILDFLAG(IS_CHROMEOS)
    "KcerFactory",
#endif // BUILDFLAG(IS_CHROMEOS)
    "LanguageSettingsPrivateDelegate",
    "LazyBackgroundTaskQueue",
    "ListFamilyMembersService",
    "LocalOrSyncableBookmarkSyncServiceFactory",
    "LoginUIServiceFactory",
    "MDnsAPI",
    "ManagedBookmarkService",
    "ManagedConfigurationAPI",
    "ManagementAPI",
#if BUILDFLAG(ENABLE_EXTENSIONS)
    "ManifestV2ExperimentManager",
#endif
    "MediaGalleriesAPI",
    "MediaRouter",
    "MediaRouterUIService",
    "MenuManager",
    "NavigationPredictorKeyedService",
    "NetworkingPrivateEventRouter",
    "NotificationDisplayService",
    "NtpBackgroundService",
    "NtpCustomBackgroundService",
#if BUILDFLAG(IS_CHROMEOS)
    "NssServiceFactory",
#endif // BUILDFLAG(IS_CHROMEOS)
    "OmniboxAPI",
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    "OnDeviceTailModelService",
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    "OneTimePermissionsTrackerKeyedService",
    "OperationManager",
    "OptimizationGuideKeyedService",
    "OriginTrialService",
    "PageContentAnnotationsService",
    "PasswordsPrivateEventRouter",
    "PermissionDecisionAutoBlocker",
    "PermissionHelper",
    "PermissionsManager",
    "PermissionsUpdaterShutdownFactory",
    "PersonalDataManager",
    "PinnedTabService",
    "PinnedToolbarActionsModel",
    "PlatformNotificationService",
    "PluginManager",
    "PluginPrefs",
    "PowerBookmarkService",
    "PredictionModelHandlerProvider",
    "PrefWatcher",
    "PreferenceAPI",
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
    "ProtocolHandlerRegistry",
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
    "SearchEngineChoiceServiceFactory",
    "SendTabToSelfClientService",
    "SendTabToSelfSyncService",
    "SerialConnectionManager",
    "SerialPortManager",
    "SessionDataService",
    "SessionProtoDBFactory",
    "SessionsAPI",
    "sessions::TabRestoreService",
    "SettingsOverridesAPI",
    "SettingsPrivateEventRouter",
    "ShoppingService",
    "SidePanelService",
    "SiteDataCacheFacadeFactory",
    "SiteEngagementService",
    "SocketManager",
    "StorageAccessHeaderService",
    "StorageFrontend",
    "StorageNotificationService",
    "SupervisedUserService",
    "SyncInvalidationsService",
    "SystemInfoAPI",
    "TCPServerSocketEventDispatcher",
    "TCPSocketEventDispatcher",
    "TabGroupsEventRouter",
    "TabsWindowsAPI",
    "TemplateURLServiceFactory",
    "ThemeService",
    "ToolbarActionsModel",
    "TopLevelTrialService",
    "TpcdTrialService",
    "TrackingProtectionSettings",
    "TranslateRanker",
    "TriggeredProfileResetter",
    "TtsAPI",
    "UDPSocketEventDispatcher",
    "UkmBackgroundRecorderService",
    "UsbDeviceManager",
    "UsbDeviceResourceManager",
    "UserCloudPolicyInvalidator",
    "UserFmRegistrationTokenUploader",
    "UserPolicySigninService",
    "WarningBadgeService",
    "WarningService",
    "WebAuthenticationProxyAPI",
#if BUILDFLAG(IS_CHROMEOS)
    "WebcamPrivateAPI",
#endif
    "WebDataService",
    "WebNavigationAPI",
    "WebRequestAPI",
    "WebRequestEventRouter",
    "WebRtcEventLogManagerKeyedService",
    "WebrtcAudioPrivateEventService",
    "WriteQuotaChecker",
    "feedback::FeedbackUploaderChrome",
    "sct_reporting::Factory",
    "ZeroSuggestCacheServiceFactory",
  };
  // clang-format on

  if (base::FeatureList::IsEnabled(commerce::kProductSpecifications)) {
    guest_active_services.insert("ProductSpecificationsService");
  }

  Profile* guest_profile =
      CreateProfileAndWaitForAllTasks(ProfileManager::GetGuestProfilePath());
  ASSERT_FALSE(guest_profile->IsOffTheRecord());
  ASSERT_TRUE(guest_profile->IsGuestSession());
  TestKeyedProfileServicesActives(guest_profile, guest_active_services);
}

IN_PROC_BROWSER_TEST_F(ProfileKeyedServiceBrowserTest,
                       SystemProfileParent_ServicesThatCanBeCreated) {
  Profile* system_profile =
      CreateProfileAndWaitForAllTasks(ProfileManager::GetSystemProfilePath());
  ASSERT_FALSE(system_profile->IsOffTheRecord());
  ASSERT_TRUE(system_profile->IsSystemProfile());

  // clang-format off
  std::set<std::string> exepcted_created_services_names = {
    // in components:
    // There is no control over the creation based on the Profile types in
    // components/. These services are not created for the System Profile by
    // default, however their creation is still possible.
    "AutocompleteControllerEmitter",
    "AutofillInternalsService",
    "DataControlsRulesService",
    "HasEnrolledInstrumentQuery",
    "LocalPresentationManager",
    "OmniboxInputWatcher",
    "OmniboxSuggestionsWatcher",
    "PasswordManagerInternalsService",
    "PasswordRequirementsServiceFactory",
    "PolicyBlocklist",
    "PolicyClipboardRestriction",
    "SafeSearch",
    "WebDataService",

    // in chrome: using `BrowserContextKeyedServiceShutdownNotifierFactory`:
    // which does not yet have an implementation using `ProfileSelections`.
    "GalleryWatchManager",
    "MediaFileSystemRegistry",
    "NotificationDisplayService",
    "PermissionsUpdaterShutdownFactory",
    "PluginInfoHostImpl",
    "TurnSyncOnHelperShutdownNotifier",
  };
  // clang-format on

  TestKeyedProfileServicesActives(system_profile,
                                  exepcted_created_services_names,
                                  /*force_create_services=*/true);
}

IN_PROC_BROWSER_TEST_F(ProfileKeyedServiceBrowserTest,
                       SystemProfileOTR_ServicesThatCanBeCreated) {
  Profile* system_profile =
      CreateProfileAndWaitForAllTasks(ProfileManager::GetSystemProfilePath());
  ASSERT_TRUE(system_profile->HasAnyOffTheRecordProfile());
  Profile* system_profile_otr = system_profile->GetPrimaryOTRProfile(false);
  ASSERT_TRUE(system_profile_otr->IsOffTheRecord());
  ASSERT_TRUE(system_profile_otr->IsSystemProfile());

  // clang-format off
  std::set<std::string> exepcted_created_services_names = {
    // in components:
    // There is no control over the creation based on the Profile types in
    // components/. These services are not created for the System Profile by
    // default, however their creation is still possible.
    "AutocompleteControllerEmitter",
    "DataControlsRulesService",
    "HasEnrolledInstrumentQuery",
    "OmniboxInputWatcher",
    "OmniboxSuggestionsWatcher",
    "PolicyBlocklist",
    "PolicyClipboardRestriction",
    "SafeSearch",

    // in chrome: using `BrowserContextKeyedServiceShutdownNotifierFactory`:
    // which does not yet have an implementation using `ProfileSelections`.
    "GalleryWatchManager",
    "MediaFileSystemRegistry",
    "NotificationDisplayService",
    "PermissionsUpdaterShutdownFactory",
    "PluginInfoHostImpl",
    "TurnSyncOnHelperShutdownNotifier",

    // Those services are needed to be able to display IPHs in the Profile
    // Picker.
    "feature_engagement::Tracker",
    "UserEducationService",
  };
  // clang-format on

  TestKeyedProfileServicesActives(system_profile_otr,
                                  exepcted_created_services_names,
                                  /*force_create_services=*/true);
}

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"

#include <utility>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/content_settings/one_time_permission_provider.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "chrome/browser/profiles/off_the_record_profile_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/buildflags.h"
#include "components/content_settings/core/browser/content_settings_pref_provider.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/features.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/webui/webui_allowlist_provider.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/trace_event/trace_event.h"
#include "extensions/browser/api/content_settings/content_settings_custom_extension_provider.h"
#include "extensions/browser/api/content_settings/content_settings_service.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/content_settings/content_settings_supervised_provider.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/installable/installed_webapp_provider.h"
#include "chrome/browser/notifications/notification_channels_provider_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/exit_type_service_factory.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

HostContentSettingsMapFactory::HostContentSettingsMapFactory()
    : RefcountedProfileKeyedServiceFactory(
          "HostContentSettingsMap",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  DependsOn(SupervisedUserSettingsServiceFactory::GetInstance());
#endif
#if BUILDFLAG(IS_ANDROID)
  DependsOn(TemplateURLServiceFactory::GetInstance());
#else
  DependsOn(OneTimePermissionsTrackerFactory::GetInstance());
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(extensions::ContentSettingsService::GetFactoryInstance());
#endif
  // Used by way of ShouldRestoreOldSessionCookies().
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  DependsOn(ExitTypeServiceFactory::GetInstance());
#endif
}

HostContentSettingsMapFactory::~HostContentSettingsMapFactory() {
}

// static
HostContentSettingsMap* HostContentSettingsMapFactory::GetForProfile(
    content::BrowserContext* browser_context) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  return static_cast<HostContentSettingsMap*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true).get());
}

// static
HostContentSettingsMapFactory* HostContentSettingsMapFactory::GetInstance() {
  return base::Singleton<HostContentSettingsMapFactory>::get();
}

scoped_refptr<RefcountedKeyedService>
    HostContentSettingsMapFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = static_cast<Profile*>(context);
  // extensions::ContentSettingsService::Get() needs the original profile.
  Profile* original_profile = profile->GetOriginalProfile();

  // In OffTheRecord mode, retrieve the host content settings map of the parent
  // profile in order to ensure the preferences have been migrated.
  // This is not required for guest sessions, since the parent profile of a
  // guest OTR profile is empty.
  if (profile->IsOffTheRecord() && !profile->IsGuestSession())
    GetForProfile(original_profile);

  bool should_record_metrics = profile->IsRegularProfile();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS creates various irregular profiles (login, lock screen...); they
  // are of type kRegular (returns true for `Profile::IsRegular()`), that aren't
  // used to browse the web and users can't configure. Don't collect metrics
  // about them.
  should_record_metrics =
      should_record_metrics && ash::ProfileHelper::IsUserProfile(profile);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  scoped_refptr<HostContentSettingsMap> settings_map(new HostContentSettingsMap(
      profile->GetPrefs(),
      profile->IsOffTheRecord() || profile->IsGuestSession(),
      /*store_last_modified=*/true, profile->ShouldRestoreOldSessionCookies(),
      should_record_metrics));

  auto allowlist_provider = std::make_unique<WebUIAllowlistProvider>(
      WebUIAllowlist::GetOrCreate(profile));
  settings_map->RegisterProvider(
      HostContentSettingsMap::WEBUI_ALLOWLIST_PROVIDER,
      std::move(allowlist_provider));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // These must be registered before before the HostSettings are passed over to
  // the IOThread.  Simplest to do this on construction.
  settings_map->RegisterProvider(
      HostContentSettingsMap::CUSTOM_EXTENSION_PROVIDER,
      std::make_unique<content_settings::CustomExtensionProvider>(
          extensions::ContentSettingsService::Get(original_profile)
              ->content_settings_store(),
          // TODO(crbug.com/1254409): This is the only call site, so can we
          // remove this constructor parameter, or should this actually reflect
          // the case where profile->IsOffTheRecord() is true? And what is the
          // interaction with profile->IsGuestSession()?
          false));
#endif // BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  supervised_user::SupervisedUserSettingsService* supervised_service =
      SupervisedUserSettingsServiceFactory::GetForKey(profile->GetProfileKey());
  // This may be null in testing.
  if (supervised_service) {
    std::unique_ptr<content_settings::SupervisedProvider> supervised_provider(
        new content_settings::SupervisedProvider(supervised_service));
    settings_map->RegisterProvider(HostContentSettingsMap::SUPERVISED_PROVIDER,
                                   std::move(supervised_provider));
  }
#endif // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if BUILDFLAG(IS_ANDROID)
  if (!profile->IsOffTheRecord()) {
    auto channels_provider =
        std::make_unique<NotificationChannelsProviderAndroid>();

    channels_provider->MigrateToChannelsIfNecessary(
        profile->GetPrefs(), settings_map->GetPrefProvider());

    // Clear blocked channels *after* migrating in case the pref provider
    // contained any erroneously-created channels that need deleting.
    channels_provider->ClearBlockedChannelsIfNecessary(
        profile->GetPrefs(), TemplateURLServiceFactory::GetForProfile(profile));

    settings_map->RegisterUserModifiableProvider(
        HostContentSettingsMap::NOTIFICATION_ANDROID_PROVIDER,
        std::move(channels_provider));

    auto webapp_provider = std::make_unique<InstalledWebappProvider>();
    settings_map->RegisterProvider(
        HostContentSettingsMap::INSTALLED_WEBAPP_PROVIDER,
        std::move(webapp_provider));
  }
#else
  if (base::FeatureList::IsEnabled(permissions::features::kOneTimePermission)) {
    auto one_time_permission_provider =
        std::make_unique<OneTimePermissionProvider>(
            OneTimePermissionsTrackerFactory::GetForBrowserContext(context));

    settings_map->RegisterUserModifiableProvider(
        HostContentSettingsMap::ONE_TIME_PERMISSION_PROVIDER,
        std::move(one_time_permission_provider));
  }
#endif  // defined (OS_ANDROID)
  return settings_map;
}

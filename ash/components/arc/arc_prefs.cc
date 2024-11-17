// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/arc_prefs.h"

#include <string>

#include "ash/components/arc/session/arc_management_transition.h"
#include "ash/components/arc/session/arc_vm_data_migration_status.h"
#include "components/guest_os/guest_os_prefs.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/pref_registry_simple.h"

namespace arc {
namespace prefs {

namespace {

void RegisterDailyMetricsPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kArcDailyMetricsKills);
  metrics::DailyEvent::RegisterPref(registry, prefs::kArcDailyMetricsSample);
}

}  // anonymous namespace

// ======== PROFILE PREFS ========
// See below for local state prefs.

// A bool preference indicating whether traffic other than the VPN connection
// set via kAlwaysOnVpnPackage should be blackholed.
const char kAlwaysOnVpnLockdown[] = "arc.vpn.always_on.lockdown";
// A string preference indicating the Android app that will be used for
// "Always On VPN". Should be empty if "Always On VPN" is not enabled.
const char kAlwaysOnVpnPackage[] = "arc.vpn.always_on.vpn_package";
// Stores the user id received from DM Server when enrolling a Play user on an
// Active Directory managed device. Used to report to DM Server that the account
// is still used.
const char kArcActiveDirectoryPlayUserId[] =
    "arc.active_directory_play_user_id";
// Stores whether ARC app is requested in the session. Used for UMA.
// -1 indicates no data. 0 or greaters are the number of app launch requests.
const char kArcAppRequestedInSession[] = "arc.app_requested_in_session";
// A preference to keep list of Android apps and their state.
const char kArcApps[] = "arc.apps";
// A preference to store backup and restore state for Android apps.
const char kArcBackupRestoreEnabled[] = "arc.backup_restore.enabled";
// Cumulative daily counts of app kills by priority and with other VM context.
const char kArcDailyMetricsKills[] = "arc.dialy_metrics_kills";
//  Timestamp of the last time daily metrics have been reported.
const char kArcDailyMetricsSample[] = "arc.daily_metrics_sample";
// A preference to indicate that Android's data directory should be removed.
const char kArcDataRemoveRequested[] = "arc.data.remove_requested";
// A preference representing whether a user has opted in to use Google Play
// Store on ARC.
// TODO(hidehiko): For historical reason, now the preference name does not
// directly reflect "Google Play Store". We should get and set the values via
// utility methods (IsArcPlayStoreEnabledForProfile() and
// SetArcPlayStoreEnabledForProfile()) in chrome/browser/ash/arc/arc_util.h.
const char kArcEnabled[] = "arc.enabled";
// A preference to control if ARC can access removable media on the host side.
// TODO(fukino): Remove this pref once "Play Store applications can't access
// this device" toast in Files app becomes aware of kArcVisibleExternalStorages.
// crbug.com/998512.
const char kArcHasAccessToRemovableMedia[] =
    "arc.has_access_to_removable_media";
// A preference to keep list of external storages which are visible to Android
// apps. (i.e. can be read/written by Android apps.)
const char kArcVisibleExternalStorages[] = "arc.visible_external_storages";
// A preference that indicates that initial settings need to be applied. Initial
// settings are applied only once per new OptIn once mojo settings instance is
// ready. Each OptOut resets this preference. Note, its sense is close to
// |kArcSignedIn|, however due the asynchronous nature of initializing mojo
// components, timing of triggering |kArcSignedIn| and
// |kArcInitialSettingsPending| can be different and
// |kArcInitialSettingsPending| may even be handled in the next user session.
const char kArcInitialSettingsPending[] = "arc.initial.settings.pending";
// A preference that indicates that a management transition is necessary, in
// response to account management state change.
const char kArcManagementTransition[] = "arc.management_transition";
// A preference that indicated whether Android reported it's compliance status
// with provided policies. This is used only as a signal to start Android kiosk.
const char kArcPolicyComplianceReported[] = "arc.policy_compliance_reported";
// A preference that indicates that user accepted PlayStore terms.
const char kArcTermsAccepted[] = "arc.terms.accepted";
// A preference to keep user's consent to use location service.
const char kArcLocationServiceEnabled[] = "arc.location_service.enabled";
// A preference to keep list of Android packages and their infomation.
const char kArcPackages[] = "arc.packages";
// A preference that indicates that arc.packages is up to date.
const char kArcPackagesIsUpToDate[] = "arc.packages_is_up_to_date";
// A preference that indicates that Play Auto Install flow was already started.
const char kArcPaiStarted[] = "arc.pai.started";
// A preference that indicates that provisioning was initiated from OOBE. This
// is preserved across Chrome restart.
const char kArcProvisioningInitiatedFromOobe[] =
    "arc.provisioning.initiated.from.oobe";
// A preference that indicates that Play Fast App Reinstall flow was already
// started.
const char kArcFastAppReinstallStarted[] = "arc.fast.app.reinstall.started";
// A preference to keep list of Play Fast App Reinstall packages.
const char kArcFastAppReinstallPackages[] = "arc.fast.app.reinstall.packages";
// Stores the history of whether the first ARC activation during user session
// start up. A list of booleans; true if the first activation is done during
// the user session start up.
const char kArcFirstActivationDuringUserSessionStartUpHistory[] =
    "arc.first_activation_during_user_session_start_up_history";
// A preference to keep the current Android framework version. Note, that value
// is only available after first packages update.
const char kArcFrameworkVersion[] = "arc.framework.version";
// A preference that holds the list of apps that the admin requested to be
// push-installed.
const char kArcPushInstallAppsRequested[] = "arc.push_install.requested";
// A preference that holds the list of apps that the admin requested to be
// push-installed, but which have not been successfully installed yet.
const char kArcPushInstallAppsPending[] = "arc.push_install.pending";
// A preference to keep deferred requests of setting notifications enabled flag.
const char kArcSetNotificationsEnabledDeferred[] =
    "arc.set_notifications_enabled_deferred";
// A preference that indicates status of Android sign-in.
const char kArcSignedIn[] = "arc.signedin";
// A preference that indicates that ARC skipped the setup UI flows that
// contain a notice related to reporting of diagnostic information.
const char kArcSkippedReportingNotice[] = "arc.skipped.reporting.notice";
// A preference that indicates an ARC comaptible filesystem was chosen for
// the user directory (i.e., the user finished required migration.)
const char kArcCompatibleFilesystemChosen[] =
    "arc.compatible_filesystem.chosen";
// Preferences for storing engagement time data, as per
// GuestOsEngagementMetrics.
const char kEngagementPrefsPrefix[] = "arc.metrics";

// A boolean preference that indicates ARC management state.
const char kArcIsManaged[] = "arc.is_managed";

// A counter preference that indicates number of ARC resize-lock splash screen.
const char kArcShowResizeLockSplashScreenLimits[] =
    "arc.show_resize_lock_splash_screen_limits";

// A preference to know whether or not the Arc.PlayStoreLaunchWithinAWeek
// metric can been recorded.
const char kArcPlayStoreLaunchMetricCanBeRecorded[] =
    "arc.playstore_launched_by_user";

// An integer preference to count how many times ARCVM /data migration has been
// automatically resumed.
const char kArcVmDataMigrationAutoResumeCount[] =
    "arc.vm_data_migration_auto_resume_count";

// A time preference to indicate when the ARCVM /data migration notification is
// shown for the first time.
const char kArcVmDataMigrationNotificationFirstShownTime[] =
    "arc.vm_data_migration_notification_first_shown_time";

// An integer preference to indicate the status of ARCVM /data migration.
const char kArcVmDataMigrationStatus[] = "arc.vm_data_migration_status";

// A preference that indicates whether links supported by Android apps should be
// opened in the browser by default.
const char kArcOpenLinksInBrowserByDefault[] =
    "arc.open_links_in_browser_by_default";

// ======== LOCAL STATE PREFS ========
// ANR count which is currently pending, not flashed to UMA.
const char kAnrPendingCount[] = "arc.anr_pending_count";

// Keeps the duration of the current ANR rate period.
const char kAnrPendingDuration[] = "arc.anr_pending_duration";

// A dictionary preference that keeps track of stability metric values, which is
// maintained by StabilityMetricsManager. Persisting values in local state is
// required to include these metrics in the initial stability log in case of a
// crash.
const char kStabilityMetrics[] = "arc.metrics.stability";

// A preference to keep the salt for generating ro.serialno and ro.boot.serialno
// Android properties. Used only in ARCVM.
const char kArcSerialNumberSalt[] = "arc.serialno_salt";

// A preferece to keep ARC snapshot related info in dictionary.
const char kArcSnapshotInfo[] = "arc.snapshot";

// A time pref indicating the time in microseconds when ARCVM success executed
// vmm swap out. If it never swapped out, the pref holds the default value
// base::Time().
const char kArcVmmSwapOutTime[] = "arc_vmm_swap_out_time";

// A preference to keep track of whether or not Android WebView was used in the
// current ARC session.
const char kWebViewProcessStarted[] = "arc.webview.started";

// Tells us whether the initial location setting sync is required or not. With
// Privacy Hub for ChromeOS this setting is needed to migrate the location
// settings from existing android settings to ChromeOS.
// Default value is true, once done we set it to false as we want to honor the
// ChromeOS settings at boot from now on. Also in case of first time login or
// arc opt-in, we will set this value to false.
const char kArcInitialLocationSettingSyncRequired[] =
    "arc.initial.location.setting.sync.required";

// An integer preference to indicate the strategy of ARCVM /data migration for
// enterprise user.
const char kArcVmDataMigrationStrategy[] = "arc.vm_data_migration_strategy";

// A preference representing if ARC is allowed on unaffiliated devices
// of an enterprise account
const char kUnaffiliatedDeviceArcAllowed[] = "arc.unaffiliated.device.allowed";

// A preference indicating the last locale set for any apps. This will be used
// as part of suggested locales for other apps' locale setting.
const char kArcLastSetAppLocale[] = "arc.last_set_app_locale";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  // Sorted in lexicographical order.
  RegisterDailyMetricsPrefs(registry);
  registry->RegisterStringPref(kArcSerialNumberSalt, std::string());
  registry->RegisterDictionaryPref(kArcSnapshotInfo);
  registry->RegisterTimePref(kArcVmmSwapOutTime, base::Time());
  registry->RegisterDictionaryPref(kStabilityMetrics);

  registry->RegisterIntegerPref(kAnrPendingCount, 0);
  registry->RegisterTimeDeltaPref(kAnrPendingDuration, base::TimeDelta());
  registry->RegisterBooleanPref(kWebViewProcessStarted, false);
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // TODO(dspaid): Implement a mechanism to allow this to sync on first boot
  // only.

  // This is used to delete the Play user ID if ARC is disabled for an
  // Active Directory managed device.
  registry->RegisterStringPref(kArcActiveDirectoryPlayUserId, std::string());

  // Note that ArcBackupRestoreEnabled and ArcLocationServiceEnabled prefs have
  // to be off by default, until an explicit gesture from the user to enable
  // them is received. This is crucial in the cases when these prefs transition
  // from a previous managed state to the unmanaged.
  registry->RegisterBooleanPref(kArcBackupRestoreEnabled, false);
  registry->RegisterBooleanPref(kArcLocationServiceEnabled, false);

  registry->RegisterIntegerPref(
      kArcManagementTransition,
      static_cast<int>(ArcManagementTransition::NO_TRANSITION));

  registry->RegisterBooleanPref(kArcIsManaged, false);

  guest_os::prefs::RegisterEngagementProfilePrefs(registry,
                                                  kEngagementPrefsPrefix);

  // Sorted in lexicographical order.
  registry->RegisterBooleanPref(kAlwaysOnVpnLockdown, false);
  registry->RegisterStringPref(kAlwaysOnVpnPackage, std::string());
  registry->RegisterBooleanPref(kArcDataRemoveRequested, false);
  registry->RegisterBooleanPref(kArcEnabled, false);
  registry->RegisterBooleanPref(kArcHasAccessToRemovableMedia, false);
  registry->RegisterBooleanPref(kArcInitialSettingsPending, false);
  registry->RegisterBooleanPref(kArcInitialLocationSettingSyncRequired, true);
  registry->RegisterStringPref(kArcLastSetAppLocale, std::string());
  registry->RegisterBooleanPref(kArcOpenLinksInBrowserByDefault, false);
  registry->RegisterBooleanPref(kArcPaiStarted, false);
  registry->RegisterBooleanPref(kArcFastAppReinstallStarted, false);
  registry->RegisterListPref(kArcFastAppReinstallPackages);
  registry->RegisterListPref(
      kArcFirstActivationDuringUserSessionStartUpHistory);
  registry->RegisterBooleanPref(kArcPolicyComplianceReported, false);
  registry->RegisterBooleanPref(kArcProvisioningInitiatedFromOobe, false);
  registry->RegisterBooleanPref(kArcSignedIn, false);
  registry->RegisterBooleanPref(kArcSkippedReportingNotice, false);
  registry->RegisterBooleanPref(kArcTermsAccepted, false);
  registry->RegisterListPref(kArcVisibleExternalStorages);
  registry->RegisterIntegerPref(kArcVmDataMigrationAutoResumeCount, 0);
  registry->RegisterTimePref(kArcVmDataMigrationNotificationFirstShownTime,
                             base::Time());
  registry->RegisterIntegerPref(
      kArcVmDataMigrationStatus,
      static_cast<int>(ArcVmDataMigrationStatus::kUnnotified));
  registry->RegisterIntegerPref(
      kArcVmDataMigrationStrategy,
      static_cast<int>(ArcVmDataMigrationStrategy::kDoNotPrompt));
  registry->RegisterBooleanPref(kUnaffiliatedDeviceArcAllowed, true);
}

}  // namespace prefs
}  // namespace arc

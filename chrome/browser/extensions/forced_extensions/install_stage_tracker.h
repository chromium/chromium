// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALL_STAGE_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALL_STAGE_TRACKER_H_

#include <map>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/install_stage.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/browser/updater/safe_manifest_parser.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user_type.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Tracker for extension installation events. Different parts of extension
// installation process report stage changes and failures to this one, where
// these events could be observed or retrieved later.
class InstallStageTracker : public KeyedService {
 public:
  // Stage of extension installing process. Typically forced extensions from
  // policies should go through all stages in this order, other extensions skip
  // CREATED stage. The stages are recorded in the increasing order of their
  // values, therefore always verify that values are in increasing order and
  // items are in order in which they appear. Exceptions are handled in
  // ShouldOverrideCurrentStage method. Note: enum used for UMA. Do NOT reorder
  // or remove entries. Don't forget to update enums.xml (name:
  // ExtensionInstallationStage) when adding new entries. Don't forget to update
  // device_management_backend.proto (name:
  // ExtensionInstallReportLogEvent::InstallationStage) when adding new entries.
  // Don't forget to update ConvertInstallationStageToProto method in
  // ExtensionInstallEventLogCollector.
  enum class Stage {
    // Extension found in ForceInstall policy and added to
    // ExtensionManagement::settings_by_id_.
    CREATED = 0,

    // NOTIFIED_FROM_MANAGEMENT = 5, // Moved to InstallCreationStage.

    // NOTIFIED_FROM_MANAGEMENT_NOT_FORCED = 6, // Moved to
    // InstallCreationStage.

    // SEEN_BY_POLICY_LOADER = 7, // Moved to InstallCreationStage.

    // SEEN_BY_EXTERNAL_PROVIDER = 8, // Moved to InstallCreationStage.

    // Extension added to PendingExtensionManager.
    PENDING = 1,

    // Extension added to ExtensionDownloader.
    DOWNLOADING = 2,

    // Extension archive downloaded and is about to be unpacked/checked/etc.
    INSTALLING = 3,

    // Extension installation finished (either successfully or not).
    COMPLETE = 4,

    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = COMPLETE,
  };

  // Intermediate stage of extension installation when the Stage is CREATED.
  // TODO(crbug.com/40638368): These stages are temporary ones for
  // investigation. Remove them after investigation will complete. Note: enum
  // used for UMA. Do NOT reorder or remove entries. Don't forget to update
  // enums.xml (name: InstallCreationStage) when adding new entries. Don't
  // forget to update device_management_backend.proto (name:
  // ExtensionInstallReportLogEvent::InstallCreationStage) when adding new
  // entries. Don't forget to update ConvertInstallCreationStageToProto method
  // in ExtensionInstallEventLogCollector.
  enum InstallCreationStage {
    UNKNOWN = 0,

    // ExtensionManagement has reported the Stage has Stage::CREATED.
    CREATION_INITIATED = 1,

    // Installation mode for the extension is set to INSTALLATION_FORCED just
    // after ExtensionManagement class is created and CREATION_INITIATED has
    // been reported.
    NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED = 2,

    // Installation mode for the extension is set to other mode just after
    // ExtensionManagement class is created and CREATION_INITIATED has been
    // reported.
    NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_NOT_FORCED = 3,

    // ExtensionManagement class is about to pass extension with
    // INSTALLATION_FORCED mode to its observers.
    NOTIFIED_FROM_MANAGEMENT = 4,

    // ExtensionManagement class is about to pass extension with other mode to
    // its observers.
    NOTIFIED_FROM_MANAGEMENT_NOT_FORCED = 5,

    // ExternalPolicyLoader with FORCED type fetches extension from
    // ExtensionManagement.
    SEEN_BY_POLICY_LOADER = 6,

    // ExternalProviderImpl receives extension.
    SEEN_BY_EXTERNAL_PROVIDER = 7,

    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = SEEN_BY_EXTERNAL_PROVIDER,
  };

  // Enum used for UMA. Do NOT reorder or remove entries. Don't forget to
  // update enums.xml (name: ExtensionInstallationFailureReason) when adding new
  // entries. Don't forget to update device_management_backend.proto (name:
  // ExtensionInstallReportLogEvent::FailureReason) when adding new entries.
  // Don't forget to update ConvertFailureReasonToProto method in
  // ExtensionInstallEventLogCollector.
  enum class FailureReason {
    // Reason for the failure is not reported. Typically this should not happen,
    // because if we know that we need to install an extension, it should
    // immediately switch to CREATED stage leading to IN_PROGRESS failure
    // reason, not UNKNOWN.
    UNKNOWN = 0,

    // Invalid id of the extension.
    INVALID_ID = 1,

    // Error during parsing extension individual settings.
    MALFORMED_EXTENSION_SETTINGS = 2,

    // The extension is marked as replaced by ARC app.
    REPLACED_BY_ARC_APP = 3,

    // Malformed extension dictionary for the extension.
    MALFORMED_EXTENSION_DICT = 4,

    // The extension format from extension dict is not supported.
    NOT_SUPPORTED_EXTENSION_DICT = 5,

    // Invalid file path in the extension dict.
    MALFORMED_EXTENSION_DICT_FILE_PATH = 6,

    // Invalid version in the extension dict.
    MALFORMED_EXTENSION_DICT_VERSION = 7,

    // Invalid updated URL in the extension dict.
    MALFORMED_EXTENSION_DICT_UPDATE_URL = 8,

    // The extension doesn't support browser locale.
    LOCALE_NOT_SUPPORTED = 9,

    // The extension marked as it shouldn't be installed.
    NOT_PERFORMING_NEW_INSTALL = 10,

    // Profile is older than supported by the extension.
    TOO_OLD_PROFILE = 11,

    // The extension can't be installed for enterprise.
    DO_NOT_INSTALL_FOR_ENTERPRISE = 12,

    // The extension is already installed.
    ALREADY_INSTALLED = 13,

    // The download of the crx failed.
    CRX_FETCH_FAILED = 14,

    // Failed to fetch the manifest for this extension.
    MANIFEST_FETCH_FAILED = 15,

    // The manifest couldn't be parsed.
    MANIFEST_INVALID = 16,

    // The manifest was fetched and parsed, and there are no updates for this
    // extension.
    NO_UPDATE = 17,

    // The crx was downloaded, but failed to install.
    // Corresponds to CrxInstallErrorType.
    CRX_INSTALL_ERROR_DECLINED = 18,
    CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE = 19,
    CRX_INSTALL_ERROR_OTHER = 20,

    // Extensions without update url should receive a default one, but somewhy
    // this didn't work. Internal error, should never happen.
    NO_UPDATE_URL = 21,

    // Extension failed to add to PendingExtensionManager.
    PENDING_ADD_FAILED = 22,

    // ExtensionDownloader refuses to start downloading this extensions
    // (possible reasons: invalid ID/URL).
    DOWNLOADER_ADD_FAILED = 23,

    // Extension (at the moment of check) is not installed nor some installation
    // error reported, so extension is being installed now, stuck in some stage
    // or some failure was not reported. See enum Stage for more details.
    // This option is a failure only in the sense that we failed to install
    // extension in required time.
    IN_PROGRESS = 24,

    // The download of the crx failed. In past histograms, this error has only
    // occurred when the update check status is "no update" in the manifest. See
    // crbug/1063031 for more details.
    CRX_FETCH_URL_EMPTY = 25,

    // The download of the crx failed.
    CRX_FETCH_URL_INVALID = 26,

    // Applying the ExtensionSettings policy changed installation mode from
    // force-installed to anything else.
    OVERRIDDEN_BY_SETTINGS = 27,

    // The extension is marked as replaced by system app.
    REPLACED_BY_SYSTEM_APP = 28,

    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = REPLACED_BY_SYSTEM_APP,
  };

  // Status for the app returned by server while fetching manifest when status
  // was not OK. Enum used for UMA. Do NOT reorder or remove entries. Don't
  // forget to update enums.xml (name: ManifestInvalidAppStatusError) when
  // adding new entries.
  enum class AppStatusError {
    // Technically it may happen that update server return some unknown value or
    // no value.
    kUnknown = 0,

    // The appid was not recognized and no action elements are included.
    kErrorUnknownApplication = 1,

    // The appid is not properly formed; no action elements are included.
    kErrorInvalidAppId = 2,

    // The application is not available to this user (usually based on country
    // export restrictions).
    kErrorRestricted = 3,

    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = kErrorRestricted,
  };

  // Info field in the update manifest returned by the server when no update is
  // available. Enum used for UMA. Do NOT reorder or remove entries. Don't
  // forget to update enums.xml (name: ExtensionNoUpdatesInfo) when adding new
  // entries.
  enum class NoUpdatesInfo {
    // Update server returns some unknown value.
    kUnknown = 0,
    // Update server returns empty info.
    kEmpty = 1,
    // Popular no update reasons are marked as "rate limit", "disabled by
    // client" and "bandwidth limit".
    kRateLimit = 2,
    kDisabledByClient = 3,
    kBandwidthLimit = 4,
    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = kBandwidthLimit,
  };

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Contains information about the current user.
  struct UserInfo {
    UserInfo();
    UserInfo(const UserInfo&);
    UserInfo(user_manager::UserType user_type,
             bool is_new_user,
             bool is_user_present);

    user_manager::UserType user_type = user_manager::UserType::kRegular;
    const bool is_new_user = false;
    const bool is_user_present = false;
  };
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Contains information about extension installation: failure reason, if any
  // reported, specific details in case of CRX install error, current
  // installation stage if known.
  struct InstallationData {
    InstallationData();
    ~InstallationData();
    InstallationData(const InstallationData&);

    std::optional<Stage> install_stage;
    std::optional<InstallCreationStage> install_creation_stage;
    std::optional<ExtensionDownloaderDelegate::Stage> downloading_stage;
    std::optional<ExtensionDownloaderDelegate::CacheStatus>
        downloading_cache_status;
    std::optional<FailureReason> failure_reason;
    std::optional<CrxInstallErrorDetail> install_error_detail;
    // Network error codes and fetch tries when applicable:
    // * failure_reason is CRX_FETCH_FAILED or MANIFEST_FETCH_FAILED
    // * `downloading_stage` is DOWNLOAD_MANIFEST_RETRY or DOWNLOAD_CRX_RETRY.
    std::optional<int> network_error_code;
    std::optional<int> response_code;
    std::optional<int> fetch_tries;
    // Unpack failure reason in case of
    // CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE.
    std::optional<SandboxedUnpackerFailureReason> unpacker_failure_reason;
    // Type of extension, assigned during CRX installation process.
    std::optional<Manifest::Type> extension_type;
    // Error detail when the fetched manifest was invalid. This includes errors
    // occurred while parsing the manifest and errors occurred due to the
    // internal details of the parsed manifest.
    std::optional<ManifestInvalidError> manifest_invalid_error;
    // Info field in the update manifest returned by the server when no update
    // is available.
    std::optional<NoUpdatesInfo> no_updates_info;
    // Type of app status error received from update server when manifest was
    // fetched.
    std::optional<AppStatusError> app_status_error;
    // Time at which the download is started.
    std::optional<base::TimeTicks> download_manifest_started_time;
    // Time at which the update manifest is downloaded and successfully parsed
    // from the server.
    std::optional<base::TimeTicks> download_manifest_finish_time;
    // See InstallationStage enum.
    std::optional<InstallationStage> installation_stage;
    // Time at which the download of CRX is started.
    std::optional<base::TimeTicks> download_CRX_started_time;
    // Time at which CRX is downloaded.
    std::optional<base::TimeTicks> download_CRX_finish_time;
    // Time at which signature verification of CRX is started.
    std::optional<base::TimeTicks> verification_started_time;
    // Time at which copying of extension archive into the working directory is
    // started.
    std::optional<base::TimeTicks> copying_started_time;
    // Time at which unpacking of the extension archive is started.
    std::optional<base::TimeTicks> unpacking_started_time;
    // Time at which the extension archive has been successfully unpacked and
    // the expectation checks before extension installation are started.
    std::optional<base::TimeTicks> checking_expectations_started_time;
    // Time at which the extension has passed the expectation checks and the
    // installation is started.
    std::optional<base::TimeTicks> finalizing_started_time;
    // Time at which the installation process is complete.
    std::optional<base::TimeTicks> installation_complete_time;
    // Detailed error description when extension failed to install with
    // SandboxedUnpackerFailureReason equal to UNPACKER_CLIENT FAILED.
    std::optional<std::u16string> unpacker_client_failed_error;
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    virtual void OnExtensionInstallationFailed(const ExtensionId& id,
                                               FailureReason reason) {}

    // Called when any change happens. For production please use more specific
    // methods (create one if necessary).
    virtual void OnExtensionDataChangedForTesting(
        const ExtensionId& id,
        const content::BrowserContext* context,
        const InstallationData& data) {}

    // Called when InstallStageTracker retrieves cache status for the
    // extension.
    virtual void OnExtensionDownloadCacheStatusRetrieved(
        const ExtensionId& id,
        ExtensionDownloaderDelegate::CacheStatus cache_status) {}

    // Called when installation stage of extension is updated.
    virtual void OnExtensionInstallationStageChanged(const ExtensionId& id,
                                                     Stage stage) {}

    // Called when downloading stage of extension is updated.
    virtual void OnExtensionDownloadingStageChanged(
        const ExtensionId& id,
        ExtensionDownloaderDelegate::Stage stage) {}

    // Called when InstallCreationStage of extension is updated.
    virtual void OnExtensionInstallCreationStageChanged(
        const ExtensionId& id,
        InstallCreationStage stage) {}
  };

  explicit InstallStageTracker(const content::BrowserContext* context);

  ~InstallStageTracker() override;

  InstallStageTracker(const InstallStageTracker&) = delete;
  InstallStageTracker& operator=(const InstallStageTracker&) = delete;

  // Returns instance of InstallStageTracker for a BrowserContext.
  static InstallStageTracker* Get(content::BrowserContext* context);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns user type of the user associated with the `profile` and whether the
  // user is new or not if there is an active user.
  static UserInfo GetUserInfo(Profile* profile);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void ReportInfoOnNoUpdatesFailure(const ExtensionId& id,
                                    const std::string& info);

  // Reports detailed error type when extension fails to install with failure
  // reason MANIFEST_INVALID. See InstallationData::manifest_invalid_error
  // for more details.
  void ReportManifestInvalidFailure(
      const ExtensionId& id,
      const ExtensionDownloaderDelegate::FailureData& failure_data);

  // Remembers failure reason and in-progress stages in memory.
  void ReportInstallationStage(const ExtensionId& id, Stage stage);
  void ReportInstallCreationStage(const ExtensionId& id,
                                  InstallCreationStage stage);
  void ReportFetchErrorCodes(
      const ExtensionId& id,
      const ExtensionDownloaderDelegate::FailureData& failure_data);
  void ReportFetchError(
      const ExtensionId& id,
      FailureReason reason,
      const ExtensionDownloaderDelegate::FailureData& failure_data);
  void ReportFailure(const ExtensionId& id, FailureReason reason);
  void ReportDownloadingStage(const ExtensionId& id,
                              ExtensionDownloaderDelegate::Stage stage);
  void ReportCRXInstallationStage(const ExtensionId& id,
                                  InstallationStage stage);
  void ReportDownloadingCacheStatus(
      const ExtensionId& id,
      ExtensionDownloaderDelegate::CacheStatus cache_status);
  // Assigns the extension type. Reported from SandboxedInstalled when (and in
  // case when) the extension type is discovered.
  // See InstallationData::extension_type for more details.
  void ReportExtensionType(const ExtensionId& id,
                           Manifest::Type extension_type);
  void ReportCrxInstallError(const ExtensionId& id,
                             FailureReason reason,
                             CrxInstallErrorDetail crx_install_error);
  void ReportSandboxedUnpackerFailureReason(
      const ExtensionId& id,
      const CrxInstallError& crx_install_error);

  // Retrieves known information for installation of extension `id`.
  // Returns empty data if not found.
  InstallationData Get(const ExtensionId& id);
  static std::string GetFormattedInstallationData(const InstallationData& data);

  // Clears all collected failures and stages.
  void Clear();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Maps the current app status to AppStatusError enum.
  AppStatusError GetManifestInvalidAppStatusError(const std::string& status);

  // Reports installation failures to the observers.
  void NotifyObserversOfFailure(const ExtensionId& id,
                                FailureReason reason,
                                const InstallationData& data);

  raw_ptr<const content::BrowserContext> browser_context_;

  std::map<ExtensionId, InstallationData> installation_data_map_;

  base::ObserverList<Observer> observers_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALL_STAGE_TRACKER_H_

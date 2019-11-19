// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_REPORTER_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_REPORTER_H_

#include <map>
#include <utility>

#include "base/macros.h"
#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Helper class to save and retrieve extension installation stage and failure
// reasons.
class InstallationReporter : public KeyedService {
 public:
  // Stage of extension installing process. Typically forced extensions from
  // policies should go through all stages in this order, other extensions skip
  // CREATED stage.
  // Note: enum used for UMA. Do NOT reorder or remove entries. Don't forget to
  // update enums.xml (name: ExtensionInstallationStage) when adding new
  // entries.
  enum class Stage {
    // Extension found in ForceInstall policy and added to
    // ExtensionManagement::settings_by_id_.
    CREATED = 0,

    // TODO(crbug.com/989526): stages from NOTIFIED_FROM_MANAGEMENT to
    // SEEN_BY_EXTERNAL_PROVIDER are temporary ones for investigation. Remove
    // then after investigation will complete and we'll be confident in
    // extension handling between CREATED and PENDING.

    // ExtensionManagement class is about to pass extension with
    // INSTALLATION_FORCED mode to its observers.
    NOTIFIED_FROM_MANAGEMENT = 5,

    // ExtensionManagement class is about to pass extension with other mode to
    // its observers.
    NOTIFIED_FROM_MANAGEMENT_NOT_FORCED = 6,

    // ExternalPolicyLoader with FORCED type fetches extension from
    // ExtensionManagement.
    SEEN_BY_POLICY_LOADER = 7,

    // ExternalProviderImpl receives extension.
    SEEN_BY_EXTERNAL_PROVIDER = 8,

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
    kMaxValue = SEEN_BY_EXTERNAL_PROVIDER,
  };

  // Enum used for UMA. Do NOT reorder or remove entries. Don't forget to
  // update enums.xml (name: ExtensionInstallationFailureReason) when adding new
  // entries.
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

    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = IN_PROGRESS,
  };

  // Contains information about extension installation: failure reason, if any
  // reported, specific details in case of CRX install error, current
  // installation stage if known.
  struct InstallationData {
    InstallationData();
    InstallationData(const InstallationData&);

    base::Optional<extensions::InstallationReporter::Stage> install_stage;
    base::Optional<extensions::ExtensionDownloaderDelegate::Stage>
        downloading_stage;
    base::Optional<extensions::ExtensionDownloaderDelegate::CacheStatus>
        downloading_cache_status;
    base::Optional<extensions::InstallationReporter::FailureReason>
        failure_reason;
    base::Optional<extensions::CrxInstallErrorDetail> install_error_detail;
  };

  class TestObserver {
   public:
    virtual ~TestObserver();
    virtual void OnExtensionDataChanged(const ExtensionId& id,
                                        const content::BrowserContext* context,
                                        const InstallationData& data) = 0;
  };

  explicit InstallationReporter(const content::BrowserContext* context);

  ~InstallationReporter() override;

  // Convenience function to get the InstallationReporter for a BrowserContext.
  static InstallationReporter* Get(content::BrowserContext* context);

  // Remembers failure reason and in-progress stages in memory.
  void ReportInstallationStage(const ExtensionId& id, Stage stage);
  void ReportFailure(const ExtensionId& id, FailureReason reason);
  void ReportDownloadingStage(const ExtensionId& id,
                              ExtensionDownloaderDelegate::Stage stage);
  void ReportDownloadingCacheStatus(
      const ExtensionId& id,
      ExtensionDownloaderDelegate::CacheStatus cache_status);
  void ReportCrxInstallError(const ExtensionId& id,
                             FailureReason reason,
                             CrxInstallErrorDetail crx_install_error);

  // Retrieves known information for installation of extension |id|.
  // Returns empty data if not found.
  InstallationData Get(const ExtensionId& id);
  static std::string GetFormattedInstallationData(const InstallationData& data);

  // Clears all collected failures and stages.
  void Clear();

  static void SetTestObserver(TestObserver* observer);

 private:
  const content::BrowserContext* browser_context_;

  std::map<ExtensionId, InstallationReporter::InstallationData>
      installation_data_map_;

  DISALLOW_COPY_AND_ASSIGN(InstallationReporter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_REPORTER_H_

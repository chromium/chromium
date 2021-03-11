// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/extension_install_event_log_collector.h"

#include "base/command_line.h"
#include "base/time/time.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/extension_info.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install/sandboxed_unpacker_failure_reason.h"

namespace em = enterprise_management;

namespace policy {

namespace {

std::unique_ptr<em::ExtensionInstallReportLogEvent> CreateSessionChangeEvent(
    em::ExtensionInstallReportLogEvent::SessionStateChangeType type) {
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_event_type(
      em::ExtensionInstallReportLogEvent::SESSION_STATE_CHANGE);
  event->set_session_state_change_type(type);
  return event;
}

// Helper method to convert InstallStageTracker::FailureReason to the failure
// reason proto.
em::ExtensionInstallReportLogEvent_FailureReason ConvertFailureReasonToProto(
    extensions::InstallStageTracker::FailureReason failure_reason) {
  switch (failure_reason) {
    case extensions::InstallStageTracker::FailureReason::UNKNOWN:
      return em::ExtensionInstallReportLogEvent::FAILURE_REASON_UNKNOWN;
    case extensions::InstallStageTracker::FailureReason::INVALID_ID:
      return em::ExtensionInstallReportLogEvent::INVALID_ID;
    case extensions::InstallStageTracker::FailureReason::
        MALFORMED_EXTENSION_SETTINGS:
      return em::ExtensionInstallReportLogEvent::MALFORMED_EXTENSION_SETTINGS;
    case extensions::InstallStageTracker::FailureReason::REPLACED_BY_ARC_APP:
      return em::ExtensionInstallReportLogEvent::REPLACED_BY_ARC_APP;
    case extensions::InstallStageTracker::FailureReason::
        MALFORMED_EXTENSION_DICT:
      return em::ExtensionInstallReportLogEvent::MALFORMED_EXTENSION_DICT;
    case extensions::InstallStageTracker::FailureReason::
        NOT_SUPPORTED_EXTENSION_DICT:
      return em::ExtensionInstallReportLogEvent::NOT_SUPPORTED_EXTENSION_DICT;
    case extensions::InstallStageTracker::FailureReason::
        MALFORMED_EXTENSION_DICT_FILE_PATH:
      return em::ExtensionInstallReportLogEvent::
          MALFORMED_EXTENSION_DICT_FILE_PATH;
    case extensions::InstallStageTracker::FailureReason::
        MALFORMED_EXTENSION_DICT_VERSION:
      return em::ExtensionInstallReportLogEvent::
          MALFORMED_EXTENSION_DICT_VERSION;
    case extensions::InstallStageTracker::FailureReason::
        MALFORMED_EXTENSION_DICT_UPDATE_URL:
      return em::ExtensionInstallReportLogEvent::
          MALFORMED_EXTENSION_DICT_UPDATE_URL;
    case extensions::InstallStageTracker::FailureReason::LOCALE_NOT_SUPPORTED:
      return em::ExtensionInstallReportLogEvent::LOCALE_NOT_SUPPORTED;
    case extensions::InstallStageTracker::FailureReason::
        NOT_PERFORMING_NEW_INSTALL:
      return em::ExtensionInstallReportLogEvent::NOT_PERFORMING_NEW_INSTALL;
    case extensions::InstallStageTracker::FailureReason::TOO_OLD_PROFILE:
      return em::ExtensionInstallReportLogEvent::TOO_OLD_PROFILE;
    case extensions::InstallStageTracker::FailureReason::
        DO_NOT_INSTALL_FOR_ENTERPRISE:
      return em::ExtensionInstallReportLogEvent::DO_NOT_INSTALL_FOR_ENTERPRISE;
    case extensions::InstallStageTracker::FailureReason::ALREADY_INSTALLED:
      return em::ExtensionInstallReportLogEvent::ALREADY_INSTALLED;
    case extensions::InstallStageTracker::FailureReason::CRX_FETCH_FAILED:
      return em::ExtensionInstallReportLogEvent::CRX_FETCH_FAILED;
    case extensions::InstallStageTracker::FailureReason::MANIFEST_FETCH_FAILED:
      return em::ExtensionInstallReportLogEvent::MANIFEST_FETCH_FAILED;
    case extensions::InstallStageTracker::FailureReason::MANIFEST_INVALID:
      return em::ExtensionInstallReportLogEvent::MANIFEST_INVALID;
    case extensions::InstallStageTracker::FailureReason::NO_UPDATE:
      return em::ExtensionInstallReportLogEvent::NO_UPDATE;
    case extensions::InstallStageTracker::FailureReason::
        CRX_INSTALL_ERROR_DECLINED:
      return em::ExtensionInstallReportLogEvent::CRX_INSTALL_ERROR_DECLINED;
    case extensions::InstallStageTracker::FailureReason::
        CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE:
      return em::ExtensionInstallReportLogEvent::
          CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE;
    case extensions::InstallStageTracker::FailureReason::
        CRX_INSTALL_ERROR_OTHER:
      return em::ExtensionInstallReportLogEvent::CRX_INSTALL_ERROR_OTHER;
    case extensions::InstallStageTracker::FailureReason::NO_UPDATE_URL:
      return em::ExtensionInstallReportLogEvent::NO_UPDATE_URL;
    case extensions::InstallStageTracker::FailureReason::PENDING_ADD_FAILED:
      return em::ExtensionInstallReportLogEvent::PENDING_ADD_FAILED;
    case extensions::InstallStageTracker::FailureReason::DOWNLOADER_ADD_FAILED:
      return em::ExtensionInstallReportLogEvent::DOWNLOADER_ADD_FAILED;
    case extensions::InstallStageTracker::FailureReason::IN_PROGRESS:
      return em::ExtensionInstallReportLogEvent::IN_PROGRESS;
    case extensions::InstallStageTracker::FailureReason::CRX_FETCH_URL_EMPTY:
      return em::ExtensionInstallReportLogEvent::CRX_FETCH_URL_EMPTY;
    case extensions::InstallStageTracker::FailureReason::CRX_FETCH_URL_INVALID:
      return em::ExtensionInstallReportLogEvent::CRX_FETCH_URL_INVALID;
    case extensions::InstallStageTracker::FailureReason::OVERRIDDEN_BY_SETTINGS:
      return em::ExtensionInstallReportLogEvent::OVERRIDDEN_BY_SETTINGS;
    case extensions::InstallStageTracker::FailureReason::REPLACED_BY_SYSTEM_APP:
      return em::ExtensionInstallReportLogEvent::REPLACED_BY_SYSTEM_APP;
    default:
      NOTREACHED();
  }
}

// Helper method to convert InstallStageTracker::Stage to the Stage proto.
em::ExtensionInstallReportLogEvent_InstallationStage
ConvertInstallationStageToProto(extensions::InstallStageTracker::Stage stage) {
  using Stage = extensions::InstallStageTracker::Stage;
  switch (stage) {
    case Stage::CREATED:
      return em::ExtensionInstallReportLogEvent::CREATED;
    case Stage::PENDING:
      return em::ExtensionInstallReportLogEvent::PENDING;
    case Stage::DOWNLOADING:
      return em::ExtensionInstallReportLogEvent::DOWNLOADING;
    case Stage::INSTALLING:
      return em::ExtensionInstallReportLogEvent::INSTALLING;
    case Stage::COMPLETE:
      return em::ExtensionInstallReportLogEvent::COMPLETE;
    default:
      NOTREACHED();
      return em::ExtensionInstallReportLogEvent::INSTALLATION_STAGE_UNKNOWN;
  }
}

// Helper method to convert InstallStageTracker::UserType to the user
// type proto.
em::ExtensionInstallReportLogEvent_UserType ConvertUserTypeToProto(
    user_manager::UserType user_type) {
  switch (user_type) {
    case user_manager::USER_TYPE_REGULAR:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_REGULAR;
    case user_manager::USER_TYPE_GUEST:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_GUEST;
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_PUBLIC_ACCOUNT;
    case user_manager::USER_TYPE_SUPERVISED_DEPRECATED:
      return em::ExtensionInstallReportLogEvent::
          USER_TYPE_SUPERVISED_DEPRECATED;
    case user_manager::USER_TYPE_KIOSK_APP:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_KIOSK_APP;
    case user_manager::USER_TYPE_CHILD:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_CHILD;
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_ARC_KIOSK_APP;
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_ACTIVE_DIRECTORY;
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_WEB_KIOSK_APP;
    case user_manager::NUM_USER_TYPES:
      NOTREACHED();
      return em::ExtensionInstallReportLogEvent::USER_TYPE_UNKNOWN;
  }
}

// Helper method to convert ExtensionDownloaderDelegate::Stage to the
// DownloadingStage proto.
em::ExtensionInstallReportLogEvent_DownloadingStage
ConvertDownloadingStageToProto(
    extensions::ExtensionDownloaderDelegate::Stage stage) {
  using DownloadingStage = extensions::ExtensionDownloaderDelegate::Stage;
  switch (stage) {
    case DownloadingStage::PENDING:
      return em::ExtensionInstallReportLogEvent::DOWNLOAD_PENDING;
    case DownloadingStage::QUEUED_FOR_MANIFEST:
      return em::ExtensionInstallReportLogEvent::QUEUED_FOR_MANIFEST;
    case DownloadingStage::DOWNLOADING_MANIFEST:
      return em::ExtensionInstallReportLogEvent::DOWNLOADING_MANIFEST;
    case DownloadingStage::DOWNLOADING_MANIFEST_RETRY:
      return em::ExtensionInstallReportLogEvent::DOWNLOADING_MANIFEST_RETRY;
    case DownloadingStage::PARSING_MANIFEST:
      return em::ExtensionInstallReportLogEvent::PARSING_MANIFEST;
    case DownloadingStage::MANIFEST_LOADED:
      return em::ExtensionInstallReportLogEvent::MANIFEST_LOADED;
    case DownloadingStage::QUEUED_FOR_CRX:
      return em::ExtensionInstallReportLogEvent::QUEUED_FOR_CRX;
    case DownloadingStage::DOWNLOADING_CRX:
      return em::ExtensionInstallReportLogEvent::DOWNLOADING_CRX;
    case DownloadingStage::DOWNLOADING_CRX_RETRY:
      return em::ExtensionInstallReportLogEvent::DOWNLOADING_CRX_RETRY;
    case DownloadingStage::FINISHED:
      return em::ExtensionInstallReportLogEvent::FINISHED;
    default:
      NOTREACHED();
      return em::ExtensionInstallReportLogEvent::DOWNLOADING_STAGE_UNKNOWN;
  }
}

em::ExtensionInstallReportLogEvent_InstallCreationStage
ConvertInstallCreationStageToProto(
    extensions::InstallStageTracker::InstallCreationStage stage) {
  using Stage = extensions::InstallStageTracker::InstallCreationStage;
  switch (stage) {
    case Stage::CREATION_INITIATED:
      return em::ExtensionInstallReportLogEvent::CREATION_INITIATED;
    case Stage::NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED:
      return em::ExtensionInstallReportLogEvent::
          NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED;
    case Stage::NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_NOT_FORCED:
      return em::ExtensionInstallReportLogEvent::
          NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_NOT_FORCED;
    case Stage::NOTIFIED_FROM_MANAGEMENT:
      return em::ExtensionInstallReportLogEvent::NOTIFIED_FROM_MANAGEMENT;
    case Stage::NOTIFIED_FROM_MANAGEMENT_NOT_FORCED:
      return em::ExtensionInstallReportLogEvent::
          NOTIFIED_FROM_MANAGEMENT_NOT_FORCED;
    case Stage::SEEN_BY_POLICY_LOADER:
      return em::ExtensionInstallReportLogEvent::SEEN_BY_POLICY_LOADER;
    case Stage::SEEN_BY_EXTERNAL_PROVIDER:
      return em::ExtensionInstallReportLogEvent::SEEN_BY_EXTERNAL_PROVIDER;
    default:
      NOTREACHED();
      return em::ExtensionInstallReportLogEvent::INSTALL_CREATION_STAGE_UNKNOWN;
  }
}

em::ExtensionInstallReportLogEvent_DownloadCacheStatus
ConvertDownloadCacheStatusToProto(
    extensions::ExtensionDownloaderDelegate::CacheStatus status) {
  using Status = extensions::ExtensionDownloaderDelegate::CacheStatus;
  switch (status) {
    case Status::CACHE_UNKNOWN:
      return em::ExtensionInstallReportLogEvent::CACHE_UNKNOWN;
    case Status::CACHE_DISABLED:
      return em::ExtensionInstallReportLogEvent::CACHE_DISABLED;
    case Status::CACHE_MISS:
      return em::ExtensionInstallReportLogEvent::CACHE_MISS;
    case Status::CACHE_OUTDATED:
      return em::ExtensionInstallReportLogEvent::CACHE_OUTDATED;
    case Status::CACHE_HIT:
      return em::ExtensionInstallReportLogEvent::CACHE_HIT;
    case Status::CACHE_HIT_ON_MANIFEST_FETCH_FAILURE:
      return em::ExtensionInstallReportLogEvent::
          CACHE_HIT_ON_MANIFEST_FETCH_FAILURE;
    default:
      NOTREACHED();
      return em::ExtensionInstallReportLogEvent::CACHE_UNKNOWN;
  }
}

// Helper function to convert extensions::SandboxedUnpackerFailureReason to the
// ExtensionInstallReportLogEvent::SandboxedUnpackerFailureReason proto.
em::ExtensionInstallReportLogEvent_SandboxedUnpackerFailureReason
ConvertUnpackerFailureReasonToProto(
    extensions::SandboxedUnpackerFailureReason reason) {
  using FailureReason = extensions::SandboxedUnpackerFailureReason;
  switch (reason) {
    case FailureReason::COULD_NOT_GET_TEMP_DIRECTORY:
      return em::ExtensionInstallReportLogEvent::COULD_NOT_GET_TEMP_DIRECTORY;
    case FailureReason::COULD_NOT_CREATE_TEMP_DIRECTORY:
      return em::ExtensionInstallReportLogEvent::
          COULD_NOT_CREATE_TEMP_DIRECTORY;
    case FailureReason::FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY:
      return em::ExtensionInstallReportLogEvent::
          FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY;
    case FailureReason::COULD_NOT_GET_SANDBOX_FRIENDLY_PATH:
      return em::ExtensionInstallReportLogEvent::
          COULD_NOT_GET_SANDBOX_FRIENDLY_PATH;
    case FailureReason::COULD_NOT_LOCALIZE_EXTENSION:
      return em::ExtensionInstallReportLogEvent::COULD_NOT_LOCALIZE_EXTENSION;
    case FailureReason::INVALID_MANIFEST:
      return em::ExtensionInstallReportLogEvent::INVALID_MANIFEST;
    case FailureReason::UNPACKER_CLIENT_FAILED:
      return em::ExtensionInstallReportLogEvent::UNPACKER_CLIENT_FAILED;
    case FailureReason::UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL:
      return em::ExtensionInstallReportLogEvent::
          UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL;
    case FailureReason::CRX_FILE_NOT_READABLE:
      return em::ExtensionInstallReportLogEvent::CRX_FILE_NOT_READABLE;
    case FailureReason::CRX_HEADER_INVALID:
      return em::ExtensionInstallReportLogEvent::CRX_HEADER_INVALID;
    case FailureReason::CRX_MAGIC_NUMBER_INVALID:
      return em::ExtensionInstallReportLogEvent::CRX_MAGIC_NUMBER_INVALID;
    case FailureReason::CRX_VERSION_NUMBER_INVALID:
      return em::ExtensionInstallReportLogEvent::CRX_VERSION_NUMBER_INVALID;
    case FailureReason::CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE:
      return em::ExtensionInstallReportLogEvent::
          CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE;
    case FailureReason::CRX_ZERO_KEY_LENGTH:
      return em::ExtensionInstallReportLogEvent::CRX_ZERO_KEY_LENGTH;
    case FailureReason::CRX_ZERO_SIGNATURE_LENGTH:
      return em::ExtensionInstallReportLogEvent::CRX_ZERO_SIGNATURE_LENGTH;
    case FailureReason::CRX_PUBLIC_KEY_INVALID:
      return em::ExtensionInstallReportLogEvent::CRX_PUBLIC_KEY_INVALID;
    case FailureReason::CRX_SIGNATURE_INVALID:
      return em::ExtensionInstallReportLogEvent::CRX_SIGNATURE_INVALID;
    case FailureReason::CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED:
      return em::ExtensionInstallReportLogEvent::
          CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED;
    case FailureReason::CRX_SIGNATURE_VERIFICATION_FAILED:
      return em::ExtensionInstallReportLogEvent::
          CRX_SIGNATURE_VERIFICATION_FAILED;
    case FailureReason::ERROR_SERIALIZING_MANIFEST_JSON:
      return em::ExtensionInstallReportLogEvent::
          ERROR_SERIALIZING_MANIFEST_JSON;
    case FailureReason::ERROR_SAVING_MANIFEST_JSON:
      return em::ExtensionInstallReportLogEvent::ERROR_SAVING_MANIFEST_JSON;
    case FailureReason::COULD_NOT_READ_IMAGE_DATA_FROM_DISK_UNUSED:
      return em::ExtensionInstallReportLogEvent::
          COULD_NOT_READ_IMAGE_DATA_FROM_DISK_UNUSED;
    case FailureReason::DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST_UNUSED:
      return em::ExtensionInstallReportLogEvent::
          DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST_UNUSED;
    case FailureReason::INVALID_PATH_FOR_BROWSER_IMAGE:
      return em::ExtensionInstallReportLogEvent::INVALID_PATH_FOR_BROWSER_IMAGE;
    case FailureReason::ERROR_REMOVING_OLD_IMAGE_FILE:
      return em::ExtensionInstallReportLogEvent::ERROR_REMOVING_OLD_IMAGE_FILE;
    case FailureReason::INVALID_PATH_FOR_BITMAP_IMAGE:
      return em::ExtensionInstallReportLogEvent::INVALID_PATH_FOR_BITMAP_IMAGE;
    case FailureReason::ERROR_RE_ENCODING_THEME_IMAGE:
      return em::ExtensionInstallReportLogEvent::ERROR_RE_ENCODING_THEME_IMAGE;
    case FailureReason::ERROR_SAVING_THEME_IMAGE:
      return em::ExtensionInstallReportLogEvent::ERROR_SAVING_THEME_IMAGE;
    case FailureReason::DEPRECATED_ABORTED_DUE_TO_SHUTDOWN:
      return em::ExtensionInstallReportLogEvent::
          DEPRECATED_ABORTED_DUE_TO_SHUTDOWN;
    case FailureReason::COULD_NOT_READ_CATALOG_DATA_FROM_DISK_UNUSED:
      return em::ExtensionInstallReportLogEvent::
          COULD_NOT_READ_CATALOG_DATA_FROM_DISK_UNUSED;
    case FailureReason::INVALID_CATALOG_DATA:
      return em::ExtensionInstallReportLogEvent::INVALID_CATALOG_DATA;
    case FailureReason::INVALID_PATH_FOR_CATALOG_UNUSED:
      return em::ExtensionInstallReportLogEvent::
          INVALID_PATH_FOR_CATALOG_UNUSED;
    case FailureReason::ERROR_SERIALIZING_CATALOG:
      return em::ExtensionInstallReportLogEvent::ERROR_SERIALIZING_CATALOG;
    case FailureReason::ERROR_SAVING_CATALOG:
      return em::ExtensionInstallReportLogEvent::ERROR_SAVING_CATALOG;
    case FailureReason::CRX_HASH_VERIFICATION_FAILED:
      return em::ExtensionInstallReportLogEvent::CRX_HASH_VERIFICATION_FAILED;
    case FailureReason::UNZIP_FAILED:
      return em::ExtensionInstallReportLogEvent::UNZIP_FAILED;
    case FailureReason::DIRECTORY_MOVE_FAILED:
      return em::ExtensionInstallReportLogEvent::DIRECTORY_MOVE_FAILED;
    case FailureReason::CRX_FILE_IS_DELTA_UPDATE:
      return em::ExtensionInstallReportLogEvent::CRX_FILE_IS_DELTA_UPDATE;
    case FailureReason::CRX_EXPECTED_HASH_INVALID:
      return em::ExtensionInstallReportLogEvent::CRX_EXPECTED_HASH_INVALID;
    case FailureReason::DEPRECATED_ERROR_PARSING_DNR_RULESET:
      return em::ExtensionInstallReportLogEvent::
          DEPRECATED_ERROR_PARSING_DNR_RULESET;
    case FailureReason::ERROR_INDEXING_DNR_RULESET:
      return em::ExtensionInstallReportLogEvent::ERROR_INDEXING_DNR_RULESET;
    case FailureReason::CRX_REQUIRED_PROOF_MISSING:
      return em::ExtensionInstallReportLogEvent::CRX_REQUIRED_PROOF_MISSING;
    case FailureReason::CRX_HEADER_VERIFIED_CONTENTS_UNCOMPRESSING_FAILURE:
      return em::ExtensionInstallReportLogEvent::
          CRX_HEADER_VERIFIED_CONTENTS_UNCOMPRESSING_FAILURE;
    case FailureReason::MALFORMED_VERIFIED_CONTENTS:
      return em::ExtensionInstallReportLogEvent::MALFORMED_VERIFIED_CONTENTS;
    case FailureReason::COULD_NOT_CREATE_METADATA_DIRECTORY:
      return em::ExtensionInstallReportLogEvent::
          COULD_NOT_CREATE_METADATA_DIRECTORY;
    case FailureReason::COULD_NOT_WRITE_VERIFIED_CONTENTS_INTO_FILE:
      return em::ExtensionInstallReportLogEvent::
          COULD_NOT_WRITE_VERIFIED_CONTENTS_INTO_FILE;
    default:
      NOTREACHED();
      return em::ExtensionInstallReportLogEvent::
          SANDBOXED_UNPACKER_FAILURE_REASON_UNKNOWN;
  }
}

// Helper function to convert extensions::CrxInstallErrorDetail to the
// ExtensionInstallReportLogEvent::CrxInstallErrorDetail proto.
em::ExtensionInstallReportLogEvent_CrxInstallErrorDetail
ConvertCrxInstallErrorDetailToProto(
    extensions::CrxInstallErrorDetail error_detail) {
  using Error = extensions::CrxInstallErrorDetail;
  switch (error_detail) {
    case Error::NONE:
      return em::ExtensionInstallReportLogEvent::
          CRX_INSTALL_ERROR_DETAIL_UNKNOWN;
    case Error::CONVERT_USER_SCRIPT_TO_EXTENSION_FAILED:
      return em::ExtensionInstallReportLogEvent::
          CONVERT_USER_SCRIPT_TO_EXTENSION_FAILED;
    case Error::UNEXPECTED_ID:
      return em::ExtensionInstallReportLogEvent::UNEXPECTED_ID;
    case Error::UNEXPECTED_VERSION:
      return em::ExtensionInstallReportLogEvent::UNEXPECTED_VERSION;
    case Error::MISMATCHED_VERSION:
      return em::ExtensionInstallReportLogEvent::MISMATCHED_VERSION;
    case Error::MANIFEST_INVALID:
      return em::ExtensionInstallReportLogEvent::CRX_ERROR_MANIFEST_INVALID;
    case Error::INSTALL_NOT_ENABLED:
      return em::ExtensionInstallReportLogEvent::INSTALL_NOT_ENABLED;
    case Error::OFFSTORE_INSTALL_DISALLOWED:
      return em::ExtensionInstallReportLogEvent::OFFSTORE_INSTALL_DISALLOWED;
    case Error::INCORRECT_APP_CONTENT_TYPE:
      return em::ExtensionInstallReportLogEvent::INCORRECT_APP_CONTENT_TYPE;
    case Error::NOT_INSTALLED_FROM_GALLERY:
      return em::ExtensionInstallReportLogEvent::NOT_INSTALLED_FROM_GALLERY;
    case Error::INCORRECT_INSTALL_HOST:
      return em::ExtensionInstallReportLogEvent::INCORRECT_INSTALL_HOST;
    case Error::DEPENDENCY_NOT_SHARED_MODULE:
      return em::ExtensionInstallReportLogEvent::DEPENDENCY_NOT_SHARED_MODULE;
    case Error::DEPENDENCY_OLD_VERSION:
      return em::ExtensionInstallReportLogEvent::DEPENDENCY_OLD_VERSION;
    case Error::DEPENDENCY_NOT_ALLOWLISTED:
      return em::ExtensionInstallReportLogEvent::DEPENDENCY_NOT_ALLOWLISTED;
    case Error::UNSUPPORTED_REQUIREMENTS:
      return em::ExtensionInstallReportLogEvent::UNSUPPORTED_REQUIREMENTS;
    case Error::EXTENSION_IS_BLOCKLISTED:
      return em::ExtensionInstallReportLogEvent::EXTENSION_IS_BLOCKLISTED;
    case Error::DISALLOWED_BY_POLICY:
      return em::ExtensionInstallReportLogEvent::DISALLOWED_BY_POLICY;
    case Error::KIOSK_MODE_ONLY:
      return em::ExtensionInstallReportLogEvent::KIOSK_MODE_ONLY;
    case Error::OVERLAPPING_WEB_EXTENT:
      return em::ExtensionInstallReportLogEvent::OVERLAPPING_WEB_EXTENT;
    case Error::CANT_DOWNGRADE_VERSION:
      return em::ExtensionInstallReportLogEvent::CANT_DOWNGRADE_VERSION;
    case Error::MOVE_DIRECTORY_TO_PROFILE_FAILED:
      return em::ExtensionInstallReportLogEvent::
          MOVE_DIRECTORY_TO_PROFILE_FAILED;
    case Error::CANT_LOAD_EXTENSION:
      return em::ExtensionInstallReportLogEvent::CANT_LOAD_EXTENSION;
    case Error::USER_CANCELED:
      return em::ExtensionInstallReportLogEvent::USER_CANCELED;
    case Error::USER_ABORTED:
      return em::ExtensionInstallReportLogEvent::USER_ABORTED;
    case Error::UPDATE_NON_EXISTING_EXTENSION:
      return em::ExtensionInstallReportLogEvent::UPDATE_NON_EXISTING_EXTENSION;
    default:
      NOTREACHED();
      return em::ExtensionInstallReportLogEvent::
          CRX_INSTALL_ERROR_DETAIL_UNKNOWN;
  }
}

// Helper function to convert extensions::ManifestInvalidError to the
// ExtensionInstallReportLogEvent::ManifestInvalidError proto.
em::ExtensionInstallReportLogEvent_ManifestInvalidError
ConvertManifestInvalidErrorToProto(extensions::ManifestInvalidError error) {
  using ManifestError = extensions::ManifestInvalidError;
  switch (error) {
    case ManifestError::XML_PARSING_FAILED:
      return em::ExtensionInstallReportLogEvent::XML_PARSING_FAILED;
    case ManifestError::INVALID_XLMNS_ON_GUPDATE_TAG:
      return em::ExtensionInstallReportLogEvent::INVALID_XLMNS_ON_GUPDATE_TAG;
    case ManifestError::MISSING_GUPDATE_TAG:
      return em::ExtensionInstallReportLogEvent::MISSING_GUPDATE_TAG;
    case ManifestError::INVALID_PROTOCOL_ON_GUPDATE_TAG:
      return em::ExtensionInstallReportLogEvent::
          INVALID_PROTOCOL_ON_GUPDATE_TAG;
    case ManifestError::MISSING_APP_ID:
      return em::ExtensionInstallReportLogEvent::MISSING_APP_ID;
    case ManifestError::MISSING_UPDATE_CHECK_TAGS:
      return em::ExtensionInstallReportLogEvent::MISSING_UPDATE_CHECK_TAGS;
    case ManifestError::MULTIPLE_UPDATE_CHECK_TAGS:
      return em::ExtensionInstallReportLogEvent::MULTIPLE_UPDATE_CHECK_TAGS;
    case ManifestError::INVALID_PRODVERSION_MIN:
      return em::ExtensionInstallReportLogEvent::INVALID_PRODVERSION_MIN;
    case ManifestError::EMPTY_CODEBASE_URL:
      return em::ExtensionInstallReportLogEvent::EMPTY_CODEBASE_URL;
    case ManifestError::INVALID_CODEBASE_URL:
      return em::ExtensionInstallReportLogEvent::INVALID_CODEBASE_URL;
    case ManifestError::MISSING_VERSION_FOR_UPDATE_CHECK:
      return em::ExtensionInstallReportLogEvent::
          MISSING_VERSION_FOR_UPDATE_CHECK;
    case ManifestError::INVALID_VERSION:
      return em::ExtensionInstallReportLogEvent::INVALID_VERSION;
    case ManifestError::BAD_UPDATE_SPECIFICATION:
      return em::ExtensionInstallReportLogEvent::BAD_UPDATE_SPECIFICATION;
    case ManifestError::BAD_APP_STATUS:
      return em::ExtensionInstallReportLogEvent::BAD_APP_STATUS;
  }
}

void AddErrorCodesToFailureEvent(
    const extensions::InstallStageTracker::InstallationData& data,
    em::ExtensionInstallReportLogEvent* event) {
  if (data.response_code)
    event->set_fetch_error_code(data.response_code.value());
  else if (data.network_error_code)
    event->set_fetch_error_code(data.network_error_code.value());

  DCHECK(data.fetch_tries);
  event->set_fetch_tries(data.fetch_tries.value_or(0));
}

}  // namespace

using FailureReason = extensions::InstallStageTracker::FailureReason;

ExtensionInstallEventLogCollector::ExtensionInstallEventLogCollector(
    extensions::ExtensionRegistry* registry,
    Delegate* delegate,
    Profile* profile)
    : InstallEventLogCollectorBase(profile),
      registry_(registry),
      delegate_(delegate) {
  registry_observer_.Add(registry_);
  stage_tracker_observer_.Add(extensions::InstallStageTracker::Get(profile_));
}

ExtensionInstallEventLogCollector::~ExtensionInstallEventLogCollector() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

void ExtensionInstallEventLogCollector::OnLoginInternal() {
  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      CreateSessionChangeEvent(em::ExtensionInstallReportLogEvent::LOGIN);
    extensions::InstallStageTracker::UserInfo user_info =
        extensions::InstallStageTracker::GetUserInfo(profile_);
    if (user_info.is_user_present) {
      event->set_user_type(ConvertUserTypeToProto(user_info.user_type));
      event->set_is_new_user(user_info.is_new_user);
    }
  event->set_online(online_);
  delegate_->AddForAllExtensions(std::move(event));
}

void ExtensionInstallEventLogCollector::OnLogoutInternal() {
  delegate_->AddForAllExtensions(
      CreateSessionChangeEvent(em::ExtensionInstallReportLogEvent::LOGOUT));
}

void ExtensionInstallEventLogCollector::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  delegate_->AddForAllExtensions(
      CreateSessionChangeEvent(em::ExtensionInstallReportLogEvent::SUSPEND));
}

void ExtensionInstallEventLogCollector::SuspendDone(
    base::TimeDelta sleep_duration) {
  delegate_->AddForAllExtensions(
      CreateSessionChangeEvent(em::ExtensionInstallReportLogEvent::RESUME));
}

void ExtensionInstallEventLogCollector::OnConnectionStateChanged(
    network::mojom::ConnectionType type) {
  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_event_type(
      em::ExtensionInstallReportLogEvent::CONNECTIVITY_CHANGE);
  event->set_online(online_);
  delegate_->AddForAllExtensions(std::move(event));
}

void ExtensionInstallEventLogCollector::OnExtensionInstallationFailed(
    const extensions::ExtensionId& extension_id,
    FailureReason reason) {
  if (!delegate_->IsExtensionPending(extension_id))
    return;
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_event_type(
      em::ExtensionInstallReportLogEvent::INSTALLATION_FAILED);
  event->set_failure_reason(ConvertFailureReasonToProto(reason));
  extensions::InstallStageTracker* install_stage_tracker =
      extensions::InstallStageTracker::Get(profile_);
  extensions::InstallStageTracker::InstallationData data =
      install_stage_tracker->Get(extension_id);
  // Extension type is only reported if extension installation failed after the
  // unpacking stage.
  if (data.extension_type) {
    event->set_extension_type(enterprise_reporting::ConvertExtensionTypeToProto(
        data.extension_type.value()));
  }
  if (data.unpacker_failure_reason) {
    event->set_unpacker_failure_reason(ConvertUnpackerFailureReasonToProto(
        data.unpacker_failure_reason.value()));
  }
  // Manifest invalid error is only reported if the extension failed due to
  // failure reason MANIFEST_INVALID.
  if (data.manifest_invalid_error) {
    event->set_manifest_invalid_error(ConvertManifestInvalidErrorToProto(
        data.manifest_invalid_error.value()));
  }

  // Crx install error detail is only reported if extension installation failed
  // after the unpacking stage.
  if (data.install_error_detail) {
    event->set_crx_install_error_detail(
        ConvertCrxInstallErrorDetailToProto(data.install_error_detail.value()));
  }

  if (reason == FailureReason::CRX_FETCH_FAILED ||
      reason == FailureReason::MANIFEST_FETCH_FAILED) {
    AddErrorCodesToFailureEvent(data, event.get());
  }

  extensions::ForceInstalledTracker* force_installed_tracker =
      extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->force_installed_tracker();
  event->set_is_misconfiguration_failure(
      force_installed_tracker->IsMisconfiguration(data, extension_id));
  delegate_->Add(extension_id, true /* gather_disk_space_info */,
                 std::move(event));
  delegate_->OnExtensionInstallationFinished(extension_id);
}

void ExtensionInstallEventLogCollector::OnExtensionInstallationStageChanged(
    const extensions::ExtensionId& id,
    extensions::InstallStageTracker::Stage stage) {
  if (!delegate_->IsExtensionPending(id))
    return;
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_installation_stage(ConvertInstallationStageToProto(stage));
  delegate_->Add(id, true /* gather_disk_space_info */, std::move(event));
}

void ExtensionInstallEventLogCollector::OnExtensionDownloadingStageChanged(
    const extensions::ExtensionId& id,
    extensions::ExtensionDownloaderDelegate::Stage stage) {
  if (!delegate_->IsExtensionPending(id))
    return;
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_downloading_stage(ConvertDownloadingStageToProto(stage));
  delegate_->Add(id, true /* gather_disk_space_info */, std::move(event));
}

void ExtensionInstallEventLogCollector::OnExtensionInstallCreationStageChanged(
    const extensions::ExtensionId& id,
    extensions::InstallStageTracker::InstallCreationStage stage) {
  if (!delegate_->IsExtensionPending(id))
    return;
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_install_creation_stage(ConvertInstallCreationStageToProto(stage));
  delegate_->Add(id, false /* gather_disk_space_info */, std::move(event));
}

void ExtensionInstallEventLogCollector::OnExtensionDownloadCacheStatusRetrieved(
    const extensions::ExtensionId& id,
    extensions::ExtensionDownloaderDelegate::CacheStatus cache_status) {
  if (!delegate_->IsExtensionPending(id))
    return;
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_download_cache_status(
      ConvertDownloadCacheStatusToProto(cache_status));
  delegate_->Add(id, false /* gather_disk_space_info */, std::move(event));
}

void ExtensionInstallEventLogCollector::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (!delegate_->IsExtensionPending(extension->id()))
    return;
  AddSuccessEvent(extension);
}

void ExtensionInstallEventLogCollector::OnExtensionsRequested(
    const extensions::ExtensionIdSet& extension_ids) {
  for (const auto& extension_id : extension_ids) {
    const extensions::Extension* extension = registry_->GetExtensionById(
        extension_id, extensions::ExtensionRegistry::ENABLED);
    if (extension)
      AddSuccessEvent(extension);
  }
}

void ExtensionInstallEventLogCollector::AddSuccessEvent(
    const extensions::Extension* extension) {
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  event->set_extension_type(
      enterprise_reporting::ConvertExtensionTypeToProto(extension->GetType()));
  delegate_->Add(extension->id(), true /* gather_disk_space_info */,
                 std::move(event));
  delegate_->OnExtensionInstallationFinished(extension->id());
}

}  // namespace policy

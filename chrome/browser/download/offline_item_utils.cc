// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/offline_item_utils.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/download/android/download_utils.h"
#endif

using DownloadItem = download::DownloadItem;
using ContentId = offline_items_collection::ContentId;
using OfflineItem = offline_items_collection::OfflineItem;
using OfflineItemFilter = offline_items_collection::OfflineItemFilter;
using OfflineItemState = offline_items_collection::OfflineItemState;
using OfflineItemProgressUnit =
    offline_items_collection::OfflineItemProgressUnit;
using FailState = offline_items_collection::FailState;
using PendingState = offline_items_collection::PendingState;

namespace {

// The namespace for downloads.
const char kDownloadNamespace[] = "LEGACY_DOWNLOAD";

// The namespace for incognito downloads.
const char kDownloadIncognitoNamespace[] = "LEGACY_DOWNLOAD_INCOGNITO";

// Prefix that all download namespaces should start with.
const char kDownloadNamespacePrefix[] = "LEGACY_DOWNLOAD";

// The remaining time for a download item if it cannot be calculated.
constexpr int64_t kUnknownRemainingTime = -1;

std::optional<OfflineItemFilter> FilterForSpecialMimeTypes(
    const std::string& mime_type) {
  if (base::EqualsCaseInsensitiveASCII(mime_type, "application/ogg"))
    return OfflineItemFilter::FILTER_AUDIO;

  return std::nullopt;
}

OfflineItemFilter MimeTypeToOfflineItemFilter(const std::string& mime_type) {
  auto filter = FilterForSpecialMimeTypes(mime_type);
  if (filter.has_value())
    return filter.value();

  if (base::StartsWith(mime_type, "audio/", base::CompareCase::SENSITIVE)) {
    filter = OfflineItemFilter::FILTER_AUDIO;
  } else if (base::StartsWith(mime_type, "video/",
                              base::CompareCase::SENSITIVE)) {
    filter = OfflineItemFilter::FILTER_VIDEO;
  } else if (base::StartsWith(mime_type, "image/",
                              base::CompareCase::SENSITIVE)) {
    filter = OfflineItemFilter::FILTER_IMAGE;
  } else if (base::StartsWith(mime_type, "text/",
                              base::CompareCase::SENSITIVE)) {
    filter = OfflineItemFilter::FILTER_DOCUMENT;
  } else {
    filter = OfflineItemFilter::FILTER_OTHER;
  }

  return filter.value();
}

bool IsInterruptedDownloadAutoResumable(download::DownloadItem* item) {
  int auto_resumption_size_limit = 0;
#if BUILDFLAG(IS_ANDROID)
  auto_resumption_size_limit = DownloadUtils::GetAutoResumptionSizeLimit();
#endif

  return download::IsInterruptedDownloadAutoResumable(
      item, auto_resumption_size_limit);
}

}  // namespace

OfflineItem OfflineItemUtils::CreateOfflineItem(const std::string& name_space,
                                                DownloadItem* download_item) {
  auto* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download_item);
  bool off_the_record =
      browser_context ? browser_context->IsOffTheRecord() : false;

  OfflineItem item;
  item.id = ContentId(name_space, download_item->GetGuid());
  item.title = download_item->GetFileNameToReportUser().AsUTF8Unsafe();
  item.description = download_item->GetFileNameToReportUser().AsUTF8Unsafe();
  item.filter = MimeTypeToOfflineItemFilter(download_item->GetMimeType());
  item.is_transient = download_item->IsTransient();
  item.is_suggested = false;
  item.is_accelerated = download_item->IsParallelDownload();

  item.total_size_bytes = download_item->GetTotalBytes();
  item.externally_removed = download_item->GetFileExternallyRemoved();
  item.creation_time = download_item->GetStartTime();
  item.completion_time = download_item->GetEndTime();
  item.last_accessed_time = download_item->GetLastAccessTime();
  item.is_openable = download_item->CanOpenDownload();
  item.file_path = download_item->GetTargetFilePath();
  item.mime_type = download_item->GetMimeType();
#if BUILDFLAG(IS_ANDROID)
  item.mime_type = DownloadUtils::RemapGenericMimeType(
      item.mime_type, download_item->GetOriginalUrl(),
      download_item->GetTargetFilePath().value());
  if (off_the_record) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    item.otr_profile_id = profile->GetOTRProfileID().Serialize();
  }
#endif

  item.url = download_item->GetURL();
  item.original_url = download_item->GetOriginalUrl();
  item.is_off_the_record = off_the_record;
  item.referrer_url = download_item->GetReferrerUrl();
  item.has_user_gesture = download_item->HasUserGesture();

  item.is_resumable = download_item->CanResume();
  item.allow_metered = download_item->AllowMetered();
  item.received_bytes = download_item->GetReceivedBytes();
  item.is_dangerous = download_item->IsDangerous();

  base::TimeDelta time_delta;
  bool time_remaining_known = download_item->TimeRemaining(&time_delta);
  item.time_remaining_ms = time_remaining_known ? time_delta.InMilliseconds()
                                                : kUnknownRemainingTime;
  item.fail_state =
      ConvertDownloadInterruptReasonToFailState(download_item->GetLastReason());
  item.can_rename = download_item->GetState() == DownloadItem::COMPLETE;

  switch (download_item->GetState()) {
    case DownloadItem::IN_PROGRESS:
      item.state = download_item->IsPaused() ? OfflineItemState::PAUSED
                                             : OfflineItemState::IN_PROGRESS;
      break;
    case DownloadItem::COMPLETE:
      item.state = download_item->GetReceivedBytes() == 0
                       ? OfflineItemState::FAILED
                       : OfflineItemState::COMPLETE;
      break;
    case DownloadItem::CANCELLED:
      item.state = OfflineItemState::CANCELLED;
      break;
    case DownloadItem::INTERRUPTED: {
      bool is_auto_resumable =
          IsInterruptedDownloadAutoResumable(download_item);
      bool max_retry_limit_reached =
          download_item->GetAutoResumeCount() >=
          download::DownloadItemImpl::kMaxAutoResumeAttempts;

      if (download_item->IsDone()) {
        item.state = OfflineItemState::FAILED;
      } else if (download_item->IsPaused() || max_retry_limit_reached) {
        item.state = OfflineItemState::PAUSED;
      } else if (is_auto_resumable) {
        item.state = OfflineItemState::PENDING;
      } else {
        item.state = OfflineItemState::INTERRUPTED;
      }
    } break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  // TODO(crbug.com/40582846): Set pending_state correctly.
  item.pending_state = item.state == OfflineItemState::PENDING
                           ? PendingState::PENDING_NETWORK
                           : PendingState::NOT_PENDING;
  item.progress.value = download_item->GetReceivedBytes();
  if (download_item->PercentComplete() != -1)
    item.progress.max = download_item->GetTotalBytes();

  item.progress.unit = OfflineItemProgressUnit::BYTES;

  return item;
}

offline_items_collection::ContentId OfflineItemUtils::GetContentIdForDownload(
    download::DownloadItem* download) {
  bool off_the_record =
      content::DownloadItemUtils::GetBrowserContext(download)->IsOffTheRecord();
  return ContentId(OfflineItemUtils::GetDownloadNamespacePrefix(off_the_record),
                   download->GetGuid());
}

std::string OfflineItemUtils::GetDownloadNamespacePrefix(
    bool is_off_the_record) {
  return is_off_the_record ? kDownloadIncognitoNamespace : kDownloadNamespace;
}

bool OfflineItemUtils::IsDownload(const ContentId& id) {
  return id.name_space.find(kDownloadNamespacePrefix) != std::string::npos;
}

// static
FailState OfflineItemUtils::ConvertDownloadInterruptReasonToFailState(
    download::DownloadInterruptReason reason) {
  switch (reason) {
    case download::DOWNLOAD_INTERRUPT_REASON_NONE:
      return offline_items_collection::FailState::NO_FAILURE;
#define INTERRUPT_REASON(name, value)              \
  case download::DOWNLOAD_INTERRUPT_REASON_##name: \
    return offline_items_collection::FailState::name;
#include "components/download/public/common/download_interrupt_reason_values.h"
#undef INTERRUPT_REASON
  }
}

// static
download::DownloadInterruptReason
OfflineItemUtils::ConvertFailStateToDownloadInterruptReason(
    offline_items_collection::FailState fail_state) {
  switch (fail_state) {
    case offline_items_collection::FailState::NO_FAILURE:
    // These two enum values are not converted from download interrupted reason,
    // maps them to none error.
    case offline_items_collection::FailState::CANNOT_DOWNLOAD:
    case offline_items_collection::FailState::NETWORK_INSTABILITY:
      return download::DOWNLOAD_INTERRUPT_REASON_NONE;
#define INTERRUPT_REASON(name, value)             \
  case offline_items_collection::FailState::name: \
    return download::DOWNLOAD_INTERRUPT_REASON_##name;
#include "components/download/public/common/download_interrupt_reason_values.h"
#undef INTERRUPT_REASON
  }
}

// static
std::u16string OfflineItemUtils::GetFailStateMessage(FailState fail_state) {
  int string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS;

  switch (fail_state) {
    case FailState::FILE_ACCESS_DENIED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_ACCESS_DENIED;
      break;
    case FailState::FILE_NO_SPACE:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_DISK_FULL;
      break;
    case FailState::FILE_NAME_TOO_LONG:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_PATH_TOO_LONG;
      break;
    case FailState::FILE_TOO_LARGE:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_FILE_TOO_LARGE;
      break;
    case FailState::FILE_VIRUS_INFECTED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_VIRUS;
      break;
    case FailState::FILE_TRANSIENT_ERROR:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_TEMPORARY_PROBLEM;
      break;
    case FailState::FILE_BLOCKED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_BLOCKED;
      break;
    case FailState::FILE_SECURITY_CHECK_FAILED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_SECURITY_CHECK_FAILED;
      break;
    case FailState::FILE_TOO_SHORT:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_FILE_TOO_SHORT;
      break;
    case FailState::FILE_SAME_AS_SOURCE:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_FILE_SAME_AS_SOURCE;
      break;
    case FailState::NETWORK_INVALID_REQUEST:
      [[fallthrough]];
    case FailState::NETWORK_FAILED:
      [[fallthrough]];
    case FailState::NETWORK_INSTABILITY:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_NETWORK_ERROR;
      break;
    case FailState::NETWORK_TIMEOUT:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_NETWORK_TIMEOUT;
      break;
    case FailState::NETWORK_DISCONNECTED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_NETWORK_DISCONNECTED;
      break;
    case FailState::NETWORK_SERVER_DOWN:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_SERVER_DOWN;
      break;
    case FailState::SERVER_FAILED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_SERVER_PROBLEM;
      break;
    case FailState::SERVER_BAD_CONTENT:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_NO_FILE;
      break;
    case FailState::USER_CANCELED:
      string_id = IDS_DOWNLOAD_STATUS_CANCELLED;
      break;
    case FailState::USER_SHUTDOWN:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_SHUTDOWN;
      break;
    case FailState::CRASH:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_CRASH;
      break;
    case FailState::SERVER_UNAUTHORIZED:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_UNAUTHORIZED;
      break;
    case FailState::SERVER_CERT_PROBLEM:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_SERVER_CERT_PROBLEM;
      break;
    case FailState::SERVER_FORBIDDEN:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_FORBIDDEN;
      break;
    case FailState::SERVER_UNREACHABLE:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_UNREACHABLE;
      break;
    case FailState::SERVER_CONTENT_LENGTH_MISMATCH:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS_CONTENT_LENGTH_MISMATCH;
      break;

    case FailState::NO_FAILURE:
      // We reach here if the received bytes is zero. Ideally, we should have a
      // separate FailState outside of download interrupt reasons, and pass the
      // bytes info to every function that invokes this.
      [[fallthrough]];
    case FailState::CANNOT_DOWNLOAD:
      [[fallthrough]];
    case FailState::SERVER_NO_RANGE:
      [[fallthrough]];
    case FailState::SERVER_CROSS_ORIGIN_REDIRECT:
      [[fallthrough]];
    case FailState::FILE_FAILED:
      [[fallthrough]];
    case FailState::FILE_HASH_MISMATCH:
      string_id = IDS_DOWNLOAD_INTERRUPTED_STATUS;
  }

  return l10n_util::GetStringUTF16(string_id);
}

// static
RenameResult OfflineItemUtils::ConvertDownloadRenameResultToRenameResult(
    DownloadRenameResult download_rename_result) {
  assert(static_cast<int>(DownloadRenameResult::RESULT_MAX) ==
         static_cast<int>(RenameResult::kMaxValue));
  switch (download_rename_result) {
    case DownloadRenameResult::SUCCESS:
      return RenameResult::SUCCESS;
    case DownloadRenameResult::FAILURE_NAME_CONFLICT:
      return RenameResult::FAILURE_NAME_CONFLICT;
    case DownloadRenameResult::FAILURE_NAME_TOO_LONG:
      return RenameResult::FAILURE_NAME_TOO_LONG;
    case DownloadRenameResult::FAILURE_NAME_INVALID:
      return RenameResult::FAILURE_NAME_INVALID;
    case DownloadRenameResult::FAILURE_UNAVAILABLE:
      return RenameResult::FAILURE_UNAVAILABLE;
    case DownloadRenameResult::FAILURE_UNKNOWN:
      return RenameResult::FAILURE_UNKNOWN;
  }
}

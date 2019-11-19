// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/last_download_finder.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/history/core/browser/download_constants.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "crypto/sha2.h"
#include "extensions/buildflags/buildflags.h"

namespace safe_browsing {

namespace {

// The following functions are overloaded for the two object types that
// represent the metadata for a download: history::DownloadRow and
// ClientIncidentReport_DownloadDetails. These are used by the template
// functions that follow.

// Returns the end time of a download represented by a DownloadRow.
int64_t GetEndTime(const history::DownloadRow& row) {
  return row.end_time.ToJavaTime();
}

// Returns the end time of a download represented by a DownloadDetails.
int64_t GetEndTime(const ClientIncidentReport_DownloadDetails& details) {
  return details.download_time_msec();
}

bool IsBinaryDownloadForCurrentOS(
    ClientDownloadRequest::DownloadType download_type) {
  // Whenever a new DownloadType is introduced, the following set of conditions
  // should also be updated so that the IsBinaryDownloadForCurrentOS() will
  // return true for that DownloadType as appropriate.
  static_assert(ClientDownloadRequest::DownloadType_MAX ==
                    ClientDownloadRequest::DOCUMENT,
                "Update logic below");

// Platform-specific types are relevant only for their own platforms.
#if defined(OS_MACOSX)
  if (download_type == ClientDownloadRequest::MAC_EXECUTABLE ||
      download_type == ClientDownloadRequest::MAC_ARCHIVE_FAILED_PARSING)
    return true;
#elif defined(OS_ANDROID)
  if (download_type == ClientDownloadRequest::ANDROID_APK)
    return true;
#endif

// Extensions are supported where enabled.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (download_type == ClientDownloadRequest::CHROME_EXTENSION)
    return true;
#endif

  if (download_type == ClientDownloadRequest::ZIPPED_EXECUTABLE ||
      download_type == ClientDownloadRequest::ZIPPED_ARCHIVE ||
      download_type == ClientDownloadRequest::INVALID_ZIP ||
      download_type == ClientDownloadRequest::RAR_COMPRESSED_EXECUTABLE ||
      download_type == ClientDownloadRequest::RAR_COMPRESSED_ARCHIVE ||
      download_type == ClientDownloadRequest::INVALID_RAR ||
      download_type == ClientDownloadRequest::ARCHIVE ||
      download_type == ClientDownloadRequest::PPAPI_SAVE_REQUEST) {
    return true;
  }

  // The default return value of download_type_util::GetDownloadType is
  // ClientDownloadRequest::WIN_EXECUTABLE.
  return download_type == ClientDownloadRequest::WIN_EXECUTABLE;
}

// Returns true if a download represented by a DownloadRow is a binary file for
// the current OS.
bool IsBinaryDownload(const history::DownloadRow& row) {
  // TODO(grt): Peek into archives to see if they contain binaries;
  // http://crbug.com/386915.
  FileTypePolicies* policies = FileTypePolicies::GetInstance();
  return (policies->IsCheckedBinaryFile(row.target_path) &&
          !policies->IsArchiveFile(row.target_path) &&
          IsBinaryDownloadForCurrentOS(
              download_type_util::GetDownloadType(row.target_path)));
}

// Returns true if a download represented by a DownloadRow is not a binary file.
bool IsNonBinaryDownload(const history::DownloadRow& row) {
  return !FileTypePolicies::GetInstance()->IsCheckedBinaryFile(
      row.target_path);
}

// Returns true if a download represented by a DownloadDetails is binary file
// for the current OS.
bool IsBinaryDownload(const ClientIncidentReport_DownloadDetails& details) {
  // DownloadDetails are only generated for binary downloads.
  return IsBinaryDownloadForCurrentOS(details.download().download_type());
}

// Returns true if a download represented by a DownloadRow has been opened.
bool HasBeenOpened(const history::DownloadRow& row) {
  return row.opened;
}

// Returns true if a download represented by a DownloadDetails has been opened.
bool HasBeenOpened(const ClientIncidentReport_DownloadDetails& details) {
  return details.has_open_time_msec() && details.open_time_msec();
}

// Returns true if |first| is more recent than |second|, preferring opened over
// non-opened for downloads that completed at the same time (extraordinarily
// unlikely). Only files that look like some kind of executable are considered.
template <class A, class B>
bool IsMoreInterestingBinaryThan(const A& first, const B& second) {
  if (GetEndTime(first) < GetEndTime(second) || !IsBinaryDownload(first))
    return false;
  return (GetEndTime(first) != GetEndTime(second) ||
          (HasBeenOpened(first) && !HasBeenOpened(second)));
}

// Returns true if |first| is more recent than |second|, preferring opened over
// non-opened for downloads that completed at the same time (extraordinarily
// unlikely). Only files that do not look like an executable are considered.
bool IsMoreInterestingNonBinaryThan(const history::DownloadRow& first,
                                    const history::DownloadRow& second) {
  if (GetEndTime(first) < GetEndTime(second) || !IsNonBinaryDownload(first))
    return false;
  return (GetEndTime(first) != GetEndTime(second) ||
          (HasBeenOpened(first) && !HasBeenOpened(second)));
}

// Returns a pointer to the most interesting completed download in |downloads|.
const history::DownloadRow* FindMostInteresting(
    const std::vector<history::DownloadRow>& downloads,
    bool is_binary) {
  const history::DownloadRow* most_recent_row = nullptr;
  for (const auto& row : downloads) {
    // Ignore incomplete downloads.
    if (row.state != history::DownloadState::COMPLETE)
      continue;

    if (!most_recent_row ||
        (is_binary ? IsMoreInterestingBinaryThan(row, *most_recent_row)
                   : IsMoreInterestingNonBinaryThan(row, *most_recent_row))) {
      most_recent_row = &row;
    }
  }
  return most_recent_row;
}

// Returns true if |candidate| is more interesting than whichever of |details|
// or |best_row| is present.
template <class D>
bool IsMostInterestingBinary(
    const D& candidate,
    const ClientIncidentReport_DownloadDetails* details,
    const history::DownloadRow& best_row) {
  return details ? IsMoreInterestingBinaryThan(candidate, *details)
                 : IsMoreInterestingBinaryThan(candidate, best_row);
}

// Populates the |details| protobuf with information pertaining to |download|.
void PopulateDetailsFromRow(const history::DownloadRow& download,
                            ClientIncidentReport_DownloadDetails* details) {
  ClientDownloadRequest* download_request = details->mutable_download();
  download_request->set_url(download.url_chain.back().spec());
  // digests is a required field, so force it to exist.
  // TODO(grt): Include digests in reports; http://crbug.com/389123.
  ignore_result(download_request->mutable_digests());
  download_request->set_length(download.received_bytes);
  for (size_t i = 0; i < download.url_chain.size(); ++i) {
    const GURL& url = download.url_chain[i];
    ClientDownloadRequest_Resource* resource =
        download_request->add_resources();
    resource->set_url(url.spec());
    if (i != download.url_chain.size() - 1) {  // An intermediate redirect.
      resource->set_type(ClientDownloadRequest::DOWNLOAD_REDIRECT);
    } else {  // The final download URL.
      resource->set_type(ClientDownloadRequest::DOWNLOAD_URL);
      if (!download.referrer_url.is_empty())
        resource->set_referrer(download.referrer_url.spec());
    }
  }
  download_request->set_file_basename(
      download.target_path.BaseName().AsUTF8Unsafe());
  download_request->set_download_type(
      download_type_util::GetDownloadType(download.target_path));
  std::string pref_locale = g_browser_process->local_state()->GetString(
      language::prefs::kApplicationLocale);
  language::ConvertToActualUILocale(&pref_locale);
  download_request->set_locale(pref_locale);

  details->set_download_time_msec(download.end_time.ToJavaTime());
  // Opened time is unknown for now, so use the download time if the file was
  // opened in Chrome.
  if (download.opened)
    details->set_open_time_msec(download.end_time.ToJavaTime());
}

// Populates the |details| protobuf with information pertaining to the
// (non-binary) |download|.
void PopulateNonBinaryDetailsFromRow(
    const history::DownloadRow& download,
    ClientIncidentReport_NonBinaryDownloadDetails* details) {
  details->set_file_type(
      base::FilePath(FileTypePolicies::GetFileExtension(download.target_path))
          .AsUTF8Unsafe());
  details->set_length(download.received_bytes);
  if (download.url_chain.back().has_host())
    details->set_host(download.url_chain.back().host());
  details->set_url_spec_sha256(
      crypto::SHA256HashString(download.url_chain.back().spec()));
}

}  // namespace

LastDownloadFinder::~LastDownloadFinder() {
  g_browser_process->profile_manager()->RemoveObserver(this);
}

// static
std::unique_ptr<LastDownloadFinder> LastDownloadFinder::Create(
    const DownloadDetailsGetter& download_details_getter,
    const LastDownloadCallback& callback) {
  std::unique_ptr<LastDownloadFinder> finder(base::WrapUnique(
      new LastDownloadFinder(download_details_getter, callback)));
  // Return NULL if there is no work to do.
  if (finder->profile_states_.empty())
    return std::unique_ptr<LastDownloadFinder>();
  return finder;
}

LastDownloadFinder::LastDownloadFinder() = default;

LastDownloadFinder::LastDownloadFinder(
    const DownloadDetailsGetter& download_details_getter,
    const LastDownloadCallback& callback)
    : download_details_getter_(download_details_getter), callback_(callback) {
  // Begin the search for all existing profiles.
  for (auto* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    SearchInProfile(profile);
  }

  // Also search on new profiles when they are added.
  g_browser_process->profile_manager()->AddObserver(this);
}

void LastDownloadFinder::SearchInProfile(Profile* profile) {
  // Do not look in OTR profiles or in profiles that do not participate in
  // safe browsing extended reporting.
  if (!IncidentReportingService::IsEnabledForProfile(profile))
    return;

  // Exit early if already processing this profile. This could happen if, for
  // example, OnProfileAdded is called after construction while waiting for
  // OnHistoryServiceLoaded.
  if (profile_states_.count(profile))
    return;

  // Initiate a metadata search. As with IncidentReportingService, it's assumed
  // that all passed profiles will outlive |this|.
  profile_states_[profile] = WAITING_FOR_METADATA;
  download_details_getter_.Run(profile,
                               base::Bind(&LastDownloadFinder::OnMetadataQuery,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          profile));
}

void LastDownloadFinder::OnMetadataQuery(
    Profile* profile,
    std::unique_ptr<ClientIncidentReport_DownloadDetails> details) {
  auto iter = profile_states_.find(profile);
  // Early-exit if the search for this profile was abandoned.
  if (iter == profile_states_.end())
    return;

  if (details) {
    if (IsMostInterestingBinary(*details, details_.get(),
                                most_recent_binary_row_)) {
      details_ = std::move(details);
      most_recent_binary_row_.end_time = base::Time();
    }
    iter->second = WAITING_FOR_NON_BINARY_HISTORY;
  } else {
    iter->second = WAITING_FOR_HISTORY;
  }

  // Initiate a history search
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  // No history service is returned for profiles that do not save history.
  if (!history_service) {
    RemoveProfileAndReportIfDone(iter);
    return;
  }
  if (history_service->BackendLoaded()) {
    history_service->QueryDownloads(
        base::BindOnce(&LastDownloadFinder::OnDownloadQuery,
                       weak_ptr_factory_.GetWeakPtr(), profile));
  } else {
    // else wait until history is loaded.
    history_service_observer_.Add(history_service);
  }
}

void LastDownloadFinder::OnDownloadQuery(
    Profile* profile,
    std::vector<history::DownloadRow> downloads) {
  // Early-exit if the history search for this profile was abandoned.
  auto iter = profile_states_.find(profile);
  if (iter == profile_states_.end())
    return;

  // Don't overwrite the download from metadata if it came from this profile.
  if (iter->second == WAITING_FOR_HISTORY) {
    // Find the most recent from this profile and use it if it's better than
    // anything else found so far.
    const history::DownloadRow* profile_best_binary =
        FindMostInteresting(downloads, true);
    if (profile_best_binary &&
        IsMostInterestingBinary(*profile_best_binary, details_.get(),
                                most_recent_binary_row_)) {
      details_.reset();
      most_recent_binary_row_ = *profile_best_binary;
    }
  }

  const history::DownloadRow* profile_best_non_binary =
      FindMostInteresting(downloads, false);
  if (profile_best_non_binary &&
      IsMoreInterestingNonBinaryThan(*profile_best_non_binary,
                                     most_recent_non_binary_row_)) {
    most_recent_non_binary_row_ = *profile_best_non_binary;
  }

  RemoveProfileAndReportIfDone(iter);
}

void LastDownloadFinder::RemoveProfileAndReportIfDone(
    std::map<Profile*, ProfileWaitState>::iterator iter) {
  DCHECK(iter != profile_states_.end());
  profile_states_.erase(iter);

  // Finish processing if all results are in.
  if (profile_states_.empty())
    ReportResults();
  // Do not touch this LastDownloadFinder after reporting results.
}

void LastDownloadFinder::ReportResults() {
  DCHECK(profile_states_.empty());

  std::unique_ptr<ClientIncidentReport_DownloadDetails> binary_details =
      nullptr;
  std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>
      non_binary_details = nullptr;

  if (details_) {
    binary_details.reset(new ClientIncidentReport_DownloadDetails(*details_));
  } else if (!most_recent_binary_row_.end_time.is_null()) {
    binary_details.reset(new ClientIncidentReport_DownloadDetails());
    PopulateDetailsFromRow(most_recent_binary_row_, binary_details.get());
  }

  if (!most_recent_non_binary_row_.end_time.is_null()) {
    non_binary_details.reset(
        new ClientIncidentReport_NonBinaryDownloadDetails());
    PopulateNonBinaryDetailsFromRow(most_recent_non_binary_row_,
                                    non_binary_details.get());
  }

  callback_.Run(std::move(binary_details), std::move(non_binary_details));
  // Do not touch this LastDownloadFinder after running the callback, since it
  // may have been deleted.
}

void LastDownloadFinder::OnProfileAdded(Profile* profile) {
  SearchInProfile(profile);
}

void LastDownloadFinder::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  for (const auto& pair : profile_states_) {
    history::HistoryService* hs = HistoryServiceFactory::GetForProfileIfExists(
        pair.first, ServiceAccessType::EXPLICIT_ACCESS);
    if (hs == history_service) {
      // Start the query in the history service if the finder was waiting for
      // the service to load.
      if (pair.second == WAITING_FOR_HISTORY ||
          pair.second == WAITING_FOR_NON_BINARY_HISTORY) {
        history_service->QueryDownloads(
            base::BindOnce(&LastDownloadFinder::OnDownloadQuery,
                           weak_ptr_factory_.GetWeakPtr(), pair.first));
      }
      return;
    }
  }
}

void LastDownloadFinder::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observer_.Remove(history_service);
}

}  // namespace safe_browsing

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/last_download_finder.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
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
#include "components/history/core/browser/download_constants.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
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
  return row.end_time.InMillisecondsSinceUnixEpoch();
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
                    ClientDownloadRequest::INVALID_SEVEN_ZIP,
                "Update logic below");

// Platform-specific types are relevant only for their own platforms.
#if BUILDFLAG(IS_MAC)
  if (download_type == ClientDownloadRequest::MAC_EXECUTABLE ||
      download_type == ClientDownloadRequest::MAC_ARCHIVE_FAILED_PARSING)
    return true;
#elif BUILDFLAG(IS_ANDROID)
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
      download_type == ClientDownloadRequest::PPAPI_SAVE_REQUEST ||
      download_type == ClientDownloadRequest::SEVEN_ZIP_COMPRESSED_EXECUTABLE ||
      download_type == ClientDownloadRequest::SEVEN_ZIP_COMPRESSED_ARCHIVE ||
      download_type == ClientDownloadRequest::INVALID_SEVEN_ZIP) {
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
  std::ignore = download_request->mutable_digests();
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

  details->set_download_time_msec(
      download.end_time.InMillisecondsSinceUnixEpoch());
  // Opened time is unknown for now, so use the download time if the file was
  // opened in Chrome.
  if (download.opened)
    details->set_open_time_msec(
        download.end_time.InMillisecondsSinceUnixEpoch());
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
    DownloadDetailsGetter download_details_getter,
    LastDownloadCallback callback) {
  std::unique_ptr<LastDownloadFinder> finder(
      base::WrapUnique(new LastDownloadFinder(
          std::move(download_details_getter), std::move(callback))));
  // Return NULL if there is no work to do.
  if (finder->pending_profiles_.empty()) {
    return nullptr;
  }
  return finder;
}

LastDownloadFinder::LastDownloadFinder() = default;

LastDownloadFinder::LastDownloadFinder(
    DownloadDetailsGetter download_details_getter,
    LastDownloadCallback callback)
    : download_details_getter_(std::move(download_details_getter)),
      callback_(std::move(callback)) {
  // Begin the search for all existing profiles.
  for (auto* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    SearchInProfile(profile);
  }

  // Also search on new profiles when they are added.
  g_browser_process->profile_manager()->AddObserver(this);
}

LastDownloadFinder::PendingProfileData::PendingProfileData(
    LastDownloadFinder* finder,
    Profile* profile,
    State state)
    : state(state), observation(finder) {
  observation.Observe(profile);
}

LastDownloadFinder::PendingProfileData::~PendingProfileData() = default;

// static
LastDownloadFinder::ProfileKey LastDownloadFinder::KeyForProfile(
    Profile* profile) {
  return ProfileKey(reinterpret_cast<std::uintptr_t>(profile));
}

void LastDownloadFinder::SearchInProfile(Profile* profile) {
  // Do not look in OTR profiles or in profiles that do not participate in
  // safe browsing extended reporting.
  if (!IncidentReportingService::IsEnabledForProfile(profile))
    return;

  // Try to initiate a metadata search.
  ProfileKey profile_key = KeyForProfile(profile);
  auto [iter, inserted] = pending_profiles_.try_emplace(
      profile_key, this, profile, PendingProfileData::WAITING_FOR_METADATA);

  // If the profile was already being processed, do nothing.
  if (!inserted) {
    return;
  }
  download_details_getter_.Run(
      profile, base::BindOnce(&LastDownloadFinder::OnMetadataQuery,
                              weak_ptr_factory_.GetWeakPtr(), profile_key));
}

void LastDownloadFinder::OnMetadataQuery(
    ProfileKey profile_key,
    std::unique_ptr<ClientIncidentReport_DownloadDetails> details) {
  auto iter = pending_profiles_.find(profile_key);
  // Early-exit if the search for this profile was abandoned.
  if (iter == pending_profiles_.end()) {
    return;
  }

  if (details) {
    if (IsMostInterestingBinary(*details, details_.get(),
                                most_recent_binary_row_)) {
      details_ = std::move(details);
      most_recent_binary_row_.end_time = base::Time();
    }
    iter->second.state = PendingProfileData::WAITING_FOR_NON_BINARY_HISTORY;
  } else {
    iter->second.state = PendingProfileData::WAITING_FOR_HISTORY;
  }

  // Initiate a history search
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(iter->second.profile(),
                                           ServiceAccessType::IMPLICIT_ACCESS);
  // No history service is returned for profiles that do not save history.
  if (!history_service) {
    RemoveProfileAndReportIfDone(iter);
    return;
  }
  if (history_service->BackendLoaded()) {
    history_service->QueryDownloads(
        base::BindOnce(&LastDownloadFinder::OnDownloadQuery,
                       weak_ptr_factory_.GetWeakPtr(), profile_key));
  } else {
    // else wait until history is loaded.
    history_service_observations_.AddObservation(history_service);
  }
}

void LastDownloadFinder::OnDownloadQuery(
    ProfileKey profile_key,
    std::vector<history::DownloadRow> downloads) {
  // Early-exit if the history search for this profile was abandoned.
  auto iter = pending_profiles_.find(profile_key);
  if (iter == pending_profiles_.end()) {
    return;
  }

  // Don't overwrite the download from metadata if it came from this profile.
  if (iter->second.state == PendingProfileData::WAITING_FOR_HISTORY) {
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
    PendingProfilesMap::iterator iter) {
  CHECK(iter != pending_profiles_.end());
  pending_profiles_.erase(iter);

  // Finish processing if all results are in.
  if (pending_profiles_.empty()) {
    ReportResults();
  }
  // Do not touch this LastDownloadFinder after reporting results.
}

void LastDownloadFinder::ReportResults() {
  CHECK(pending_profiles_.empty());

  std::unique_ptr<ClientIncidentReport_DownloadDetails> binary_details;
  std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>
      non_binary_details;

  if (details_) {
    binary_details =
        std::make_unique<ClientIncidentReport_DownloadDetails>(*details_);
  } else if (!most_recent_binary_row_.end_time.is_null()) {
    binary_details = std::make_unique<ClientIncidentReport_DownloadDetails>();
    PopulateDetailsFromRow(most_recent_binary_row_, binary_details.get());
  }

  if (!most_recent_non_binary_row_.end_time.is_null()) {
    non_binary_details =
        std::make_unique<ClientIncidentReport_NonBinaryDownloadDetails>();
    PopulateNonBinaryDetailsFromRow(most_recent_non_binary_row_,
                                    non_binary_details.get());
  }

  std::move(callback_).Run(std::move(binary_details),
                           std::move(non_binary_details));
  // Do not touch this LastDownloadFinder after running the callback, since it
  // may have been deleted.
}

void LastDownloadFinder::OnProfileAdded(Profile* profile) {
  SearchInProfile(profile);
}

void LastDownloadFinder::OnProfileWillBeDestroyed(Profile* profile) {
  // If a Profile is about to be destroyed while we are observing it, the
  // `profile` must be present in the map of pending queries.
  auto iter = pending_profiles_.find(KeyForProfile(profile));
  CHECK(iter != pending_profiles_.end());
  RemoveProfileAndReportIfDone(iter);
}

void LastDownloadFinder::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  for (auto& [profile_key, profile_data] : pending_profiles_) {
    history::HistoryService* hs = HistoryServiceFactory::GetForProfileIfExists(
        profile_data.profile(), ServiceAccessType::EXPLICIT_ACCESS);
    if (hs == history_service) {
      // Start the query in the history service if the finder was waiting for
      // the service to load.
      if (profile_data.state == PendingProfileData::WAITING_FOR_HISTORY ||
          profile_data.state ==
              PendingProfileData::WAITING_FOR_NON_BINARY_HISTORY) {
        history_service->QueryDownloads(
            base::BindOnce(&LastDownloadFinder::OnDownloadQuery,
                           weak_ptr_factory_.GetWeakPtr(), profile_key));
      }
      return;
    }
  }
}

void LastDownloadFinder::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observations_.RemoveObservation(history_service);
}

}  // namespace safe_browsing

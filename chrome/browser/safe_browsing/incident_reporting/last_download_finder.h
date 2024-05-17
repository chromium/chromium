// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_LAST_DOWNLOAD_FINDER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_LAST_DOWNLOAD_FINDER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/safe_browsing/incident_reporting/download_metadata_manager.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"

class Profile;

namespace safe_browsing {

class ClientIncidentReport_DownloadDetails;
class ClientIncidentReport_NonBinaryDownloadDetails;

// Finds the most recent executable download and non-executable download by
// any on-the-record profile with history that participates in safe browsing
// extended reporting.
class LastDownloadFinder : public ProfileManagerObserver,
                           public ProfileObserver,
                           public history::HistoryServiceObserver {
 public:
  typedef base::RepeatingCallback<void(
      content::BrowserContext* context,
      DownloadMetadataManager::GetDownloadDetailsCallback)>
      DownloadDetailsGetter;

  // The type of a callback run by the finder upon completion. Each argument is
  // a protobuf containing details of the respective download that was found,
  // or an empty pointer if none was found.
  typedef base::OnceCallback<void(
      std::unique_ptr<ClientIncidentReport_DownloadDetails>,
      std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>)>
      LastDownloadCallback;

  LastDownloadFinder(const LastDownloadFinder&) = delete;
  LastDownloadFinder& operator=(const LastDownloadFinder&) = delete;

  ~LastDownloadFinder() override;

  // Initiates an asynchronous search for the most recent downloads. |callback|
  // will be run when the search is complete. The returned instance can be
  // deleted to terminate the search, in which case |callback| is not invoked.
  // Returns NULL without running |callback| if there are no eligible profiles
  // to search.
  static std::unique_ptr<LastDownloadFinder> Create(
      DownloadDetailsGetter download_details_getter,
      LastDownloadCallback callback);

 protected:
  // Protected constructor so that unit tests can create a fake finder.
  LastDownloadFinder();

 private:
  // Holds state for a Profile for which a query is currently pending. The State
  // describes what we are waiting for. The observation is a mechanism to ensure
  // that we do not attempt to process the results of a search for a Profile
  // that is no longer valid. This guarantees that a PendingProfileData exists
  // iff the Profile is alive and we are waiting on a query for it.
  struct PendingProfileData {
    enum State {
      WAITING_FOR_METADATA,
      WAITING_FOR_HISTORY,
      WAITING_FOR_NON_BINARY_HISTORY,
    };

    // Makes the `finder` start observing `profile`.
    PendingProfileData(LastDownloadFinder* finder,
                       Profile* profile,
                       State state);

    ~PendingProfileData();

    Profile* profile() { return observation.GetSource(); }

    State state;
    base::ScopedObservation<Profile, LastDownloadFinder> observation;
  };

  // We identify Profiles by their addresses, in the form of a uintptr_t value,
  // which is derived from a Profile* but is not to be dereferenced.
  using ProfileKey = base::StrongAlias<class ProfileKeyTag, uintptr_t>;

  using PendingProfilesMap = std::map<ProfileKey, PendingProfileData>;

  LastDownloadFinder(DownloadDetailsGetter download_details_getter,
                     LastDownloadCallback callback);

  // Returns the address of the Profile in a safe form to be used as a map key.
  static inline ProfileKey KeyForProfile(Profile* profile);

  // Adds |profile| to the set of profiles to be searched if it is an
  // on-the-record profile with history that participates in safe browsing
  // extended reporting. A search for metadata is initiated immediately.
  void SearchInProfile(Profile* profile);

  // DownloadMetadataManager::GetDownloadDetailsCallback. If |details| are
  // provided, retrieves them if they are the most relevant results. Otherwise
  // begins a search in history. Reports results if there are no more pending
  // queries.
  void OnMetadataQuery(
      ProfileKey profile_key,
      std::unique_ptr<ClientIncidentReport_DownloadDetails> details);

  // HistoryService::DownloadQueryCallback. Retrieves the most recent completed
  // executable download from |downloads| and reports results if there are no
  // more pending queries.
  void OnDownloadQuery(ProfileKey profile_key,
                       std::vector<history::DownloadRow> downloads);

  // Severs ties with the Profile whose state is pointed at by `iter` within
  // `pending_profiles_`, either after a query finishes or to abandon an ongoing
  // query. Also reports results if there are no more pending queries.
  void RemoveProfileAndReportIfDone(PendingProfilesMap::iterator iter);

  // Invokes the caller-supplied callback with the download found.
  void ReportResults();

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // history::HistoryServiceObserver:
  void OnHistoryServiceLoaded(history::HistoryService* service) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // Caller-supplied callback to make an asynchronous request for a profile's
  // persistent download details.
  DownloadDetailsGetter download_details_getter_;

  // Caller-supplied callback to be invoked when the most recent download is
  // found.
  LastDownloadCallback callback_;

  // A mapping of profiles for which a query is pending to their respective
  // states. Items are removed from here when a query finishes, or when we
  // abandon a query.
  PendingProfilesMap pending_profiles_;

  // The most interesting download details retrieved from download metadata.
  std::unique_ptr<ClientIncidentReport_DownloadDetails> details_;

  // The most recent download, updated progressively as query results arrive.
  history::DownloadRow most_recent_binary_row_;

  // The most recent non-binary download, updated progressively as query results
  // arrive.
  history::DownloadRow most_recent_non_binary_row_;

  // HistoryServiceObserver
  base::ScopedMultiSourceObservation<history::HistoryService,
                                     history::HistoryServiceObserver>
      history_service_observations_{this};

  // A factory for asynchronous operations on profiles' HistoryService.
  base::WeakPtrFactory<LastDownloadFinder> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_LAST_DOWNLOAD_FINDER_H_

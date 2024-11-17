// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_DOWNLOAD_METADATA_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_DOWNLOAD_METADATA_MANAGER_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/download/public/common/all_download_event_notifier.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class BrowserContext;
class DownloadManager;
}

namespace download {
class DownloadItem;
class SimpleDownloadManagerCoordinator;
}

namespace safe_browsing {

class ClientDownloadRequest;
class ClientIncidentReport_DownloadDetails;

// A browser-wide object that manages the persistent state of metadata
// pertaining to a download.
class DownloadMetadataManager
    : public download::AllDownloadEventNotifier::Observer {
 public:
  // A callback run when the results of a call to GetDownloadDetails are ready.
  // The supplied parameter may be null, indicating that there are no persisted
  // details for the |browser_context| passed to GetDownloadDetails.
  typedef base::OnceCallback<void(
      std::unique_ptr<ClientIncidentReport_DownloadDetails>)>
      GetDownloadDetailsCallback;

  DownloadMetadataManager();

  DownloadMetadataManager(const DownloadMetadataManager&) = delete;
  DownloadMetadataManager& operator=(const DownloadMetadataManager&) = delete;

  ~DownloadMetadataManager() override;

  // Adds the coordinator for `download_manager` to the set observed by the
  // metadata manager.
  void AddDownloadManager(content::DownloadManager* download_manager);

  // Sets |request| as the relevant metadata to persist for |download| upon
  // completion. |request| will be persisted when the download completes, or
  // discarded if the download is cancelled.
  virtual void SetRequest(download::DownloadItem* download,
                          const ClientDownloadRequest* request);

  // Gets the persisted DownloadDetails for |browser_context|. |callback| will
  // be run immediately if the data is available. Otherwise, it will be run
  // later on the caller's thread.
  virtual void GetDownloadDetails(content::BrowserContext* browser_context,
                                  GetDownloadDetailsCallback callback);

  // download::AllDownloadEventNotifier::Observer:
  void OnManagerGoingDown(
      download::SimpleDownloadManagerCoordinator* coordinator) override;
  void OnDownloadUpdated(
      download::SimpleDownloadManagerCoordinator* coordinator,
      download::DownloadItem* download) override;
  void OnDownloadOpened(download::SimpleDownloadManagerCoordinator* coordinator,
                        download::DownloadItem* download) override;
  void OnDownloadRemoved(
      download::SimpleDownloadManagerCoordinator* coordinator,
      download::DownloadItem* download) override;

 protected:
  // Returns the coordinator for a given BrowserContext. Virtual for tests.
  virtual download::SimpleDownloadManagerCoordinator*
  GetCoordinatorForBrowserContext(content::BrowserContext* context);

 private:
  class ManagerContext;

  // A mapping of DownloadManagerCoordinators to their corresponding contexts.
  typedef std::map<download::SimpleDownloadManagerCoordinator*,
                   raw_ptr<ManagerContext, CtnExperimental>>
      ManagerToContextMap;

  // A task runner to which IO tasks are posted.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Contexts for each coordinator that has been added and has not yet "gone
  // down".
  ManagerToContextMap contexts_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_DOWNLOAD_METADATA_MANAGER_H_

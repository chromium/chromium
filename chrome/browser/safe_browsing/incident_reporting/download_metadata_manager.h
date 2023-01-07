// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_DOWNLOAD_METADATA_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_DOWNLOAD_METADATA_MANAGER_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/download_manager.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class BrowserContext;
}

namespace download {
class DownloadItem;
}

namespace safe_browsing {

class ClientDownloadRequest;
class ClientIncidentReport_DownloadDetails;

// A browser-wide object that manages the persistent state of metadata
// pertaining to a download.
class DownloadMetadataManager : public content::DownloadManager::Observer {
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

  // Adds |download_manager| to the set observed by the metadata manager.
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

 protected:
  // Returns the DownloadManager for a given BrowserContext. Virtual for tests.
  virtual content::DownloadManager* GetDownloadManagerForBrowserContext(
      content::BrowserContext* context);

  // content::DownloadManager:Observer methods.
  void OnDownloadCreated(content::DownloadManager* download_manager,
                         download::DownloadItem* item) override;
  void ManagerGoingDown(content::DownloadManager* download_manager) override;

 private:
  class ManagerContext;

  // A mapping of DownloadManagers to their corresponding contexts.
  typedef std::map<content::DownloadManager*, ManagerContext*>
      ManagerToContextMap;

  // A task runner to which IO tasks are posted.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Contexts for each DownloadManager that has been added and has not yet
  // "gone down".
  ManagerToContextMap contexts_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_DOWNLOAD_METADATA_MANAGER_H_

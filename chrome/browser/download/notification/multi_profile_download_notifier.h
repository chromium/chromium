// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_NOTIFICATION_MULTI_PROFILE_DOWNLOAD_NOTIFIER_H_
#define CHROME_BROWSER_DOWNLOAD_NOTIFICATION_MULTI_PROFILE_DOWNLOAD_NOTIFIER_H_

#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/download/content/public/all_download_item_notifier.h"

// `MultiProfileDownloadNotifier` observes the `DownloadItem`s created on an
// arbitrary number of `Profile`s. No `Profile`s are observed by default; a
// client must specify `Profile`s to be observed using `AddProfile()`. Once a
// `Profile` is being observed, any off-the-record profile it has spawned or
// spawns later will also be observed unless it is filtered out by
// `ShouldObserveProfile()`.

// Example Usage:
// class DownloadsDelegate : public MultiProfileDownloadNotifier::Client {
//  public:
//   DownloadsDelegate(Profile* profile) {
//     downloads_notifier_.AddProfile(profile);
//   }
//
// void OnManagerInitialized(content::DownloadManager* manager) override { ... }
// void OnManagerGoingDown(content::DownloadManager* manager) override { ... }
// void OnDownloadCreated(content::DownloadManager* manager,
//                        download::DownloadItem* item) override { ... }
// void OnDownloadUpdated(content::DownloadManager* manager,
//                        download::DownloadItem* item) override { ... }
// void OnDownloadDestroyed(content::DownloadManager* manager,
//                          download::DownloadItem* item) override { ... }
// bool ShouldObserveProfile(Profile* profile) override { ... }
//
//  private:
//   MultiProfileDownloadNotifier downloads_notifier_{this};
// };

namespace content {
class DownloadManager;
}  // namespace content

namespace download {
class DownloadItem;
}  // namespace download

class MultiProfileDownloadNotifier
    : public ProfileObserver,
      public download::AllDownloadItemNotifier::Observer {
 public:
  class Client {
   public:
    Client() = default;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    virtual ~Client() = default;

    // Each method is called with the relevant download manager for use in a
    // client's auxiliary data structures, if applicable.
    virtual void OnManagerInitialized(content::DownloadManager* manager) {}
    virtual void OnManagerGoingDown(content::DownloadManager* manager) {}
    virtual void OnDownloadCreated(content::DownloadManager* manager,
                                   download::DownloadItem* item) {}
    virtual void OnDownloadUpdated(content::DownloadManager* manager,
                                   download::DownloadItem* item) {}
    virtual void OnDownloadDestroyed(content::DownloadManager* manager,
                                     download::DownloadItem* item) {}

    // Specifies whether the client wants to be notified for downloads created
    // on `profile`. This function is called before observing any profile,
    // regardless of whether `profile` was added automatically as the
    // off-the-record child of an observed profile or the client added it
    // explicitly using `AddProfile()`.
    virtual bool ShouldObserveProfile(Profile* profile);
  };

  // `wait_for_manager_initialization` controls whether `client` will be
  // notified about downloads belonging to `Profile`s with uninitialized
  // `DownloadManager`s.
  MultiProfileDownloadNotifier(Client* client,
                               bool wait_for_manager_initialization);
  MultiProfileDownloadNotifier(const MultiProfileDownloadNotifier&) = delete;
  MultiProfileDownloadNotifier& operator=(const MultiProfileDownloadNotifier&) =
      delete;
  ~MultiProfileDownloadNotifier() override;

  // Creates a download notifier for the download manager associated with
  // `profile` if one does not already exist.
  void AddProfile(Profile* profile);

  // Returns all downloads for all observed profiles.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>
  GetAllDownloads();

  // Searches all download notifiers for an observed `DownloadItem` matching
  // `guid`. Returns the item if found or nullptr if none exists. Note that this
  // function will return a matching download item even if it belongs to an
  // uninitialized manager and `wait_for_manager_initialization_` is true.
  download::DownloadItem* GetDownloadByGuid(const std::string& guid);

 private:
  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // download::AllDownloadItemNotifier::Observer:
  void OnManagerInitialized(content::DownloadManager* manager) override;
  void OnManagerGoingDown(content::DownloadManager* manager) override;
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadDestroyed(content::DownloadManager* manager,
                           download::DownloadItem* item) override;

  // Helper function that makes sure a `DownloadManager` is initialized if
  // `client_` requires it to be.
  bool IsManagerReady(content::DownloadManager* manager);

  const raw_ptr<MultiProfileDownloadNotifier::Client> client_;

  const bool wait_for_manager_initialization_;

  std::set<std::unique_ptr<download::AllDownloadItemNotifier>,
           base::UniquePtrComparator>
      download_notifiers_;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profile_observer_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_NOTIFICATION_MULTI_PROFILE_DOWNLOAD_NOTIFIER_H_

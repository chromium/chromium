// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_DOWNLOAD_CONTROLLER_CLIENT_LACROS_H_
#define CHROME_BROWSER_LACROS_DOWNLOAD_CONTROLLER_CLIENT_LACROS_H_

#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"

namespace download {
class DownloadItem;
}  // namespace download

// This class receives and forwards download events to Ash. It can only be
// used on the main thread. In the near future, it will also be the receiver for
// calls to pause, cancel, and resume downloads from ash-chrome, hence the name.
class DownloadControllerClientLacros : public ProfileManagerObserver,
                                       public ProfileObserver {
 public:
  DownloadControllerClientLacros();
  DownloadControllerClientLacros(const DownloadControllerClientLacros&) =
      delete;
  DownloadControllerClientLacros& operator=(
      const DownloadControllerClientLacros&) = delete;
  ~DownloadControllerClientLacros() override;

 private:
  class ObservableDownloadManager;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  void OnManagerGoingDown(ObservableDownloadManager* observable_manager);
  void OnDownloadCreated(download::DownloadItem* item);
  void OnDownloadUpdated(download::DownloadItem* item);
  void OnDownloadDestroyed(download::DownloadItem* item);

  std::set<std::unique_ptr<ObservableDownloadManager>,
           base::UniquePtrComparator>
      observable_download_managers_;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profile_observer_{this};
};

#endif  // CHROME_BROWSER_LACROS_DOWNLOAD_CONTROLLER_CLIENT_LACROS_H_

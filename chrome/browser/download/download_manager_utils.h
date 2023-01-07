// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_UTILS_H_

#include <memory>

#include "base/functional/callback_forward.h"

class Profile;
class ProfileKey;

namespace download {
class InProgressDownloadManager;
}

class DownloadManagerUtils {
 public:
  DownloadManagerUtils(const DownloadManagerUtils&) = delete;
  DownloadManagerUtils& operator=(const DownloadManagerUtils&) = delete;

  // Creates an InProgressDownloadManager from a profile.
  static std::unique_ptr<download::InProgressDownloadManager>
  RetrieveInProgressDownloadManager(Profile* profile);

  // Initializes the SimpleDownloadManager that is associated with |key| whenver
  // possible.
  static void InitializeSimpleDownloadManager(ProfileKey* key);

  // Creates an InProgressDownloadManager for a particular |key| if it doesn't
  // exist and return the pointer.
  static download::InProgressDownloadManager* GetInProgressDownloadManager(
      ProfileKey* key);

  // Registers a `callback` to be run during subsequent invocations of
  // `RetrieveInProgressDownloadManager()`, providing an opportunity to cache
  // a pointer to the in progress download manager being released.
  static void SetRetrieveInProgressDownloadManagerCallbackForTesting(
      base::RepeatingCallback<void(download::InProgressDownloadManager*)>
          callback);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_UTILS_H_

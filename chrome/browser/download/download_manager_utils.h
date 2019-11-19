// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_UTILS_H_

#include "base/macros.h"

class Profile;
class ProfileKey;

namespace download {
class InProgressDownloadManager;
}

class DownloadManagerUtils {
 public:
  // Creates an InProgressDownloadManager from a profile.
  static download::InProgressDownloadManager* RetrieveInProgressDownloadManager(
      Profile* profile);

  // Initializes the SimpleDownloadManager that is associated with |key| whenver
  // possible.
  static void InitializeSimpleDownloadManager(ProfileKey* key);

  // Creates an InProgressDownloadManager for a particular |key| if it doesn't
  // exist and return the pointer.
  static download::InProgressDownloadManager* GetInProgressDownloadManager(
      ProfileKey* key);

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadManagerUtils);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_UTILS_H_

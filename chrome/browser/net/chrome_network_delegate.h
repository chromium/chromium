// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_

#include "base/files/file_path.h"

// TODO(jam): rename this class.
class ChromeNetworkDelegate {
 public:
  ChromeNetworkDelegate(const ChromeNetworkDelegate&) = delete;
  ChromeNetworkDelegate& operator=(const ChromeNetworkDelegate&) = delete;

  // Returns true if access to |path| is allowed. |profile_path| is used to
  // locate certain paths on Chrome OS. See set_profile_path() for details.
  static bool IsAccessAllowed(const base::FilePath& path,
                              const base::FilePath& profile_path);

  // Like above, but also takes |path|'s absolute path in |absolute_path| to
  // further validate access.
  static bool IsAccessAllowed(const base::FilePath& path,
                              const base::FilePath& absolute_path,
                              const base::FilePath& profile_path);

  // Enables access to all files for testing purposes. This function is used
  // to bypass the access control for file: scheme. Calling this function
  // with false brings back the original (production) behaviors.
  static void EnableAccessToAllFilesForTesting(bool enabled);
};

#endif  // CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_

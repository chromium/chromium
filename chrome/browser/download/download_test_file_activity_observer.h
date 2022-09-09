// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TEST_FILE_ACTIVITY_OBSERVER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TEST_FILE_ACTIVITY_OBSERVER_H_

#include "base/memory/weak_ptr.h"

class Profile;

// Observes and overrides file chooser dialog and open activity for a profile.
// By default, once attached to a profile, this class overrides the default file
// related activity by replacing the ChromeDownloadManagerDelegate associated
// with |profile|.
// NOTE: Again, this overrides the ChromeDownloadManagerDelegate for |profile|.
class DownloadTestFileActivityObserver {
 public:
  // Attaches to |profile|. By default file chooser dialogs will be disabled
  // once attached. Call EnableFileChooser() to re-enable.
  explicit DownloadTestFileActivityObserver(Profile* profile);
  ~DownloadTestFileActivityObserver();

  // Sets whether the file chooser dialog is enabled. If |enable| is false, any
  // attempt to display a file chooser dialog will cause the download to be
  // canceled. Otherwise, attempting to display a file chooser dialog will
  // result in the download continuing with the suggested path.
  void EnableFileChooser(bool enable);

  // Returns true if a file chooser dialog was displayed since the last time
  // this method was called.
  bool TestAndResetDidShowFileChooser();

 private:
  class MockDownloadManagerDelegate;

  base::WeakPtr<MockDownloadManagerDelegate> test_delegate_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TEST_FILE_ACTIVITY_OBSERVER_H_

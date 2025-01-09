// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_SHARING_DATA_SHARING_NAVIGATION_UTILS_H_
#define CHROME_BROWSER_DATA_SHARING_DATA_SHARING_NAVIGATION_UTILS_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"

namespace content {
class WebContents;
}

namespace data_sharing {

// This class keeps track of the latest user interaction on a WebContents to
// handle the case that gesture is invalidated on client redirect.
class DataSharingNavigationUtils {
 public:
  // Returns the singleton instance of this class.
  static DataSharingNavigationUtils* GetInstance();

  // Updates the latest user interaction of the WebContents.
  void UpdateLastUserInteractionTime(content::WebContents* web_contents);

  // Returns whether the last user interaction on WebContents has expired.
  bool IsLastUserInteractionExpired(content::WebContents* web_contents);

  // Helper method for testing.
  void set_clock_for_testing(base::Clock* clock) { clock_ = clock; }

 private:
  friend base::NoDestructor<DataSharingNavigationUtils>;

  DataSharingNavigationUtils();
  ~DataSharingNavigationUtils();

  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();

  std::map<uintptr_t, base::Time> recent_interractions_map_;
};

}  // namespace data_sharing

#endif  // CHROME_BROWSER_DATA_SHARING_DATA_SHARING_NAVIGATION_UTILS_H_

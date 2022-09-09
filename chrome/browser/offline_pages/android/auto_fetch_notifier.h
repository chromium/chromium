// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_AUTO_FETCH_NOTIFIER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_AUTO_FETCH_NOTIFIER_H_

#include <string>


namespace offline_pages {

// Functions for calling into AutoFetchNotifier.java.

// Functions to show, remove, and update the in-progress notification.
// See AutoFetchNotifier.java.
void ShowAutoFetchInProgressNotification(int in_progress_count);
void UpdateAutoFetchInProgressNotificationCountIfShowing(int in_progress_count);

// Returns true if the user canceled the in-progress notification while
// Chrome wasn't running. If true, the active requests should be canceled,
// and then |AutoFetchCancellationComplete()| should be called.
bool AutoFetchInProgressNotificationCanceled();
void AutoFetchCancellationComplete();

// Triggers the auto-fetch complete notification. See AutoFetchNotifier.java.
void ShowAutoFetchCompleteNotification(const std::u16string& page_title,
                                       const std::string& original_url,
                                       const std::string& final_url,
                                       int android_tab_id,
                                       int64_t offline_id);

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_AUTO_FETCH_NOTIFIER_H_

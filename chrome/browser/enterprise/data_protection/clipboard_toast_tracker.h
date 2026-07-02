// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_CLIPBOARD_TOAST_TRACKER_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_CLIPBOARD_TOAST_TRACKER_H_

#include "base/containers/flat_set.h"
#include "base/supports_user_data.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace enterprise_data_protection {

// Types of informational copy restriction toasts that can be throttled by
// `ClipboardToastTracker`.
enum class CopyToastType {
  // Toast shown when copying content that is strictly monitored/audited by
  // organization policies.
  kAudit,
  // Toast shown when copying content that is permitted only within authorized
  // managed Chrome browser boundaries.
  kKeptInManagedChrome,
};

// Tracks which copy informational toasts have already been displayed during the
// active browser session for a given `Profile`.
//
// Using `base::SupportsUserData avoids the heavy boilerplate of a KeyedService,
// providing a lightweight way to manage the lifecycle of this simple,
// dependency-free tracker.
class ClipboardToastTracker : public base::SupportsUserData::Data {
 public:
  ClipboardToastTracker();
  ~ClipboardToastTracker() override;

  // Retrieves (or lazily creates) the unique tracker instance attached to
  // `profile`. Returns nullptr if `profile` is null.
  static ClipboardToastTracker* GetForProfile(Profile* profile);

  // Returns true if the specified `type` of toast has not yet been shown during
  // the current session for this profile.
  bool ShouldShowToast(CopyToastType type);

  // Records that the specified `type` of toast has been displayed so that
  // subsequent checks for this type return false.
  void RecordToastShown(CopyToastType type);

 private:
  // Set of toast types that have been shown to the user in this session.
  base::flat_set<CopyToastType> shown_toasts_;
};

// Surfaces an informational copy toast corresponding to `type` in the active
// window hosting `web_contents`, provided it has not already been shown for
// `profile` during this session.
void MaybeShowCopyToast(Profile* profile,
                        content::WebContents* web_contents,
                        CopyToastType type);

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_CLIPBOARD_TOAST_TRACKER_H_

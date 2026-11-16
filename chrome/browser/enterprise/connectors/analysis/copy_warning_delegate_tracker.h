// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_COPY_WARNING_DELEGATE_TRACKER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_COPY_WARNING_DELEGATE_TRACKER_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace enterprise_connectors {

class ContentAnalysisDelegate;

// A WebContentsUserData tracker used to associate the active copy warning
// scan delegate with its corresponding WebContents without global state.
// This class is used to integrate with Chrome's Toast logic and to find the
// right delegate to bypass the copy warning. See
// chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h
// Toasts are initialized on session start way before the delegate is created,
// so we need a way to track the delegate and WebContents to bypass the copy
// warning when the user clicks the bypass button on the toast later.
class CopyWarningDelegateTracker
    : public content::WebContentsUserData<CopyWarningDelegateTracker> {
 public:
  // The user's answer to a copy warning.
  struct Answer {
    // When the warning showed. Only copies made by then get the answer.
    base::TimeTicks warning_time;
    bool bypassed = false;
    std::optional<std::u16string> justification;
  };

  // Sets the delegate for the given WebContents. If a delegate is already
  // set, it will be deleted.
  static void SetDelegate(content::WebContents* web_contents,
                          ContentAnalysisDelegate* delegate);

  // Bypasses the copy warning for the given WebContents and clears the
  // delegate.
  static void BypassAndClear(content::WebContents* web_contents);

  // Cancels the copy warning for the given WebContents and clears the
  // delegate.
  static void CancelAndClear(content::WebContents* web_contents);

  // Clears the delegate from the tracker for the given WebContents if it
  // matches the given delegate.
  // This is used when the delegate is destroyed to avoid the tracker holding
  // onto a deleted delegate.
  static void ClearIfMatches(content::WebContents* web_contents,
                             ContentAnalysisDelegate* delegate);

  // Returns the delegate whose copy warning is waiting on the user in
  // `web_contents`, or null if there's none or `web_contents` is null.
  static ContentAnalysisDelegate* GetDelegate(
      content::WebContents* web_contents);

  // Returns the delegate whose copy warning the user is writing a
  // justification for in `web_contents`, or null if there's none or
  // `web_contents` is null. The justification dialog owns it.
  static ContentAnalysisDelegate* GetJustifyingDelegate(
      content::WebContents* web_contents);

  // Keeps the answer to the last copy warning in `web_contents`, so formats of
  // that copy whose verdict comes in later get it too. Does nothing if
  // `web_contents` is null.
  static void SetAnswer(content::WebContents* web_contents, Answer answer);

  // Returns the answer kept for `web_contents`, or nullopt if there's none or
  // `web_contents` is null.
  static std::optional<Answer> GetAnswer(content::WebContents* web_contents);

  CopyWarningDelegateTracker(const CopyWarningDelegateTracker&) = delete;
  CopyWarningDelegateTracker& operator=(const CopyWarningDelegateTracker&) =
      delete;

  ~CopyWarningDelegateTracker() override;

 private:
  friend class content::WebContentsUserData<CopyWarningDelegateTracker>;
  explicit CopyWarningDelegateTracker(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtr<ContentAnalysisDelegate> delegate_ = nullptr;
  base::WeakPtr<ContentAnalysisDelegate> justifying_delegate_;
  std::optional<Answer> answer_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_COPY_WARNING_DELEGATE_TRACKER_H_

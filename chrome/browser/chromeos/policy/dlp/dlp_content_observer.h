// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_OBSERVER_H_

#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"

namespace content {
class WebContents;
}  // namespace content

namespace policy {

// Interface for a class observing changed in Data Leak Prevention restricted
// content per WebContents object.
class DlpContentObserver {
 public:
  // Returns proper implementation of the interface. Never returns nullptr.
  static DlpContentObserver* Get();

  // Returns if an instance of the class is currently active.
  static bool HasInstance();

  virtual ~DlpContentObserver() = default;

  // Being called when confidentiality state changes for |web_contents|, e.g.
  // because of navigation.
  virtual void OnConfidentialityChanged(
      content::WebContents* web_contents,
      const DlpContentRestrictionSet& restriction_set) = 0;

  // Called when |web_contents| is about to be destroyed.
  virtual void OnWebContentsDestroyed(content::WebContents* web_contents) = 0;

  // Called when |web_contents| becomes visible or not.
  virtual void OnVisibilityChanged(content::WebContents* web_contents) = 0;

 private:
  friend class ScopedDlpContentObserverForTesting;

  static void SetDlpContentObserverForTesting(
      DlpContentObserver* dlp_content_observer);
  static void ResetDlpContentObserverForTesting();
};

// Helper class to call SetDlpContentObserverForTesting and
// ResetDlpContentObserverForTesting automatically.
// The caller (test) should manage `test_dlp_content_observer` lifetime.
// This class does not own it.
class ScopedDlpContentObserverForTesting {
 public:
  explicit ScopedDlpContentObserverForTesting(
      DlpContentObserver* test_dlp_content_observer);
  ~ScopedDlpContentObserverForTesting();
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_OBSERVER_H_

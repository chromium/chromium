// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_dialog.h"
#include "url/gurl.h"

namespace content {
struct DesktopMediaID;
struct WebContentsMediaCaptureId;
class WebContents;
}  // namespace content

namespace policy {

class DlpReportingManager;

class DlpWarnNotifier;

// System-wide class that tracks the set of currently known confidential
// WebContents and whether any of them are currently visible.
// If any confidential WebContents is visible, the corresponding restrictions
// will be enforced according to the current enterprise policy.
class DlpContentManager : public DlpContentObserver {
 public:
  DlpContentManager(const DlpContentManager&) = delete;
  DlpContentManager& operator=(const DlpContentManager&) = delete;

  // Returns platform-specific implementation of the class. Never returns
  // nullptr.
  static DlpContentManager* Get();

  // Returns which restrictions are applied to the |web_contents| according to
  // the policy.
  DlpContentRestrictionSet GetConfidentialRestrictions(
      content::WebContents* web_contents) const;

  // Checks whether printing of |web_contents| is restricted or not advised.
  // Depending on the result, calls |callback| and passes an indicator whether
  // to proceed or not.
  void CheckPrintingRestriction(content::WebContents* web_contents,
                                OnDlpRestrictionCheckedCallback callback);

  // Checks whether screen sharing of content from the |media_id| source with
  // application |application_name| is restricted or not advised. Depending on
  // the result, calls |callback| and passes an indicator whether to proceed or
  // not.
  virtual void CheckScreenShareRestriction(
      const content::DesktopMediaID& media_id,
      const std::u16string& application_title,
      OnDlpRestrictionCheckedCallback callback) = 0;

 protected:
  void SetIsScreenShareWarningModeEnabledForTesting(bool is_enabled);

  // Structure that relates a list of confidential contents to the
  // corresponding restriction level.
  struct ConfidentialContentsInfo {
    RestrictionLevelAndUrl restriction_info;
    DlpConfidentialContents confidential_contents;
  };

  DlpContentManager();
  ~DlpContentManager() override;

  // Reports events of type DlpPolicyEvent::Mode::WARN_PROCEED to
  // `reporting_manager`.
  static void ReportWarningProceededEvent(
      const GURL& url,
      DlpRulesManager::Restriction restriction,
      DlpReportingManager* reporting_manager);

  // Helper method to create a callback with ReportWarningProceededEvent
  // function.
  static bool MaybeReportWarningProceededEvent(
      GURL url,
      DlpRulesManager::Restriction restriction,
      DlpReportingManager* reporting_manager,
      bool should_proceed);

  // Initializing to be called separately to make testing possible.
  virtual void Init();

  // DlpContentObserver overrides:
  void OnConfidentialityChanged(
      content::WebContents* web_contents,
      const DlpContentRestrictionSet& restriction_set) override;
  void OnWebContentsDestroyed(content::WebContents* web_contents) override;

  // Helper to remove |web_contents| from the confidential set.
  virtual void RemoveFromConfidential(content::WebContents* web_contents);

  // Returns which level and url of printing restriction is currently enforced
  // for |web_contents|.
  RestrictionLevelAndUrl GetPrintingRestrictionInfo(
      content::WebContents* web_contents) const;

  // Returns confidential info for screen share of a single WebContents with
  // |web_contents_id|.
  ConfidentialContentsInfo GetScreenShareConfidentialContentsInfoForWebContents(
      const content::WebContentsMediaCaptureId& web_contents_id) const;

  // Applies retrieved restrictions in |info| to screens share attempt from
  // app |application_title| and calls the |callback| with a result.
  void ProcessScreenShareRestriction(const std::u16string& application_title,
                                     ConfidentialContentsInfo info,
                                     OnDlpRestrictionCheckedCallback callback);

  // Called back from warning dialogs. Passes along the user's response,
  // reflected in the value of |should_proceed| along to |callback| which
  // handles continuing or cancelling the action based on this response. In case
  // that |should_proceed| is true, also saves the |confidential_contents| that
  // were allowed by the user for |restriction| to avoid future warnings.
  void OnDlpWarnDialogReply(
      const DlpConfidentialContents& confidential_contents,
      DlpRulesManager::Restriction restriction,
      OnDlpRestrictionCheckedCallback callback,
      bool should_proceed);

  // Reports events if required by the |restriction_info| and
  // `reporting_manager` is configured.
  void MaybeReportEvent(const RestrictionLevelAndUrl& restriction_info,
                        DlpRulesManager::Restriction restriction);

  // Reports warning events if `reporting_manager` is configured.
  void ReportWarningEvent(const GURL& url,
                          DlpRulesManager::Restriction restriction);

  // Removes all elemxents of |contents| that the user has recently already
  // acknowledged the warning for.
  void RemoveAllowedContents(DlpConfidentialContents& contents,
                             DlpRulesManager::Restriction restriction);

  // Map from currently known confidential WebContents to the restrictions.
  base::flat_map<content::WebContents*, DlpContentRestrictionSet>
      confidential_web_contents_;

  // Keeps track of the contents for which the user allowed the action after
  // being shown a warning for each type of restriction.
  // TODO(crbug.com/1264803): Change to DlpConfidentialContentsCache
  DlpConfidentialContentsCache user_allowed_contents_cache_;

  raw_ptr<DlpReportingManager> reporting_manager_{nullptr};

  std::unique_ptr<DlpWarnNotifier> warn_notifier_;

  // TODO(https://crbug.com/1278733): Remove this flag
  bool is_screen_share_warning_mode_enabled_ = false;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_

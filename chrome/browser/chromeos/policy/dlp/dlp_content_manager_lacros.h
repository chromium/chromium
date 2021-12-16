// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_LACROS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_LACROS_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "content/public/browser/desktop_media_id.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

namespace policy {

// LaCros-wide class that tracks the set of currently known confidential
// WebContents and whether any of them are currently visible.
class DlpContentManagerLacros : public DlpContentManager,
                                public aura::WindowObserver {
 public:
  // Creates the instance if not yet created.
  // There will always be a single instance created on the first access.
  static DlpContentManagerLacros* Get();

  // Checks whether screen sharing of content from the |media_id| source with
  // application |application_name| is restricted or not advised. Depending on
  // the result, calls |callback| and passes an indicator whether to proceed or
  // not.
  void CheckScreenShareRestriction(
      const content::DesktopMediaID& media_id,
      const std::u16string& application_title,
      OnDlpRestrictionCheckedCallback callback) override;

 private:
  DlpContentManagerLacros();
  ~DlpContentManagerLacros() override;

  // DlpContentObserver overrides:
  void OnConfidentialityChanged(
      content::WebContents* web_contents,
      const DlpContentRestrictionSet& restriction_set) override;
  void OnWebContentsDestroyed(content::WebContents* web_contents) override;
  void OnVisibilityChanged(content::WebContents* web_contents) override;

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override;

  // Updates |confidential_windows_| entry for |window| and notifies Ash if
  // needed.
  void UpdateRestrictions(aura::Window* window);

  // Tracks set of known confidential WebContents* for each Window*.
  base::flat_map<aura::Window*, base::flat_set<content::WebContents*>>
      window_webcontents_;

  // Tracks current restrictions applied to Window* based on visible
  // WebContents* belonging to Window*.
  base::flat_map<aura::Window*, DlpContentRestrictionSet> confidential_windows_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_LACROS_H_

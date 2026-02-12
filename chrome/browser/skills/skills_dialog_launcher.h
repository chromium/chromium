// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_DIALOG_LAUNCHER_H_
#define CHROME_BROWSER_SKILLS_SKILLS_DIALOG_LAUNCHER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/skills/public/skill.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace tabs {
class TabInterface;
}

namespace skills {

// A self-deleting helper that waits for a tab to finish its initial navigation
// before launching the Skills dialog. This prevents the dialog from being
// immediately closed by the navigation.
class SkillsDialogLauncher
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SkillsDialogLauncher> {
 public:
  using SkillResultCallback = base::OnceCallback<void(bool)>;

  // Helper that creates the launcher, which manages its own lifetime.
  static void CreateForTab(tabs::TabInterface* tab,
                           Skill skill,
                           SkillResultCallback callback);

  ~SkillsDialogLauncher() override;

 private:
  friend class content::WebContentsUserData<SkillsDialogLauncher>;

  // Helper to trigger the launch of the dialog.
  static void TriggerDialog(tabs::TabInterface* tab,
                            Skill skill,
                            SkillResultCallback callback);

  SkillsDialogLauncher(content::WebContents* contents,
                       tabs::TabInterface* tab,
                       Skill skill,
                       SkillResultCallback callback);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void Show();

  // Weak reference to the target tab; may become null if the tab is closed
  // during navigation.
  base::WeakPtr<tabs::TabInterface> tab_;
  // The user skill to be passed to the Skills dialog upon successful load.
  Skill skill_;
  // Callback to signal success or failure to the caller.
  SkillResultCallback callback_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_DIALOG_LAUNCHER_H_

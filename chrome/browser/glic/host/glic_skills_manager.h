// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_SKILLS_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_SKILLS_MANAGER_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "components/skills/public/skill.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace tabs {
class TabInterface;
}

class Profile;
namespace skills {
struct Skill;
}  // namespace skills
namespace glic {

// This is a host-scoped object that is responsible for sending skills to the
// web client.
class GlicSkillsManager {
 public:
  GlicSkillsManager() = default;
  virtual ~GlicSkillsManager() = default;
  GlicSkillsManager(const GlicSkillsManager&) = delete;
  GlicSkillsManager& operator=(const GlicSkillsManager&) = delete;

  virtual void Bind(mojo::PendingReceiver<mojom::SkillsHandler> receiver,
                    mojo::PendingRemote<mojom::SkillsClient> client) = 0;

  // Triggers sending skills previews to the web client. The |updated_tab|
  // is used when the preview update is due to a change at the tab level.
  // TODO(b:481051392): support updating all skill previews rather than just
  // contextual skill previews.
  virtual void UpdateSkillPreviews(
      std::optional<tabs::TabInterface*> updated_tab) = 0;

  // Orchestrates the launch of a Skills dialog. If `tab` is null, it will
  // attempt to create a new tab/window for the given profile.
  virtual void LaunchSkillsDialog(Profile* profile,
                                  skills::Skill skill,
                                  skills::mojom::SkillsDialogType dialog_type,
                                  base::OnceCallback<void(bool)> callback) = 0;

  // Shows the Manage Skills UI.
  virtual void ShowManageSkillsUi() = 0;

  // Shows the Browse Skills UI.
  virtual void ShowBrowseSkillsUi() = 0;

  // Notify that a glic panel associated with the skill manager is being opened.
  virtual void NotifyPanelOpenedOrActivated() = 0;

  virtual void NotifyContextualSkillsChanged(
      std::vector<mojom::SkillPreviewPtr> contextual_skill_previews) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_SKILLS_MANAGER_H_

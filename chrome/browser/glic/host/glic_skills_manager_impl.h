// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_SKILLS_MANAGER_IMPL_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_SKILLS_MANAGER_IMPL_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/host/glic_skills_manager.h"
#include "chrome/browser/glic/host/host.h"

namespace tabs {
class TabInterface;
}

namespace skills {
struct Skill;
}  // namespace skills
namespace glic {

class FocusedTabData;

// This is a host-scoped object that is responsible for sending skills to the
// web client.
class GlicSkillsManagerImpl : public GlicSkillsManager, public Host::Observer {
 public:
  explicit GlicSkillsManagerImpl(Host* host);
  ~GlicSkillsManagerImpl() override;
  explicit GlicSkillsManagerImpl(const GlicSkillsManager&) = delete;
  GlicSkillsManagerImpl& operator=(const GlicSkillsManager&) = delete;

  // GlicSkillsManager
  void UpdateSkillPreviews(
      std::optional<tabs::TabInterface*> updated_tab) override;

  void LaunchSkillsDialog(Profile* profile,
                          skills::Skill skill,
                          base::OnceCallback<void(bool)> callback) override;

  void ShowManageSkillsUi() override;

  glic::mojom::SkillPtr GetContextualSkill(std::string_view skill_id) override;

 private:
  tabs::TabInterface* EnsureTabForSkills();

  // The function corresponding to our subscription.
  void OnFocusedTabChanged(const FocusedTabData& focused_tab_data);

  // Host::Observer
  void WebUiStateChanged(mojom::WebUiState state) override;

  // We update the set of skills on focused tab changes.
  base::CallbackListSubscription focused_tab_changed_subscription_;

  // Owns |this|.
  const raw_ref<Host> host_;

  // Used for observer WebUI state changes; this can also trigger updates.
  base::ScopedObservation<Host, Host::Observer> host_observation_{this};

  // A cache of the contextual skills for the focused tab. When the user runs a
  // skill, Glic retrieves the skill from this cache and sends it to the web
  // client.
  std::vector<glic::mojom::SkillPtr> contextual_skills_;

  base::WeakPtrFactory<GlicSkillsManagerImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_SKILLS_MANAGER_IMPL_H_

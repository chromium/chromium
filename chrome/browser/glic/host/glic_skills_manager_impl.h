// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_SKILLS_MANAGER_IMPL_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_SKILLS_MANAGER_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_skills_manager.h"
#include "chrome/browser/glic/host/host.h"
#include "components/skills/public/skill.mojom-forward.h"
#include "components/skills/public/skills_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace tabs {
class TabInterface;
}

namespace skills {
struct Skill;
}  // namespace skills

class Profile;

namespace glic {

class FocusedTabData;
class GlicInstance;
class GlicInstanceMetrics;

class GlicSkillsClientSession;

// This is a host-scoped object that is responsible for sending skills to the
// web client.
class GlicSkillsManagerImpl : public GlicSkillsManager {
 public:
  GlicSkillsManagerImpl(GlicInstance* instance,
                        Profile* profile,
                        GlicInstanceMetrics* instance_metrics);
  ~GlicSkillsManagerImpl() override;
  explicit GlicSkillsManagerImpl(const GlicSkillsManager&) = delete;
  GlicSkillsManagerImpl& operator=(const GlicSkillsManager&) = delete;

  // GlicSkillsManager
  void Bind(mojo::PendingReceiver<mojom::SkillsHandler> receiver,
            mojo::PendingRemote<mojom::SkillsClient> client) override;

  void UpdateSkillPreviews(
      std::optional<tabs::TabInterface*> updated_tab) override;

  void LaunchSkillsDialog(Profile* profile,
                          skills::Skill skill,
                          skills::mojom::SkillsDialogType dialog_type,
                          base::OnceCallback<void(bool)> callback) override;

  void ShowManageSkillsUi() override;
  void ShowBrowseSkillsUi() override;

  void NotifyPanelOpenedOrActivated() override;

  void NotifyContextualSkillsChanged(
      std::vector<mojom::SkillPreviewPtr> contextual_skill_previews) override;

  Profile* profile() const { return &*profile_; }
  GlicInstance& instance() { return *instance_; }
  GlicActiveTabForProfileTracker& active_tab_tracker() {
    return active_tab_tracker_;
  }
  void RecordSkillsWebClientEvent(mojom::SkillsWebClientEvent event);

 private:
  tabs::TabInterface* EnsureTabForSkills();
  void ShowSkillsUiAtRelativePath(const std::string& path);
  void OnBrowserWindowCreatedForSkills(
      skills::Skill skill,
      skills::mojom::SkillsDialogType dialog_type,
      const GURL& url,
      base::OnceCallback<void(bool)> callback,
      BrowserWindowInterface* browser_window);
  void LaunchSkillsDialogWithTab(tabs::TabInterface* target_tab,
                                 skills::Skill skill,
                                 skills::mojom::SkillsDialogType dialog_type,
                                 base::OnceCallback<void(bool)> callback);

  // The function corresponding to our subscription.
  void OnActiveTabChanged(tabs::TabInterface* tab);

  // The instance that owns this skills manager.
  const raw_ref<GlicInstance> instance_;
  const raw_ref<Profile> profile_;

  GlicActiveTabForProfileTracker active_tab_tracker_;

  // We update the set of skills on active tab changes.
  base::CallbackListSubscription active_tab_changed_subscription_;

  const raw_ref<GlicInstanceMetrics> instance_metrics_;

  std::unique_ptr<GlicSkillsClientSession> session_;
  std::vector<mojom::SkillPreviewPtr> pending_contextual_skills_;

  base::WeakPtrFactory<GlicSkillsManagerImpl> weak_ptr_factory_{this};
};

class GlicSkillsClientSession : public glic::mojom::SkillsHandler,
                                public skills::SkillsService::Observer {
 public:
  GlicSkillsClientSession(
      GlicSkillsManagerImpl* manager,
      mojo::PendingReceiver<mojom::SkillsHandler> receiver,
      mojo::PendingRemote<mojom::SkillsClient> client,
      std::vector<mojom::SkillPreviewPtr> initial_contextual_skills);
  ~GlicSkillsClientSession() override;
  explicit GlicSkillsClientSession(const GlicSkillsClientSession&) = delete;
  GlicSkillsClientSession& operator=(const GlicSkillsClientSession&) = delete;

  // glic::mojom::SkillsHandler
  void CreateSkill(mojom::CreateSkillRequestPtr request,
                   CreateSkillCallback callback) override;
  void UpdateSkill(mojom::UpdateSkillRequestPtr request,
                   UpdateSkillCallback callback) override;
  void GetSkill(const std::string& id, GetSkillCallback callback) override;
  void RecordSkillsWebClientEvent(mojom::SkillsWebClientEvent event) override;
  void ShowManageSkillsUi() override;
  void ShowBrowseSkillsUi() override;

  // skills::SkillsService::Observer
  void OnSkillUpdated(std::string_view skill_id,
                      skills::SkillsService::UpdateSource update_source,
                      bool is_position_changed) override;
  void OnTemporarySkillDisplay(
      std::string_view skill_id,
      skills::SkillsService::DisplayState display_state) override;
  void OnStatusChanged() override;
  void OnDiscoverySkillsUpdated(
      const skills::FirstPartySkillData* first_party_skill_data) override;
  bool Require1PSkillRefresh() override;

  void UpdateSkillPreviews(std::optional<tabs::TabInterface*> updated_tab);
  void NotifyContextualSkillsChanged(
      std::vector<mojom::SkillPreviewPtr> contextual_skill_previews);

 private:
  mojom::SkillPtr GetSkillById(std::string_view skill_id);
  std::vector<mojom::SkillPreviewPtr> GetSkillPreviewsList();

  const raw_ref<GlicSkillsManagerImpl> manager_;
  mojo::Receiver<mojom::SkillsHandler> receiver_;
  mojo::Remote<mojom::SkillsClient> client_;
  raw_ptr<skills::SkillsService> skills_service_;

  // A cache of the contextual skills for the focused tab. When the user runs a
  // skill, Glic retrieves the skill from this cache and sends it to the web
  // client.
  std::vector<glic::mojom::SkillPreviewPtr> contextual_skill_previews_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_SKILLS_MANAGER_IMPL_H_

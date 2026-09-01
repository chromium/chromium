// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_skills_manager_impl.h"

#include <algorithm>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/public/glic_api_metrics.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_dialog_launcher.h"
#include "chrome/browser/skills/skills_glic_mojom_util.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_update_observer.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/webui_url_constants.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.mojom.h"
#include "components/skills/public/skills_metrics.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/base/base_window.h"

namespace glic {

namespace {

mojom::SkillPreviewPtr ToMojomSkillPreview(const skills::proto::Skill& skill) {
  std::optional<std::string> curated_by;
  if (!skill.curated_by().empty()) {
    curated_by = skill.curated_by();
  }
  std::optional<std::string> category;
  if (skill.has_category()) {
    category = skill.category();
  }
  std::optional<GURL> image_url;
  if (skill.has_image_url() && !skill.image_url().empty()) {
    GURL url(skill.image_url());
    if (url.is_valid()) {
      image_url = std::move(url);
    }
  }
  return mojom::SkillPreview::New(
      skill.id(), skill.name(), skill.icon(), mojom::SkillSource::kFirstParty,
      skill.description(), curated_by, image_url, category,
      /*creation_time=*/std::nullopt);
}

}  // namespace

GlicSkillsManagerImpl::GlicSkillsManagerImpl(
    GlicInstance* instance,
    Profile* profile,
    GlicInstanceMetrics* instance_metrics)
    : instance_(*instance),
      profile_(*profile),
      active_tab_tracker_(profile),
      instance_metrics_(CHECK_DEREF(instance_metrics)) {
  active_tab_changed_subscription_ =
      active_tab_tracker_.AddActiveTabChangedCallback(
          base::BindRepeating(&GlicSkillsManagerImpl::OnActiveTabChanged,
                              weak_ptr_factory_.GetWeakPtr()));
}

GlicSkillsManagerImpl::~GlicSkillsManagerImpl() = default;

void GlicSkillsManagerImpl::Bind(
    mojo::PendingReceiver<mojom::SkillsHandler> receiver,
    mojo::PendingRemote<mojom::SkillsClient> client) {
  session_ = std::make_unique<GlicSkillsClientSession>(
      this, std::move(receiver), std::move(client),
      std::exchange(pending_contextual_skills_, {}));
}

void GlicSkillsManagerImpl::UpdateSkillPreviews(
    std::optional<tabs::TabInterface*> updated_tab) {
  if (session_) {
    session_->UpdateSkillPreviews(updated_tab);
  }
}

tabs::TabInterface* GlicSkillsManagerImpl::EnsureTabForSkills() {
  tabs::TabInterface* tab = active_tab_tracker_.GetActiveTab();

  if (tab) {
    return tab;
  }

  if (instance_->IsHibernated()) {
    return nullptr;
  }

  BrowserWindowInterface* active_browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&active_browser, this](BrowserWindowInterface* browser) {
        if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL &&
            browser->GetProfile() == &*profile_) {
          active_browser = browser;
          return false;
        }
        return true;
      });

  if (!active_browser) {
    return nullptr;
  }

  return TabListInterface::From(active_browser)
      ->OpenTab(GURL("chrome://newtab"), -1);
}

void GlicSkillsManagerImpl::LaunchSkillsDialog(
    Profile* profile,
    skills::Skill skill,
    skills::mojom::SkillsDialogType dialog_type,
    base::OnceCallback<void(bool)> callback) {
  tabs::TabInterface* target_tab = EnsureTabForSkills();
  if (!target_tab || !target_tab->IsInNormalWindow()) {
    const GURL skills_url = skills::AppendOpenStartTime(
        GURL(chrome::kChromeUISkillsURL)
            .Resolve(chrome::kChromeUISkillsBrowsePath));

    BrowserWindowCreateParams create_params(
        BrowserWindowInterface::Type::TYPE_NORMAL, *profile_,
        /*from_user_gesture=*/true);

    CreateBrowserWindow(
        std::move(create_params),
        base::BindOnce(&GlicSkillsManagerImpl::OnBrowserWindowCreatedForSkills,
                       weak_ptr_factory_.GetWeakPtr(), std::move(skill),
                       dialog_type, skills_url, std::move(callback)));
    return;
  }

  LaunchSkillsDialogWithTab(target_tab, std::move(skill), dialog_type,
                            std::move(callback));
}

void GlicSkillsManagerImpl::OnBrowserWindowCreatedForSkills(
    skills::Skill skill,
    skills::mojom::SkillsDialogType dialog_type,
    const GURL& url,
    base::OnceCallback<void(bool)> callback,
    BrowserWindowInterface* browser_window) {
  if (!browser_window) {
    std::move(callback).Run(false);
    return;
  }
  browser_window->GetWindow()->Show();
  auto* tab_list = TabListInterface::From(browser_window);
  if (!tab_list) {
    std::move(callback).Run(false);
    return;
  }
  tabs::TabInterface* opened_tab = tab_list->OpenTab(url, /*index=*/-1);
  if (!opened_tab) {
    std::move(callback).Run(false);
    return;
  }

  LaunchSkillsDialogWithTab(opened_tab, std::move(skill), dialog_type,
                            std::move(callback));
}

void GlicSkillsManagerImpl::LaunchSkillsDialogWithTab(
    tabs::TabInterface* target_tab,
    skills::Skill skill,
    skills::mojom::SkillsDialogType dialog_type,
    base::OnceCallback<void(bool)> callback) {
  auto target = std::make_unique<glic::Target>(
      instance_->GetInvokeTarget(/*fallback_surface=*/target_tab->GetHandle()));
  skills::SkillsDialogLauncher::CreateForTab(target_tab, std::move(skill),
                                             dialog_type, std::move(target),
                                             std::move(callback));
}

void GlicSkillsManagerImpl::ShowManageSkillsUi() {
  ShowSkillsUiAtRelativePath(chrome::kChromeUISkillsYourSkillsPath);
}

void GlicSkillsManagerImpl::ShowBrowseSkillsUi() {
  ShowSkillsUiAtRelativePath(chrome::kChromeUISkillsBrowsePath);
}

void GlicSkillsManagerImpl::ShowSkillsUiAtRelativePath(
    const std::string& path) {
  if (!skills::SkillsServiceFactory::IsSkillsEnabledForProfile(&*profile_)) {
    return;
  }
  const GURL skills_url_without_query =
      GURL(chrome::kChromeUISkillsURL).Resolve(path);
  const GURL skills_url = skills::AppendOpenStartTime(skills_url_without_query);

  bool existing_skills_tab_found = false;

  BrowserWindowInterface* most_recent_browser = nullptr;

  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&skills_url_without_query, &existing_skills_tab_found,
       &most_recent_browser, this](BrowserWindowInterface* browser) {
        if (browser->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL ||
            browser->GetProfile() != &*profile_) {
          return true;
        }

        if (!most_recent_browser) {
          most_recent_browser = browser;
        }

        TabListInterface* tab_list = TabListInterface::From(browser);
        if (!tab_list) {
          return true;
        }
        for (const auto& tab : tab_list->GetAllTabs()) {
          content::WebContents* web_contents = tab->GetContents();
          if (web_contents) {
            GURL::Replacements clear_query;
            clear_query.ClearQuery();
            if (web_contents->GetURL().ReplaceComponents(clear_query) ==
                skills_url_without_query) {
              if (browser->GetWindow()) {
                browser->GetWindow()->Activate();
              }
              tab_list->ActivateTab(tab->GetHandle());
              existing_skills_tab_found = true;
              return false;
            }
          }
        }
        return true;
      });

  if (!existing_skills_tab_found) {
    if (most_recent_browser) {
      TabListInterface::From(most_recent_browser)
          ->OpenTab(skills_url, /*index=*/-1);
      return;
    }

    BrowserWindowCreateParams create_params(
        BrowserWindowInterface::Type::TYPE_NORMAL, *profile_,
        /*from_user_gesture=*/true);

    CreateBrowserWindow(
        std::move(create_params),
        base::BindOnce(
            [](const GURL& url, BrowserWindowInterface* browser_window) {
              if (browser_window) {
                browser_window->GetWindow()->Show();
                if (auto* tab_list = TabListInterface::From(browser_window)) {
                  tab_list->OpenTab(url, /*index=*/-1);
                }
              }
            },
            skills_url));
  }
}

void GlicSkillsManagerImpl::OnActiveTabChanged(tabs::TabInterface* tab) {
  if (!skills::SkillsServiceFactory::IsSkillsEnabledForProfile(&*profile_)) {
    return;
  }
  pending_contextual_skills_.clear();
  UpdateSkillPreviews(std::nullopt);
}

void GlicSkillsManagerImpl::NotifyPanelOpenedOrActivated() {
  // NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only
  // restrictions from Skills backend.
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  if (skills::SkillsServiceFactory::IsSkillsEnabledForProfile(&*profile_)) {
    skills::SkillsServiceFactory::GetForProfile(&*profile_)
        ->RefreshDiscoverySkills();
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void GlicSkillsManagerImpl::NotifyContextualSkillsChanged(
    std::vector<mojom::SkillPreviewPtr> contextual_skill_previews) {
  if (!skills::SkillsServiceFactory::IsSkillsEnabledForProfile(&*profile_)) {
    return;
  }
  if (session_) {
    session_->NotifyContextualSkillsChanged(
        std::move(contextual_skill_previews));
  } else {
    pending_contextual_skills_ = std::move(contextual_skill_previews);
  }
}

void GlicSkillsManagerImpl::RecordSkillsWebClientEvent(
    mojom::SkillsWebClientEvent event) {
  instance_metrics_->RecordSkillsWebClientEvent(event);
}

// GlicSkillsClientSession implementation:

GlicSkillsClientSession::GlicSkillsClientSession(
    GlicSkillsManagerImpl* manager,
    mojo::PendingReceiver<mojom::SkillsHandler> receiver,
    mojo::PendingRemote<mojom::SkillsClient> client,
    std::vector<mojom::SkillPreviewPtr> initial_contextual_skills)
    : manager_(*manager),
      receiver_(this, std::move(receiver)),
      client_(std::move(client)) {
  if (base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
    skills_service_ =
        skills::SkillsServiceFactory::GetForProfile(manager_->profile());
    if (skills_service_) {
      skills_service_->AddObserver(this);
    }
  }

  if (client_) {
    client_->NotifySkillPreviewsChanged(GetSkillPreviewsList());
    if (!initial_contextual_skills.empty()) {
      client_->NotifyContextualSkillPreviewsChanged(
          std::move(initial_contextual_skills));
    } else {
      UpdateSkillPreviews(std::nullopt);
    }
  }
}

GlicSkillsClientSession::~GlicSkillsClientSession() {
  if (skills_service_) {
    skills_service_->RemoveObserver(this);
  }
}

void GlicSkillsClientSession::CreateSkill(mojom::CreateSkillRequestPtr request,
                                          CreateSkillCallback callback) {
  LogApiRequestCount(GlicHostApiRequestId::kCreateSkill);
  auto scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false);

  if (!skills::SkillsServiceFactory::IsSkillsEnabledForProfile(
          manager_->profile())) {
    return;
  }
  // There are three scenarios:
  // 1. Users click the + button in the / menu: no field is set.
  // 2. Users click the save as a skill button: only prompt is set.
  // 3. Users edit a 1P skill: all fields are set.
  // TODO(https://crbug.com/479950619): consider using mojom source enum
  // directly in skills::Skill..
  skills::Skill skill(request->id, request->name, request->icon,
                      request->prompt, request->description,
                      /*curated_by=*/"", /*image_url=*/GURL(),
                      skills::GlicMojomToSyncPbSkillSource(request->source));
  manager_->LaunchSkillsDialog(manager_->profile(), std::move(skill),
                               skills::mojom::SkillsDialogType::kAdd,
                               std::move(scoped_callback));
}

void GlicSkillsClientSession::UpdateSkill(mojom::UpdateSkillRequestPtr request,
                                          UpdateSkillCallback callback) {
  LogApiRequestCount(GlicHostApiRequestId::kUpdateSkill);
  auto scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false);

  if (!skills::SkillsServiceFactory::IsSkillsEnabledForProfile(
          manager_->profile())) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kSkillsWebViewV2Enabled)) {
    skills::Skill skill;
    skill.id = request->id;
    manager_->LaunchSkillsDialog(manager_->profile(), std::move(skill),
                                 skills::mojom::SkillsDialogType::kEdit,
                                 std::move(scoped_callback));
    return;
  }

  if (!skills_service_) {
    return;
  }
  // Get skill by ID from the SkillsService.
  if (const skills::Skill* skill = skills_service_->GetSkillById(request->id)) {
    manager_->LaunchSkillsDialog(manager_->profile(), *skill,
                                 skills::mojom::SkillsDialogType::kEdit,
                                 std::move(scoped_callback));
  }
}

void GlicSkillsClientSession::GetSkill(const std::string& id,
                                       GetSkillCallback callback) {
  LogApiRequestCount(GlicHostApiRequestId::kGetSkill);
  if (!skills::SkillsServiceFactory::IsSkillsEnabledForProfile(
          manager_->profile())) {
    std::move(callback).Run(nullptr);
    return;
  }
  mojom::SkillPtr skill = GetSkillById(id);
  std::move(callback).Run(std::move(skill));
}

void GlicSkillsClientSession::RecordSkillsWebClientEvent(
    mojom::SkillsWebClientEvent event) {
  LogApiRequestCount(GlicHostApiRequestId::kRecordSkillsWebClientEvent);
  if (!skills::SkillsServiceFactory::IsSkillsEnabledForProfile(
          manager_->profile())) {
    return;
  }
  manager_->RecordSkillsWebClientEvent(event);
}

void GlicSkillsClientSession::ShowManageSkillsUi() {
  LogApiRequestCount(GlicHostApiRequestId::kShowManageSkillsUi);
  manager_->ShowManageSkillsUi();
}

void GlicSkillsClientSession::ShowBrowseSkillsUi() {
  LogApiRequestCount(GlicHostApiRequestId::kShowBrowseSkillsUi);
  manager_->ShowBrowseSkillsUi();
}

void GlicSkillsClientSession::OnSkillUpdated(
    std::string_view skill_id,
    skills::SkillsService::UpdateSource update_source,
    bool is_position_changed) {
  if (!client_) {
    return;
  }

  if (is_position_changed) {
    // Update all the skill previews for simplicity as updating the position
    // is not frequent.
    UpdateSkillPreviews(std::nullopt);
    client_->NotifySkillPreviewsChanged(GetSkillPreviewsList());
    return;
  }

  mojom::SkillPtr skill = GetSkillById(skill_id);
  if (!skill) {
    client_->NotifySkillDeleted(std::string(skill_id));
  } else {
    client_->NotifySkillPreviewChanged(std::move(skill->preview));
  }
}

void GlicSkillsClientSession::OnTemporarySkillDisplay(
    std::string_view skill_id,
    skills::SkillsService::DisplayState display_state) {
  if (!client_) {
    return;
  }
  switch (display_state) {
    case skills::SkillsService::DisplayState::kDeleted:
      client_->NotifySkillDeleted(std::string(skill_id));
      break;
    case skills::SkillsService::DisplayState::kReshown:
      mojom::SkillPtr skill = GetSkillById(skill_id);
      CHECK(skill);
      client_->NotifySkillPreviewChanged(std::move(skill->preview));
      break;
  }
}

void GlicSkillsClientSession::OnStatusChanged() {
  if (!client_) {
    return;
  }
  UpdateSkillPreviews(std::nullopt);
  client_->NotifySkillPreviewsChanged(GetSkillPreviewsList());
}

void GlicSkillsClientSession::OnDiscoverySkillsUpdated(
    const skills::FirstPartySkillData* first_party_skill_data) {
  // If first_party_skill_data is null, this means we don't have an updated
  // value so we shouldn't modify the stored 1p data.
  if (first_party_skill_data == nullptr) {
    return;
  }
  if (!client_) {
    return;
  }
  UpdateSkillPreviews(std::nullopt);
  client_->NotifySkillPreviewsChanged(GetSkillPreviewsList());
}

bool GlicSkillsClientSession::Require1PSkillRefresh() {
  return manager_->instance().GetPanelState().kind !=
         mojom::PanelStateKind::kHidden;
}

void GlicSkillsClientSession::UpdateSkillPreviews(
    std::optional<tabs::TabInterface*> updated_tab) {
  if (!skills::SkillsServiceFactory::IsSkillsEnabledForProfile(
          manager_->profile())) {
    return;
  }
  if (!client_) {
    return;
  }
  auto* active_tab = manager_->active_tab_tracker().GetActiveTab();
  if (!active_tab ||
      !manager_->instance().GetSharingManager()->IsTabShared(active_tab)) {
    client_->NotifyContextualSkillPreviewsChanged({});
    contextual_skill_previews_.clear();
    return;
  }
  if (updated_tab && active_tab != *updated_tab) {
    return;
  }
  auto* observer = skills::SkillsUpdateObserver::From(active_tab);
  if (!observer) {
    return;
  }
  auto new_skill_previews = observer->GetContextualSkillPreviews();

  if (mojo::Equals(contextual_skill_previews_, new_skill_previews)) {
    return;
  }
  contextual_skill_previews_ = std::move(new_skill_previews);

  std::vector<mojom::SkillPreviewPtr> skill_previews;
  for (const auto& preview : contextual_skill_previews_) {
    skill_previews.push_back(preview.Clone());
  }
  client_->NotifyContextualSkillPreviewsChanged(std::move(skill_previews));
}

void GlicSkillsClientSession::NotifyContextualSkillsChanged(
    std::vector<mojom::SkillPreviewPtr> contextual_skill_previews) {
  if (!skills::SkillsServiceFactory::IsSkillsEnabledForProfile(
          manager_->profile())) {
    return;
  }
  if (client_) {
    client_->NotifyContextualSkillPreviewsChanged(
        std::move(contextual_skill_previews));
  }
}

void GlicSkillsClientSession::OnSkillsEnabledChanged(bool enabled) {
  if (client_) {
    if (enabled) {
      UpdateSkillPreviews(std::nullopt);
      client_->NotifySkillPreviewsChanged(GetSkillPreviewsList());
    } else {
      client_->NotifySkillPreviewsChanged({});
      client_->NotifyContextualSkillPreviewsChanged({});
    }
    client_->NotifySkillsEnabledChanged(enabled);
  }
}

mojom::SkillPtr GlicSkillsClientSession::GetSkillById(
    std::string_view skill_id) {
  if (!skills::SkillsServiceFactory::IsSkillsEnabledForProfile(
          manager_->profile())) {
    return nullptr;
  }
  if (!skills_service_) {
    return nullptr;
  }

  const skills::Skill* skill = skills_service_->GetSkillById(skill_id);
  if (!skill) {
    return nullptr;
  }
  // We should only set the source_skill_id if the skill was derived from
  // another skill.
  return mojom::Skill::New(
      skills::SkillToGlicMojomSkillPreview(skill), skill->prompt,
      skill->source ==
              sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY
          ? skill->source_skill_id
          : std::string());
}

std::vector<mojom::SkillPreviewPtr>
GlicSkillsClientSession::GetSkillPreviewsList() {
  std::vector<mojom::SkillPreviewPtr> skill_previews;
  if (!skills::SkillsServiceFactory::IsSkillsEnabledForProfile(
          manager_->profile())) {
    return skill_previews;
  }
  if (!skills_service_) {
    return skill_previews;
  }
  const std::vector<std::unique_ptr<skills::Skill>>& skills =
      skills_service_->GetSkills();
  skill_previews.reserve(skills.size());
  for (const auto& skill : skills) {
    skill_previews.push_back(skills::SkillToGlicMojomSkillPreview(skill.get()));
  }

  const auto& first_party_skills_list = skills_service_->Get1PSkills();
  std::vector<mojom::SkillPreviewPtr> first_party_skills;
  for (const auto& skill : first_party_skills_list) {
    if (skill.category() == "Internal") {
      continue;
    }
    first_party_skills.push_back(ToMojomSkillPreview(skill));
  }

  std::sort(first_party_skills.begin(), first_party_skills.end(),
            [](const mojom::SkillPreviewPtr& skill_a,
               const mojom::SkillPreviewPtr& skill_b) {
              return skill_a->name < skill_b->name;
            });

  skill_previews.reserve(skill_previews.size() + first_party_skills.size());
  skill_previews.insert(skill_previews.end(),
                        std::make_move_iterator(first_party_skills.begin()),
                        std::make_move_iterator(first_party_skills.end()));
  return skill_previews;
}

}  // namespace glic

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/desk_template_client_lacros.h"

#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/lacros/profile_loader.h"
#include "chrome/browser/lacros/profile_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_util.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_info.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"

namespace {

const std::vector<int> ConvertRangeToTabGroupIndices(const gfx::Range& range) {
  std::vector<int> indices;

  for (uint32_t index = range.start(); index < range.end(); ++index) {
    indices.push_back(static_cast<int>(index));
  }

  return indices;
}

bool ValidateTabRange(const tab_groups::TabGroupInfo& group_info,
                      const TabStripModel* tab_strip_model) {
  const gfx::Range& range = group_info.tab_range;

  if (range.length() == 0) {
    LOG(WARNING) << "group_info: range must have a non-zero length!";
    return false;
  }

  if (range.start() > range.end()) {
    LOG(WARNING)
        << "group_info: range.start() cannot be larger than range.end()!";
    return false;
  }

  if (range.GetMax() > static_cast<uint32_t>(tab_strip_model->count())) {
    LOG(WARNING)
        << "group_info: range max cannot be larger than count of tabs!";
    return false;
  }

  return true;
}

// Creates a standard icon image via `result`, and then calls `callback` with
// the standardized image.
void ImageResultToImageSkia(
    base::OnceCallback<void(const gfx::ImageSkia&)> callback,
    const favicon_base::FaviconRawBitmapResult& result) {
  TRACE_EVENT0("ui", "desk_template_client_lacros::ImageResultToImageSkia");
  if (!result.is_valid()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  auto image =
      gfx::Image::CreateFrom1xPNGBytes(result.bitmap_data).AsImageSkia();
  image.EnsureRepsForSupportedScales();
  std::move(callback).Run(apps::CreateStandardIconImage(image));
}

void AddTabGroupToBrowser(TabStripModel* browser_tab_model,
                          const tab_groups::TabGroupInfo& group_info) {
  if (!ValidateTabRange(group_info, browser_tab_model)) {
    return;
  }

  if (!browser_tab_model->SupportsTabGroups()) {
    return;
  }

  const std::vector<int> tab_range =
      ConvertRangeToTabGroupIndices(group_info.tab_range);
  tab_groups::TabGroupId new_group_id =
      browser_tab_model->AddToNewGroup(tab_range);
  browser_tab_model->group_model()
      ->GetTabGroup(new_group_id)
      ->SetVisualData(group_info.visual_data);
}

void PopulateTabGroups(const std::vector<tab_groups::TabGroupInfo>& group_infos,
                       Browser* out_browser) {
  TabStripModel* browser_tab_model = out_browser->tab_strip_model();
  DCHECK(browser_tab_model);

  for (const auto& group : group_infos) {
    AddTabGroupToBrowser(browser_tab_model, group);
  }
}

void SetPinnedTabs(const int first_non_pinned_tab_index, Browser* out_browser) {
  TabStripModel* browser_tab_model = out_browser->tab_strip_model();
  DCHECK(browser_tab_model);

  if (first_non_pinned_tab_index < 0 ||
      first_non_pinned_tab_index > out_browser->tab_strip_model()->count()) {
    LOG(WARNING) << "Pinned tab outside of tab bounds!";
    return;
  }

  for (int index = 0; index < first_non_pinned_tab_index; ++index) {
    browser_tab_model->SetTabPinned(index, /*pinned=*/true);
  }
}

void ConvertTabGroupsToTabGroupInfos(
    const TabGroupModel* group_model,
    crosapi::mojom::DeskTemplateState* out_state) {
  DCHECK(group_model);
  const std::vector<tab_groups::TabGroupId>& listed_group_ids =
      group_model->ListTabGroups();

  if (listed_group_ids.size() == 0) {
    return;
  }

  out_state->groups = std::vector<tab_groups::TabGroupInfo>();
  for (const tab_groups::TabGroupId& group_id : listed_group_ids) {
    const TabGroup* tab_group = group_model->GetTabGroup(group_id);
    out_state->groups->emplace_back(
        gfx::Range(tab_group->ListTabs()),
        tab_groups::TabGroupVisualData(*(tab_group->visual_data())));
  }
}

void CreateBrowserWithProfile(
    const gfx::Rect& bounds,
    const ui::mojom::WindowShowState show_state,
    crosapi::mojom::DeskTemplateStatePtr additional_state,
    Profile* profile) {
  if (!profile) {
    // If we failed to load the profile, we should not try to proceed.
    return;
  }

  const std::optional<std::string>& browser_app_name =
      additional_state->browser_app_name;

  Browser::CreateParams create_params =
      browser_app_name.has_value() && !browser_app_name.value().empty()
          ? Browser::CreateParams::CreateForApp(browser_app_name.value(),
                                                /*trusted_source=*/true, bounds,
                                                profile,
                                                /*user_gesture=*/false)
          : Browser::CreateParams(Browser::TYPE_NORMAL, profile,
                                  /*user_gesture=*/false);
  create_params.should_trigger_session_restore = false;
  create_params.initial_show_state = show_state;
  create_params.initial_bounds = bounds;
  create_params.restore_id = additional_state->restore_window_id;
  create_params.creation_source = Browser::CreationSource::kDeskTemplate;
  Browser* browser = Browser::Create(create_params);

  // TODO(crbug.com/40910343): Remove after issue is root caused.
  LOG(ERROR) << "window " << additional_state->restore_window_id
             << " created by lacros with " << additional_state->urls.size()
             << " tabs";

  for (size_t i = 0; i < additional_state->urls.size(); i++) {
    chrome::AddTabAt(
        browser, additional_state->urls.at(i), /*index=*/-1,
        /*foreground=*/
        (i == static_cast<size_t>(additional_state->active_index)));
  }

  if (additional_state->groups.has_value()) {
    PopulateTabGroups(additional_state->groups.value(), browser);
  }

  SetPinnedTabs(additional_state->first_non_pinned_index, browser);

  if (show_state == ui::mojom::WindowShowState::kMinimized) {
    // TODO(crbug.com/329800621): This behavior difference between `Show()` and
    // `ShowInactive()` is confusing and should be fixed.
    // Calling `Show()` for a widget created as a minimized widget keeps the
    // widget minimized the first time `Show()` is called. However calling
    // `ShowInactive()` results in the browser window getting unminimized. So
    // use `Show()` instead of `ShowInactive()`.
    browser->window()->Show();
  } else {
    browser->window()->ShowInactive();
  }
}

// This helper will attempt to load a specific profile if `profile_id` is
// non-zero.  Otherwise, the main profile is loaded. The loaded profile (or
// null) is passed to the callback.
void LoadSpecificOrMainProfile(uint64_t profile_id,
                               bool can_trigger_fre,
                               base::OnceCallback<void(Profile*)> callback) {
  auto on_load = [](base::OnceCallback<void(Profile*)> callback,
                    Profile* profile) {
    std::move(callback).Run(
        ProfileManager::MaybeForceOffTheRecordMode(profile));
  };

  if (profile_id) {
    LoadProfileWithId(base::BindOnce(on_load, std::move(callback)),
                      can_trigger_fre, profile_id);
  } else {
    LoadMainProfile(base::BindOnce(on_load, std::move(callback)),
                    can_trigger_fre);
  }
}

}  // namespace

// DeskTemplateClientLacros
DeskTemplateClientLacros::DeskTemplateClientLacros() {
  auto* const lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::DeskTemplate>()) {
    lacros_service->GetRemote<crosapi::mojom::DeskTemplate>()
        ->AddDeskTemplateClient(receiver_.BindNewPipeAndPassRemote());
  }
}

DeskTemplateClientLacros::~DeskTemplateClientLacros() = default;

void DeskTemplateClientLacros::CreateBrowserWithRestoredData(
    const gfx::Rect& bounds,
    const ui::mojom::WindowShowState show_state,
    crosapi::mojom::DeskTemplateStatePtr additional_state) {
  LoadSpecificOrMainProfile(
      additional_state->lacros_profile_id,
      /*can_trigger_fre=*/false,
      base::BindOnce(&CreateBrowserWithProfile, bounds, show_state,
                     std::move(additional_state)));
}

void DeskTemplateClientLacros::GetBrowserInformation(
    uint32_t serial,
    const std::string& window_unique_id,
    GetBrowserInformationCallback callback) {
  Browser* browser = nullptr;

  for (Browser* b : *BrowserList::GetInstance()) {
    if (views::DesktopWindowTreeHostLacros::From(
            b->window()->GetNativeWindow()->GetHost())
            ->platform_window()
            ->GetWindowUniqueId() == window_unique_id) {
      browser = b;
      break;
    }
  }

  if (!browser) {
    std::move(callback).Run(serial, window_unique_id, {});
    return;
  }

  crosapi::mojom::DeskTemplateStatePtr state =
      crosapi::mojom::DeskTemplateState::New();
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  DCHECK(tab_strip_model);
  state->active_index = tab_strip_model->active_index();
  state->first_non_pinned_index = tab_strip_model->IndexOfFirstNonPinnedTab();

  if (browser->type() == Browser::Type::TYPE_APP) {
    state->browser_app_name = browser->app_name();
  }

  for (int i = 0; i < tab_strip_model->count(); ++i) {
    state->urls.push_back(
        tab_strip_model->GetWebContentsAt(i)->GetLastCommittedURL());
  }

  if (tab_strip_model->group_model() != nullptr) {
    ConvertTabGroupsToTabGroupInfos(tab_strip_model->group_model(),
                                    state.get());
  }

  state->lacros_profile_id =
      HashProfilePathToProfileId(browser->profile()->GetPath());

  std::move(callback).Run(serial, window_unique_id, std::move(state));
}

void DeskTemplateClientLacros::GetFaviconImage(
    const GURL& url,
    std::optional<uint64_t> profile_id,
    GetFaviconImageCallback callback) {
  LoadSpecificOrMainProfile(
      profile_id.value_or(0), /*can_trigger_fre=*/false,
      base::BindOnce(&DeskTemplateClientLacros::GetFaviconImageWithProfile,
                     base::Unretained(this), url, std::move(callback)));
}

void DeskTemplateClientLacros::GetFaviconImageWithProfile(
    const GURL& url,
    GetFaviconImageCallback callback,
    Profile* profile) {
  if (!profile) {
    std::move(callback).Run(gfx::ImageSkia());
  }

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);

  favicon_service->GetRawFaviconForPageURL(
      url, {favicon_base::IconType::kFavicon}, 0,
      /*fallback_to_host=*/false,
      base::BindOnce(&ImageResultToImageSkia, std::move(callback)),
      &task_tracker_);
}

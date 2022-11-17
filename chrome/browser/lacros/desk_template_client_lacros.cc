// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/desk_template_client_lacros.h"

#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_util.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"

namespace {

// Creates a callback for when a favicon image is retrieved which creates a
// standard icon image and then calls `callback` with the standardized image.
base::OnceCallback<void(const favicon_base::FaviconImageResult&)>
CreateFaviconResultCallback(
    base::OnceCallback<void(const gfx::ImageSkia&)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(const gfx::ImageSkia&)> image_skia_callback,
         const favicon_base::FaviconImageResult& result) {
        auto image = result.image.AsImageSkia();
        image.EnsureRepsForSupportedScales();
        std::move(image_skia_callback)
            .Run(apps::CreateStandardIconImage(image));
      },
      std::move(callback));
}

}  // namespace

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
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  DCHECK(profile) << "No last used profile is found.";

  const absl::optional<std::string>& browser_app_name =
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
  create_params.initial_show_state =
      static_cast<ui::WindowShowState>(show_state);
  create_params.initial_bounds = bounds;
  create_params.restore_id = additional_state->restore_window_id;
  Browser* browser = Browser::Create(create_params);
  for (size_t i = 0; i < additional_state->urls.size(); i++) {
    chrome::AddTabAt(
        browser, additional_state->urls.at(i), /*index=*/-1,
        /*foreground=*/
        (i == static_cast<size_t>(additional_state->active_index)));
  }
  if (show_state == ui::mojom::WindowShowState::SHOW_STATE_MINIMIZED) {
    browser->window()->Minimize();
  } else {
    browser->window()->ShowInactive();
  }
}

void DeskTemplateClientLacros::GetBrowserInformation(
    uint32_t serial,
    const std::string& window_unique_id,
    GetBrowserInformationCallback callback) {
  Browser* browser = nullptr;
  for (auto* b : *BrowserList::GetInstance()) {
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
  state->active_index = tab_strip_model->active_index();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    state->urls.push_back(
        tab_strip_model->GetWebContentsAt(i)->GetLastCommittedURL());
  }
  std::move(callback).Run(serial, window_unique_id, std::move(state));
}

void DeskTemplateClientLacros::GetFaviconImage(
    const GURL& url,
    GetFaviconImageCallback callback) {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile(),
          ServiceAccessType::EXPLICIT_ACCESS);

  favicon_service->GetFaviconImageForPageURL(
      url, CreateFaviconResultCallback(std::move(callback)), &task_tracker_);
}

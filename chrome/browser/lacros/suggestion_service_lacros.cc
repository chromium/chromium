// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/suggestion_service_lacros.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/profile_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"

namespace {

crosapi::mojom::SuggestionDeviceFormFactor ToMojoFormFactor(
    syncer::DeviceInfo::FormFactor form_factor) {
  switch (form_factor) {
    case syncer::DeviceInfo::FormFactor::kUnknown:
    case syncer::DeviceInfo::FormFactor::kAutomotive:
    case syncer::DeviceInfo::FormFactor::kWearable:
    case syncer::DeviceInfo::FormFactor::kTv:
    case syncer::DeviceInfo::FormFactor::kDesktop:
      return crosapi::mojom::SuggestionDeviceFormFactor::kDesktop;
    case syncer::DeviceInfo::FormFactor::kPhone:
      return crosapi::mojom::SuggestionDeviceFormFactor::kPhone;
    case syncer::DeviceInfo::FormFactor::kTablet:
      return crosapi::mojom::SuggestionDeviceFormFactor::kTablet;
  }
}

}  // namespace

SuggestionServiceLacros::SuggestionServiceLacros() {
  auto* const lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::SuggestionService>()) {
    lacros_service->GetRemote<crosapi::mojom::SuggestionService>()
        ->AddSuggestionServiceProvider(receiver_.BindNewPipeAndPassRemote());
  }
}

SuggestionServiceLacros::~SuggestionServiceLacros() = default;

void SuggestionServiceLacros::GetTabSuggestionItems(
    GetTabSuggestionItemsCallback callback) {
  std::vector<crosapi::mojom::TabSuggestionItemPtr> tab_items;

  Profile* profile = GetMainProfile();
  if (!profile) {
    std::move(callback).Run({});
    return;
  }
  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile);
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      session_sync_service->GetOpenTabsUIDelegate();
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      remote_sessions;

  if (!open_tabs || !open_tabs->GetAllForeignSessions(&remote_sessions)) {
    std::move(callback).Run(std::move(tab_items));
    return;
  }

  for (auto& session : remote_sessions) {
    const std::string& session_tag = session->GetSessionTag();
    std::vector<const sessions::SessionTab*> tabs_in_session;
    if (open_tabs->GetForeignSessionTabs(session_tag, &tabs_in_session) &&
        !tabs_in_session.empty()) {
      for (auto* tab : tabs_in_session) {
        const sessions::SerializedNavigationEntry& current_navigation =
            tab->navigations.at(tab->normalized_navigation_index());
        crosapi::mojom::TabSuggestionItemPtr item =
            crosapi::mojom::TabSuggestionItem::New();
        item->title = base::UTF16ToUTF8(current_navigation.title());
        item->url = current_navigation.virtual_url();
        item->timestamp = current_navigation.timestamp();
        item->favicon_url = current_navigation.favicon_url();
        item->session_name = session->GetSessionName();
        item->form_factor = ToMojoFormFactor(session->GetDeviceFormFactor());
        tab_items.push_back(std::move(item));
      }
    }
  }

  std::move(callback).Run(std::move(tab_items));
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups_page_handler.h"

#include "base/i18n/message_formatter.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/search/ntp_features.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/tab_groups/tab_group_color.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr size_t kMaxFavicons = 4;
constexpr char kTabGroupsLastDismissedTimePrefName[] =
    "NewTabPage.TabGroups.LastDimissedTime";

enum class TimeDimension {
  kDay = 0,
  kDays = 1,
  kWeek = 2,
  kWeeks = 3,
  kMaxValue = kWeeks,
};

ntp::tab_groups::mojom::TabGroupPtr MakeTabGroup(
    const std::string& id,
    const std::string& title,
    const std::string& update_time,
    const std::optional<std::string>& device_name,
    const std::vector<GURL>& urls,
    int total_tabs,
    tab_groups::TabGroupColorId color_id,
    bool is_shared_tab_group) {
  auto group = ntp::tab_groups::mojom::TabGroup::New();
  group->id = id;
  group->title = title;
  group->update_time = update_time;
  group->device_name = device_name;
  group->color = color_id;
  group->favicon_urls.reserve(urls.size());
  for (const GURL& url : urls) {
    group->favicon_urls.emplace_back(url);
  }
  group->total_tab_count = total_tabs;
  group->is_shared_tab_group = is_shared_tab_group;
  return group;
}

std::u16string GetElapsedTimeText(base::Time update_time) {
  base::TimeDelta time_delta = base::Time::Now() - update_time;
  TimeDimension dimension;
  int num;
  if (time_delta < base::Days(1)) {
    return l10n_util::GetStringUTF16(IDS_NTP_MODULES_TAB_GROUPS_RECENTLY_USED);
  } else if (time_delta < base::Days(7)) {
    num = time_delta.InDays();
    dimension = (num == 1) ? TimeDimension::kDay : TimeDimension::kDays;
  } else {
    num = time_delta.InDays() / 7;
    dimension = (num == 1) ? TimeDimension::kWeek : TimeDimension::kWeeks;
  }

  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringFUTF16(IDS_NTP_MODULES_TAB_GROUPS_TIME_DELTA,
                                 base::UTF8ToUTF16(base::NumberToString(num))),
      static_cast<int>(dimension));
}

}  // namespace

// static
void TabGroupsPageHandler::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kTabGroupsLastDismissedTimePrefName, base::Time());
}

TabGroupsPageHandler::TabGroupsPageHandler(
    mojo::PendingReceiver<ntp::tab_groups::mojom::PageHandler>
        pending_page_handler,
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      pref_service_(profile_->GetPrefs()),
      page_handler_(this, std::move(pending_page_handler)) {
  DCHECK(web_contents_);
  DCHECK(profile_);
  tab_group_service_ =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile_);
  CHECK(tab_group_service_);
}

TabGroupsPageHandler::~TabGroupsPageHandler() = default;

void TabGroupsPageHandler::CreateNewTabGroup() {
  auto* browser = webui::GetTabInterface(web_contents_)
                      ->GetBrowserWindowInterface()
                      ->GetBrowserForMigrationOnly();
  browser->command_controller()->ExecuteCommand(IDC_CREATE_NEW_TAB_GROUP);
}

std::vector<const tab_groups::SavedTabGroup*>
TabGroupsPageHandler::FilterActiveGroup(
    std::vector<const tab_groups::SavedTabGroup*> groups) {
  auto* bwi = webui::GetBrowserWindowInterface(web_contents_);
  if (!bwi) {
    return groups;
  }

  TabStripModel* tab_strip_model = bwi->GetTabStripModel();
  if (!tab_strip_model) {
    // No tab strip in this window type.
    return groups;
  }

  // Get the group ID of the currently active tab. This can be nullopt if the
  // active tab is not in any group.
  std::optional<tab_groups::TabGroupId> active_group_id =
      tab_strip_model->GetTabGroupForTab(tab_strip_model->active_index());

  // Filter the group that the active tab is in.
  if (active_group_id.has_value()) {
    groups.erase(
        std::remove_if(
            groups.begin(), groups.end(),
            [&active_group_id](const tab_groups::SavedTabGroup* group) {
              return group->local_group_id().has_value() &&
                     group->local_group_id().value() == active_group_id.value();
            }),
        groups.end());
  }

  return groups;
}

std::vector<const tab_groups::SavedTabGroup*>
TabGroupsPageHandler::GetMostRecentTabGroups(
    std::vector<const tab_groups::SavedTabGroup*> groups,
    size_t count) {
  if (groups.empty() || count == 0) {
    return {};
  }

  groups = FilterActiveGroup(groups);

  // Sort the remaining groups by update time (most recent first).
  std::sort(groups.begin(), groups.end(),
            [](const tab_groups::SavedTabGroup* a,
               const tab_groups::SavedTabGroup* b) {
              return a->update_time() > b->update_time();
            });

  // Truncate the list to at most |count| groups.
  size_t size = std::min(count, groups.size());
  return std::vector<const tab_groups::SavedTabGroup*>(groups.begin(),
                                                       groups.begin() + size);
}

std::optional<std::string> TabGroupsPageHandler::GetDeviceName(
    const std::optional<std::string>& cache_guid) {
  if (!cache_guid.has_value()) {
    return std::nullopt;
  }

  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile_);
  if (!device_info_sync_service) {
    return std::nullopt;
  }

  syncer::DeviceInfoTracker* device_info_tracker =
      device_info_sync_service->GetDeviceInfoTracker();
  // Return nothing if the tab group was last updated by the current device.
  if (!device_info_tracker ||
      device_info_tracker->IsRecentLocalCacheGuid(cache_guid.value())) {
    return std::nullopt;
  }

  const syncer::DeviceInfo* device_info =
      device_info_tracker->GetDeviceInfo(cache_guid.value());
  if (!device_info) {
    return std::nullopt;
  }

  return device_info->client_name();
}

bool TabGroupsPageHandler::ShouldShowZeroState() {
  std::vector<const tab_groups::SavedTabGroup*> groups =
      tab_group_service_->ReadAllGroups();
  return base::FeatureList::IsEnabled(
             ntp_features::kNtpTabGroupsModuleZeroState) &&
         groups.empty();
}

std::vector<ntp::tab_groups::mojom::TabGroupPtr>
TabGroupsPageHandler::GetSavedTabGroups() {
  std::vector<const tab_groups::SavedTabGroup*> groups =
      tab_group_service_->ReadAllGroups();
  std::vector<const tab_groups::SavedTabGroup*> most_recent_groups =
      GetMostRecentTabGroups(
          groups, ntp_features::kNtpTabGroupsModuleMaxGroupCountParam.Get());
  std::vector<ntp::tab_groups::mojom::TabGroupPtr> tab_groups_mojom;

  for (const auto group : most_recent_groups) {
    // Compose a list of up to |kMaxFavicons| URLs.
    const std::vector<tab_groups::SavedTabGroupTab>& tabs = group->saved_tabs();
    const size_t list_size = std::min(kMaxFavicons, tabs.size());
    std::vector<GURL> favicon_urls;
    std::transform(
        tabs.begin(), tabs.begin() + list_size,
        std::back_inserter(favicon_urls),
        [](const tab_groups::SavedTabGroupTab& tab) { return tab.url(); });

    // Set default name for unnamed groups, e.g., "1 tab", "3 tabs".
    std::string title =
        group->title().empty()
            ? l10n_util::GetPluralStringFUTF8(IDS_SAVED_TAB_GROUP_TABS_COUNT,
                                              group->saved_tabs().size())
            : base::UTF16ToUTF8(group->title());

    std::string update_time =
        base::UTF16ToUTF8(GetElapsedTimeText(group->update_time()));
    std::optional<std::string> device_name =
        GetDeviceName(group->last_updater_cache_guid());

    tab_groups_mojom.push_back(
        MakeTabGroup(group->saved_guid().AsLowercaseString(), title,
                     update_time, device_name, favicon_urls, tabs.size(),
                     group->color(), group->is_shared_tab_group()));
  }

  return tab_groups_mojom;
}

void TabGroupsPageHandler::GetTabGroups(GetTabGroupsCallback callback) {
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                         std::nullopt, false);

  base::Time dismiss_time =
      pref_service_->GetTime(kTabGroupsLastDismissedTimePrefName);
  if (dismiss_time != base::Time() &&
      base::Time::Now() - dismiss_time <
          ntp_features::kNtpTabGroupsModuleWindowEndDeltaParam.Get()) {
    // Callback wrapper will be invoked with std::nullopt on destruction.
    return;
  }

  const std::string data_type_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpTabGroupsModule,
      ntp_features::kNtpTabGroupsModuleDataParam);
  std::vector<ntp::tab_groups::mojom::TabGroupPtr> tab_groups_mojom;
  bool should_show_zero_state =
      data_type_param.empty()
          ? ShouldShowZeroState()
          : (data_type_param.find("Fake Zero State") != std::string::npos);

  if (data_type_param.empty()) {
    // Fetch real Tab Groups data.
    tab_groups_mojom = GetSavedTabGroups();
  } else {
    // Generate fake data.
    if (data_type_param.find("Fake Data") != std::string::npos) {
      tab_groups_mojom.push_back(MakeTabGroup(
          "0", "Tab Group 1 (3 tabs total)", "Recently used", "Test Device",
          std::vector<GURL>{GURL("https://www.google.com"),
                            GURL("https://www.google.com"),
                            GURL("https://www.google.com")},
          3, tab_groups::TabGroupColorId::kBlue, false));

      tab_groups_mojom.push_back(MakeTabGroup(
          "0", "Tab Group 2 (4 tabs total)", "Used 1 day ago", "Test Device",
          std::vector<GURL>{
              GURL("https://www.google.com"), GURL("https://www.google.com"),
              GURL("https://www.google.com"), GURL("https://www.google.com")},
          4, tab_groups::TabGroupColorId::kPurple, true));

      tab_groups_mojom.push_back(MakeTabGroup(
          "0", "Tab Group 3 (8 tabs total)", "Used 1 week ago", "Test Device",
          std::vector<GURL>{
              GURL("https://www.google.com"), GURL("https://www.google.com"),
              GURL("https://www.google.com"), GURL("https://www.google.com")},
          8, tab_groups::TabGroupColorId::kYellow, false));

      tab_groups_mojom.push_back(MakeTabGroup(
          "0", "Tab Group 4 (199 tabs total)", "Used 2 weeks ago", std::nullopt,
          std::vector<GURL>{
              GURL("https://www.google.com"), GURL("https://www.google.com"),
              GURL("https://www.google.com"), GURL("https://www.google.com")},
          199, tab_groups::TabGroupColorId::kGreen, true));
    } else if (data_type_param.find("Fake Zero State") != std::string::npos) {
      // No-op: return empty vector to invoke the zero state card.
    }
  }

  std::move(callback).Run(std::move(tab_groups_mojom), should_show_zero_state);
}

void TabGroupsPageHandler::DismissModule() {
  pref_service_->SetTime(kTabGroupsLastDismissedTimePrefName,
                         base::Time::Now());
}

void TabGroupsPageHandler::RestoreModule() {
  // Clear the module's last dimissed time.
  pref_service_->SetTime(kTabGroupsLastDismissedTimePrefName, base::Time());
}

void TabGroupsPageHandler::OpenTabGroup(const std::string& id) {
  const base::Uuid group_id = base::Uuid::ParseLowercase(id);
  const std::optional<tab_groups::SavedTabGroup> group =
      tab_group_service_->GetGroup(group_id);

  if (!group.has_value() || group->saved_tabs().empty()) {
    return;
  }

  auto* browser = webui::GetTabInterface(web_contents_)
                      ->GetBrowserWindowInterface()
                      ->GetBrowserForMigrationOnly();
  tab_group_service_->OpenTabGroup(
      group->saved_guid(),
      std::make_unique<tab_groups::TabGroupActionContextDesktop>(
          browser, tab_groups::OpeningSource::kOpenedFromRevisitUi));
}

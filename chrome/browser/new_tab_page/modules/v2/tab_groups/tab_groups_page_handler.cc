// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups_page_handler.h"

#include "base/i18n/message_formatter.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/search/ntp_features.h"
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

std::vector<const tab_groups::SavedTabGroup*> GetMostRecentTabGroups(
    std::vector<const tab_groups::SavedTabGroup*> groups,
    size_t count) {
  std::sort(groups.begin(), groups.end(),
            [](const tab_groups::SavedTabGroup* a,
               const tab_groups::SavedTabGroup* b) {
              return a->update_time() > b->update_time();
            });
  size_t size = std::min(count, groups.size());
  return std::vector<const tab_groups::SavedTabGroup*>(groups.begin(),
                                                       groups.begin() + size);
}

ntp::tab_groups::mojom::TabGroupPtr MakeTabGroup(const std::string& title,
                                                 const std::string& update_time,
                                                 const std::vector<GURL>& urls,
                                                 int total_tabs) {
  auto group = ntp::tab_groups::mojom::TabGroup::New();
  group->title = title;
  group->update_time = update_time;
  group->favicon_urls.reserve(urls.size());
  for (const GURL& url : urls) {
    group->favicon_urls.emplace_back(url);
  }
  group->total_tab_count = total_tabs;
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
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      pref_service_(profile_->GetPrefs()),
      page_handler_(this, std::move(pending_page_handler)) {
  DCHECK(profile_);
  tab_group_service_ =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile_);
  CHECK(tab_group_service_);
}

TabGroupsPageHandler::~TabGroupsPageHandler() = default;

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

    std::string title = base::UTF16ToUTF8(group->title());
    std::string update_time =
        base::UTF16ToUTF8(GetElapsedTimeText(group->update_time()));

    tab_groups_mojom.push_back(
        MakeTabGroup(title, update_time, favicon_urls, tabs.size()));
  }

  return tab_groups_mojom;
}

void TabGroupsPageHandler::GetTabGroups(GetTabGroupsCallback callback) {
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                         std::nullopt);

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

  if (data_type_param.empty()) {
    // Fetch real Tab Groups data.
    tab_groups_mojom = GetSavedTabGroups();
  } else {
    // Generate fake data.
    if (data_type_param.find("Fake Data") != std::string::npos) {
      tab_groups_mojom.push_back(
          MakeTabGroup("Tab Group 1 (3 tabs total)", "Recently Used",
                       std::vector<GURL>{GURL("https://www.google.com"),
                                         GURL("https://www.youtube.com"),
                                         GURL("https://www.wikipedia.org")},
                       3));

      tab_groups_mojom.push_back(
          MakeTabGroup("Tab Group 2 (4 tabs total)", "Used 1 day ago",
                       std::vector<GURL>{GURL("https://www.google.com"),
                                         GURL("https://www.youtube.com"),
                                         GURL("https://www.wikipedia.org"),
                                         GURL("https://maps.google.com")},
                       4));

      tab_groups_mojom.push_back(
          MakeTabGroup("Tab Group 3 (8 tabs total)", "Used 1 week ago",
                       std::vector<GURL>{GURL("https://www.google.com"),
                                         GURL("https://www.youtube.com"),
                                         GURL("https://www.wikipedia.org"),
                                         GURL("https://maps.google.com")},
                       8));

      tab_groups_mojom.push_back(
          MakeTabGroup("Tab Group 4 (199 tabs total)", "Used 2 weeks ago",
                       std::vector<GURL>{GURL("https://www.google.com"),
                                         GURL("https://www.youtube.com"),
                                         GURL("https://www.wikipedia.org"),
                                         GURL("https://maps.google.com")},
                       199));
    } else if (data_type_param.find("Fake Zero State") != std::string::npos) {
      // No-op: return empty vector to invoke the zero state card.
    }
  }

  std::move(callback).Run(std::move(tab_groups_mojom));
}

void TabGroupsPageHandler::DismissModule() {
  pref_service_->SetTime(kTabGroupsLastDismissedTimePrefName,
                         base::Time::Now());
}

void TabGroupsPageHandler::RestoreModule() {
  // Clear the module's last dimissed time.
  pref_service_->SetTime(kTabGroupsLastDismissedTimePrefName, base::Time());
}

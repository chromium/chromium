// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_service.h"

#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"
#include "chrome/browser/profiles/profile.h"

namespace privacy_sandbox {

namespace {

using ::privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using ::privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;

using GroupKey = std::variant<std::monostate, NoticeViewGroup, NoticeId>;

size_t CountTargetApis(base::span<Notice*> notices) {
  return std::accumulate(notices.begin(), notices.end(), 0,
                         [](size_t total, Notice* notice) {
                           return total + notice->target_apis().size();
                         });
}

int CompareNoticeGroups(base::span<Notice*> lhs, base::span<Notice*> rhs) {
  size_t lhs_api_count = CountTargetApis(lhs);
  size_t rhs_api_count = CountTargetApis(rhs);

  // 1: The group targeting more APIs wins.
  if (lhs_api_count != rhs_api_count) {
    return (lhs_api_count > rhs_api_count) ? 1 : -1;
  }

  // 2: If unique API counts are the same, the group with fewer
  // notices wins.
  if (lhs.size() != rhs.size()) {
    return (lhs.size() < rhs.size()) ? 1 : -1;
  }

  // Otherwise, the groups are considered equal by these criteria.
  return 0;
}

std::set<NoticeApi*> GetEligibleApis(base::span<NoticeApi*> apis) {
  // Consider only APIs that are Enabled, have some level of
  // eligibility, and weren't already fulfilled in the past.
  std::set<NoticeApi*> filtered_apis;
  for (auto api : apis) {
    // TODO(crbug.com/417725558) Right now, we assume that all APIs are their
    // latest versions. Add support for getting the latest enabled API version
    // when versions are supported.
    if (!api->IsEnabled()) {
      continue;
    }
    if (api->GetEligibilityLevel() == EligibilityLevel::kNotEligible) {
      continue;
    }

    // TODO(crbug.com/417733694) This actually does some redundant work:
    // Refetches the eligibility, Ideally, this should be pushed once by the ps
    // service, and should be cached here.
    if (api->IsFulfilled()) {
      continue;
    }
    filtered_apis.insert(api);
  }
  return filtered_apis;
}

std::map<GroupKey, std::vector<Notice*>> GetEligibleNotices(
    SurfaceType surface,
    std::set<NoticeApi*> filtered_apis,
    base::span<Notice*> notices) {
  // From all of the available Notices, keep only ones that
  //    - Target the requested surface
  //    - Haven't yet been fulfilled
  //    - Are Enabled
  //    - Based on the eligibility, can fulfill all of its target APIs
  //    - Do not contain any target APIs not present in the filtered list.
  // The Notices are grouped if they have an assigned group. Otherwise, they are
  // in their own group.
  std::map<GroupKey, std::vector<Notice*>> grouped_notices;
  for (Notice* notice : notices) {
    if (auto [_, s] = notice->notice_id(); s != surface) {
      continue;
    }
    if (notice->was_fulfilled()) {
      continue;
    }
    if (!notice->IsEnabled()) {
      continue;
    }

    // TODO(crbug.com/417727236) Run the exclusion callback that each notice
    // defines.

    // Check that ALL target APIs are fulfillable by the notice.
    if (!notice->CanFulfillAllTargetApis()) {
      continue;
    }

    // Check if all target APIs are in the filtered list.
    if (std::ranges::any_of(notice->target_apis(),
                            [&filtered_apis](NoticeApi* target_api) {
                              return !filtered_apis.contains(target_api);
                            })) {
      continue;
    }

    // Notice is eligible at this point. Add it to its group.
    if (notice->view_group().first != NoticeViewGroup::kNotSet) {
      grouped_notices[{notice->view_group().first}].push_back(notice);
    } else {
      grouped_notices[{notice->notice_id()}].push_back(notice);
    }
  }
  return grouped_notices;
}

GroupKey GetBestGroupKey(
    std::map<GroupKey, std::vector<Notice*>>& grouped_notices) {
  // Choosing the best group based on the which group covers the most apis with
  // the least number of notices.
  GroupKey best_group_key;
  for (auto& [group, notices] : grouped_notices) {
    if (best_group_key.index() == 0 ||
        CompareNoticeGroups(notices, grouped_notices[best_group_key]) > 0) {
      best_group_key = group;
    }
  }

  return best_group_key;
}

}  // namespace

PrivacySandboxNoticeService::PrivacySandboxNoticeService(
    Profile* profile,
    std::unique_ptr<NoticeCatalog> catalog,
    std::unique_ptr<NoticeStorage> storage)
    : profile_(profile),
      catalog_(std::move(catalog)),
      notice_storage_(std::move(storage)) {
  CHECK(profile_);
  CHECK(notice_storage_);
  CHECK(catalog_);

#if !BUILDFLAG(IS_ANDROID)
  desktop_view_manager_ = std::make_unique<DesktopViewManager>(this);
  CHECK(desktop_view_manager_);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Refresh fulfillment status for all notices at service initialization.
  for (Notice* notice : catalog_->GetNotices()) {
    CHECK(notice);
    notice->RefreshFulfillmentStatus(*notice_storage_);
  }

  EmitStartupHistograms();
}

PrivacySandboxNoticeService::~PrivacySandboxNoticeService() = default;

void PrivacySandboxNoticeService::Shutdown() {
  profile_ = nullptr;
  notice_storage_ = nullptr;
  catalog_ = nullptr;
}

void PrivacySandboxNoticeService::EventOccurred(
    NoticeId notice_id,
    PrivacySandboxNoticeEvent event) {
  Notice* notice = catalog_->GetNotice(notice_id);
  CHECK(notice);

  notice_storage()->RecordEvent(*notice, event);

  // Refresh fulfillment status after an event has occurred.
  notice->RefreshFulfillmentStatus(*notice_storage());
  notice->UpdateTargetApiResults(event);
}

std::vector<PrivacySandboxNotice>
PrivacySandboxNoticeService::GetRequiredNotices(SurfaceType surface) {
  // Step 1: Filtering APIs.
  std::set<NoticeApi*> filtered_apis =
      GetEligibleApis(catalog_->GetNoticeApis());

  // Step 2: Finding Notices for the surface type, and the filtered APIs.
  auto grouped_notices = GetEligibleNotices(surface, std::move(filtered_apis),
                                            catalog_->GetNotices());

  // Step 3: Scoring.
  GroupKey best_group_key = GetBestGroupKey(grouped_notices);

  if (best_group_key.index() == 0) {
    return {};
  }

  // Step 4: Sort and return.
  // Now we have the best group, sort and return.
  std::vector<Notice*>& best_group = grouped_notices[best_group_key];
  std::sort(best_group.begin(), best_group.end(), [](Notice* a, Notice* b) {
    return a->view_group().second < b->view_group().second;
  });

  std::vector<PrivacySandboxNotice> required_notices;
  for (Notice* notice : best_group) {
    required_notices.push_back(notice->notice_id().first);
  }

  return required_notices;
}

void PrivacySandboxNoticeService::EmitStartupHistograms() {
  notice_storage()->RecordStartupHistograms();
}

#if !BUILDFLAG(IS_ANDROID)
DesktopViewManagerInterface*
PrivacySandboxNoticeService::GetDesktopViewManager() {
  return desktop_view_manager_.get();
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace privacy_sandbox

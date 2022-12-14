// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/important_sites_util.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <unordered_set>
#include <utility>

#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/installable/installable_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#endif

namespace site_engagement {

namespace {

using bookmarks::BookmarkModel;
using bookmarks::UrlAndTitle;
using ImportantDomainInfo = ImportantSitesUtil::ImportantDomainInfo;
using ImportantReason = ImportantSitesUtil::ImportantReason;

// Note: These values are stored on both the per-site content settings
// dictionary and the dialog preference dictionary.

static const char kTimeLastIgnored[] = "TimeLastIgnored";
static const int kSuppressionExpirationTimeDays = 30 * 5;

static const char kNumTimesIgnoredName[] = "NumTimesIgnored";
static const int kTimesIgnoredForSuppression = 3;

// These are the maximum # of bookmarks we can use as signals. If the user has
// <= kMaxBookmarks, then we just use those bookmarks. Otherwise we filter all
// bookmarks on site engagement > 0, sort, and trim to kMaxBookmarks.
static const int kMaxBookmarks = 5;

// We need this to be a macro, as the histogram macros cache their pointers
// after the first call, so when we change the uma name we check fail if we're
// just a method.
#define RECORD_UMA_FOR_IMPORTANT_REASON(uma_name, uma_count_name,              \
                                        reason_bitfield)                       \
  do {                                                                         \
    int count = 0;                                                             \
    int32_t bitfield = (reason_bitfield);                                      \
    for (int i = 0; i < ImportantReason::REASON_BOUNDARY; i++) {               \
      if ((bitfield >> i) & 1) {                                               \
        count++;                                                               \
        UMA_HISTOGRAM_ENUMERATION((uma_name), static_cast<ImportantReason>(i), \
                                  ImportantReason::REASON_BOUNDARY);           \
      }                                                                        \
    }                                                                          \
    UMA_HISTOGRAM_EXACT_LINEAR(                                                \
        (uma_count_name), count,                                               \
        static_cast<int>(ImportantReason::REASON_BOUNDARY));                   \
  } while (0)

// Do not change the values here, as they are used for UMA histograms and
// testing in important_sites_util_unittest.
enum CrossedReason {
  CROSSED_DURABLE = 0,
  CROSSED_NOTIFICATIONS = 1,
  CROSSED_ENGAGEMENT = 2,
  CROSSED_NOTIFICATIONS_AND_ENGAGEMENT = 3,
  CROSSED_DURABLE_AND_ENGAGEMENT = 4,
  CROSSED_NOTIFICATIONS_AND_DURABLE = 5,
  CROSSED_NOTIFICATIONS_AND_DURABLE_AND_ENGAGEMENT = 6,
  CROSSED_REASON_UNKNOWN = 7,
  CROSSED_REASON_BOUNDARY
};

void RecordIgnore(base::Value::Dict& dict) {
  int times_ignored = dict.FindInt(kNumTimesIgnoredName).value_or(0);
  dict.Set(kNumTimesIgnoredName, ++times_ignored);
  dict.Set(kTimeLastIgnored, base::Time::Now().ToDoubleT());
}

// If we should suppress the item with the given dictionary ignored record.
bool ShouldSuppressItem(base::Value::Dict& dict) {
  absl::optional<double> last_ignored_time = dict.FindDouble(kTimeLastIgnored);
  if (last_ignored_time) {
    base::TimeDelta diff =
        base::Time::Now() - base::Time::FromDoubleT(*last_ignored_time);
    if (diff >= base::Days(kSuppressionExpirationTimeDays)) {
      dict.Set(kNumTimesIgnoredName, 0);
      dict.Remove(kTimeLastIgnored);
      return false;
    }
  }

  absl::optional<int> times_ignored = dict.FindInt(kNumTimesIgnoredName);
  return times_ignored && *times_ignored >= kTimesIgnoredForSuppression;
}

CrossedReason GetCrossedReasonFromBitfield(int32_t reason_bitfield) {
  bool durable = (reason_bitfield & (1 << ImportantReason::DURABLE)) != 0;
  bool notifications =
      (reason_bitfield & (1 << ImportantReason::NOTIFICATIONS)) != 0;
  bool engagement = (reason_bitfield & (1 << ImportantReason::ENGAGEMENT)) != 0;
  if (durable && notifications && engagement)
    return CROSSED_NOTIFICATIONS_AND_DURABLE_AND_ENGAGEMENT;
  else if (notifications && durable)
    return CROSSED_NOTIFICATIONS_AND_DURABLE;
  else if (notifications && engagement)
    return CROSSED_NOTIFICATIONS_AND_ENGAGEMENT;
  else if (durable && engagement)
    return CROSSED_DURABLE_AND_ENGAGEMENT;
  else if (notifications)
    return CROSSED_NOTIFICATIONS;
  else if (durable)
    return CROSSED_DURABLE;
  else if (engagement)
    return CROSSED_ENGAGEMENT;
  return CROSSED_REASON_UNKNOWN;
}

void MaybePopulateImportantInfoForReason(
    const GURL& origin,
    std::set<GURL>* visited_origins,
    ImportantReason reason,
    absl::optional<std::string> app_name,
    std::map<std::string, ImportantDomainInfo>* output) {
  if (!origin.is_valid() || !visited_origins->insert(origin).second)
    return;
  std::string registerable_domain =
      ImportantSitesUtil::GetRegisterableDomainOrIP(origin);
  ImportantDomainInfo& info = (*output)[registerable_domain];
  info.reason_bitfield |= 1 << reason;
  if (info.example_origin.is_empty()) {
    info.registerable_domain = registerable_domain;
    info.example_origin = origin;
  }
  info.app_name = app_name;
}

// Returns the score associated with the given reason. The order of
// ImportantReason does not need to correspond to the score order. The higher
// the score, the more important the reason is.
int GetScoreForReason(ImportantReason reason) {
  switch (reason) {
    case ImportantReason::ENGAGEMENT:
      return 1 << 0;
    case ImportantReason::DURABLE:
      return 1 << 1;
    case ImportantReason::BOOKMARKS:
      return 1 << 2;
    case ImportantReason::HOME_SCREEN:
      return 1 << 3;
    case ImportantReason::NOTIFICATIONS:
      return 1 << 4;
    case ImportantReason::REASON_BOUNDARY:
      return 0;
  }
  return 0;
}

int GetScoreForReasonsBitfield(int32_t reason_bitfield) {
  int score = 0;
  for (int i = 0; i < ImportantReason::REASON_BOUNDARY; i++) {
    if ((reason_bitfield >> i) & 1) {
      score += GetScoreForReason(static_cast<ImportantReason>(i));
    }
  }
  return score;
}

// Returns if |a| has a higher score than |b|, so that when we sort the higher
// score is first.
bool CompareDescendingImportantInfo(
    const std::pair<std::string, ImportantDomainInfo>& a,
    const std::pair<std::string, ImportantDomainInfo>& b) {
  int score_a = GetScoreForReasonsBitfield(a.second.reason_bitfield);
  int score_b = GetScoreForReasonsBitfield(b.second.reason_bitfield);
  int bitfield_diff = score_a - score_b;
  if (bitfield_diff != 0)
    return bitfield_diff > 0;
  return a.second.engagement_score > b.second.engagement_score;
}

std::unordered_set<std::string> GetSuppressedImportantDomains(
    Profile* profile) {
  ContentSettingsForOneType content_settings_list;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  map->GetSettingsForOneType(ContentSettingsType::IMPORTANT_SITE_INFO,

                             &content_settings_list);
  std::unordered_set<std::string> ignoring_domains;
  for (ContentSettingPatternSource& site : content_settings_list) {
    GURL origin(site.primary_pattern.ToString());
    if (!origin.is_valid() || base::Contains(ignoring_domains, origin.host())) {
      continue;
    }

    if (!site.setting_value.is_dict())
      continue;

    if (ShouldSuppressItem(site.setting_value.GetDict())) {
      ignoring_domains.insert(origin.host());
    }
  }
  return ignoring_domains;
}

// Inserts origins with some engagement measure into the map, including a site
// engagement cutoff and recent launches from home screen.
void PopulateInfoMapWithEngagement(
    Profile* profile,
    blink::mojom::EngagementLevel minimum_engagement,
    std::map<GURL, double>* engagement_map,
    std::map<std::string, ImportantDomainInfo>* output) {
  SiteEngagementService* service = SiteEngagementService::Get(profile);
  std::vector<mojom::SiteEngagementDetails> engagement_details =
      service->GetAllDetails();
  std::set<GURL> content_origins;

  // We can have multiple origins for a single domain, so we record the one
  // with the highest engagement score.
  for (const auto& detail : engagement_details) {
    if (detail.installed_bonus > 0) {
      MaybePopulateImportantInfoForReason(detail.origin, &content_origins,
                                          ImportantReason::HOME_SCREEN,
                                          absl::nullopt, output);
    }

    (*engagement_map)[detail.origin] = detail.total_score;

    if (!SiteEngagementService::IsEngagementAtLeast(detail.total_score,
                                                    minimum_engagement)) {
      continue;
    }

    std::string registerable_domain =
        ImportantSitesUtil::GetRegisterableDomainOrIP(detail.origin);
    ImportantDomainInfo& info = (*output)[registerable_domain];
    if (detail.total_score > info.engagement_score) {
      info.registerable_domain = registerable_domain;
      info.engagement_score = detail.total_score;
      info.example_origin = detail.origin;
      info.reason_bitfield |= 1 << ImportantReason::ENGAGEMENT;
    }
  }
}

void PopulateInfoMapWithContentTypeAllowed(
    Profile* profile,
    ContentSettingsType content_type,
    ImportantReason reason,
    std::map<std::string, ImportantDomainInfo>* output) {
  // Grab our content settings list.
  ContentSettingsForOneType content_settings_list;
  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      content_type, &content_settings_list);

  // Extract a set of urls, using the primary pattern. We don't handle
  // wildcard patterns.
  std::set<GURL> content_origins;
  for (const ContentSettingPatternSource& site : content_settings_list) {
    if (site.GetContentSetting() != CONTENT_SETTING_ALLOW)
      continue;
    GURL url(site.primary_pattern.ToString());

    MaybePopulateImportantInfoForReason(url, &content_origins, reason,
                                        absl::nullopt, output);
  }
}

void PopulateInfoMapWithBookmarks(
    Profile* profile,
    const std::map<GURL, double>& engagement_map,
    std::map<std::string, ImportantDomainInfo>* output) {
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContextIfExists(profile);
  if (!model)
    return;
  std::vector<UrlAndTitle> untrimmed_bookmarks;
  model->GetBookmarks(&untrimmed_bookmarks);

  // Process the bookmarks and optionally trim them if we have too many.
  std::vector<UrlAndTitle> result_bookmarks;
  if (untrimmed_bookmarks.size() > kMaxBookmarks) {
    base::ranges::copy_if(
        untrimmed_bookmarks, std::back_inserter(result_bookmarks),
        [&engagement_map](const UrlAndTitle& entry) {
          auto it = engagement_map.find(entry.url.DeprecatedGetOriginAsURL());
          double score = it == engagement_map.end() ? 0 : it->second;
          return SiteEngagementService::IsEngagementAtLeast(
              score, blink::mojom::EngagementLevel::LOW);
        });
    // TODO(dmurph): Simplify this (and probably much more) once
    // SiteEngagementService::GetAllDetails lands (crbug/703848), as that will
    // allow us to remove most of these lookups and merging of signals.
    std::sort(
        result_bookmarks.begin(), result_bookmarks.end(),
        [&engagement_map](const UrlAndTitle& a, const UrlAndTitle& b) {
          auto a_it = engagement_map.find(a.url.DeprecatedGetOriginAsURL());
          auto b_it = engagement_map.find(b.url.DeprecatedGetOriginAsURL());
          double a_score = a_it == engagement_map.end() ? 0 : a_it->second;
          double b_score = b_it == engagement_map.end() ? 0 : b_it->second;
          return a_score > b_score;
        });
    if (result_bookmarks.size() > kMaxBookmarks)
      result_bookmarks.resize(kMaxBookmarks);
  } else {
    result_bookmarks = std::move(untrimmed_bookmarks);
  }

  std::set<GURL> content_origins;
  for (const UrlAndTitle& bookmark : result_bookmarks) {
    MaybePopulateImportantInfoForReason(bookmark.url, &content_origins,
                                        ImportantReason::BOOKMARKS,
                                        absl::nullopt, output);
  }
}

// WebAppRegistrar is desktop specific, but Android does not warn users
// about clearing data for installed apps, so this and any functions explicitly
// used to warn about clearing data for installed apps can be excluded from the
// Android build.
#if !BUILDFLAG(IS_ANDROID)
void PopulateInfoMapWithInstalledEngagedInTimePeriod(
    browsing_data::TimePeriod time_period,
    Profile* profile,
    std::map<std::string, ImportantDomainInfo>* output) {
  SiteEngagementService* service = SiteEngagementService::Get(profile);
  std::vector<mojom::SiteEngagementDetails> engagement_details =
      service->GetAllDetailsEngagedInTimePeriod(time_period);
  std::set<GURL> content_origins;

  // Check with WebAppRegistrar to make sure the apps have not yet been
  // uninstalled.
  std::map<std::string, std::string> installed_origins_map;
  if (web_app::AreWebAppsUserInstallable(profile)) {
    const web_app::WebAppRegistrar& registrar =
        web_app::WebAppProvider::GetForWebApps(profile)->registrar_unsafe();
    auto app_ids = registrar.GetAppIds();
    for (auto& app_id : app_ids) {
      GURL scope = registrar.GetAppScope(app_id);
      DCHECK(scope.is_valid());
      auto app_name = registrar.GetAppShortName(app_id);
      installed_origins_map.emplace(
          std::make_pair(scope.DeprecatedGetOriginAsURL().spec(), app_name));
    }
  }

  for (const auto& detail : engagement_details) {
    if (detail.installed_bonus > 0) {
      auto origin_pair = installed_origins_map.find(detail.origin.spec());
      if (origin_pair != installed_origins_map.end()) {
        MaybePopulateImportantInfoForReason(detail.origin, &content_origins,
                                            ImportantReason::HOME_SCREEN,
                                            origin_pair->second, output);
      }
    }
  }
}
#endif

}  // namespace

ImportantDomainInfo::ImportantDomainInfo() = default;
ImportantDomainInfo::~ImportantDomainInfo() = default;
ImportantDomainInfo::ImportantDomainInfo(ImportantDomainInfo&&) = default;
ImportantDomainInfo& ImportantDomainInfo::operator=(ImportantDomainInfo&&) =
    default;

std::string ImportantSitesUtil::GetRegisterableDomainOrIP(const GURL& url) {
  return GetRegisterableDomainOrIPFromHost(url.host_piece());
}

std::string ImportantSitesUtil::GetRegisterableDomainOrIPFromHost(
    base::StringPiece host) {
  std::string registerable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (registerable_domain.empty() && url::HostIsIPAddress(host))
    registerable_domain = std::string(host);
  return registerable_domain;
}

bool ImportantSitesUtil::IsDialogDisabled(Profile* profile) {
  PrefService* service = profile->GetPrefs();
  ScopedDictPrefUpdate update(service, prefs::kImportantSitesDialogHistory);

  return ShouldSuppressItem(update.Get());
}

void ImportantSitesUtil::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kImportantSitesDialogHistory);
}

// static
std::set<std::string> ImportantSitesUtil::GetInstalledRegisterableDomains(
    Profile* profile) {
  std::set<GURL> installed_origins = GetOriginsWithInstalledWebApps(profile);
  std::set<std::string> registerable_domains;

  for (auto& origin : installed_origins) {
    registerable_domains.emplace(
        ImportantSitesUtil::GetRegisterableDomainOrIP(origin));
  }
  return registerable_domains;
}

std::vector<ImportantDomainInfo>
ImportantSitesUtil::GetImportantRegisterableDomains(Profile* profile,
                                                    size_t max_results) {
  SCOPED_UMA_HISTOGRAM_TIMER("Storage.ImportantSites.GenerationTime");
  std::map<std::string, ImportantDomainInfo> important_info;
  std::map<GURL, double> engagement_map;

  PopulateInfoMapWithEngagement(profile, blink::mojom::EngagementLevel::MEDIUM,
                                &engagement_map, &important_info);

  PopulateInfoMapWithContentTypeAllowed(
      profile, ContentSettingsType::NOTIFICATIONS,
      ImportantReason::NOTIFICATIONS, &important_info);

  PopulateInfoMapWithContentTypeAllowed(
      profile, ContentSettingsType::DURABLE_STORAGE, ImportantReason::DURABLE,
      &important_info);

  PopulateInfoMapWithBookmarks(profile, engagement_map, &important_info);

  std::unordered_set<std::string> suppressed_domains =
      GetSuppressedImportantDomains(profile);

  std::vector<std::pair<std::string, ImportantDomainInfo>> items;
  for (auto& item : important_info)
    items.emplace_back(std::move(item));
  std::sort(items.begin(), items.end(), &CompareDescendingImportantInfo);

  std::vector<ImportantDomainInfo> final_list;
  for (std::pair<std::string, ImportantDomainInfo>& domain_info : items) {
    if (final_list.size() >= max_results)
      return final_list;
    if (suppressed_domains.find(domain_info.first) != suppressed_domains.end())
      continue;

    final_list.push_back(std::move(domain_info.second));
    RECORD_UMA_FOR_IMPORTANT_REASON(
        "Storage.ImportantSites.GeneratedReason",
        "Storage.ImportantSites.GeneratedReasonCount",
        domain_info.second.reason_bitfield);
  }

  return final_list;
}

#if !BUILDFLAG(IS_ANDROID)
std::vector<ImportantDomainInfo>
ImportantSitesUtil::GetInstalledRegisterableDomains(
    browsing_data::TimePeriod time_period,
    Profile* profile,
    size_t max_results) {
  std::vector<ImportantDomainInfo> installed_domains;
  std::map<std::string, ImportantDomainInfo> installed_app_info;
  PopulateInfoMapWithInstalledEngagedInTimePeriod(time_period, profile,
                                                  &installed_app_info);

  std::unordered_set<std::string> excluded_domains =
      GetSuppressedImportantDomains(profile);

  std::vector<std::pair<std::string, ImportantDomainInfo>> items;
  for (auto& item : installed_app_info)
    items.emplace_back(std::move(item));
  std::sort(items.begin(), items.end(), &CompareDescendingImportantInfo);

  for (std::pair<std::string, ImportantDomainInfo>& domain_info : items) {
    if (installed_domains.size() >= max_results)
      break;
    if (excluded_domains.find(domain_info.first) != excluded_domains.end())
      continue;
    installed_domains.push_back(std::move(domain_info.second));
  }
  return installed_domains;
}
#endif

void ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
    Profile* profile,
    const std::vector<std::string>& excluded_sites,
    const std::vector<int32_t>& excluded_sites_reason_bitfield,
    const std::vector<std::string>& ignored_sites,
    const std::vector<int32_t>& ignored_sites_reason_bitfield) {
  // First, record the metrics for excluded and ignored sites.
  for (int32_t reason_bitfield : excluded_sites_reason_bitfield) {
    RECORD_UMA_FOR_IMPORTANT_REASON(
        "Storage.ImportantSites.CBDChosenReason",
        "Storage.ImportantSites.CBDChosenReasonCount", reason_bitfield);
  }
  for (int32_t reason_bitfield : ignored_sites_reason_bitfield) {
    RECORD_UMA_FOR_IMPORTANT_REASON(
        "Storage.ImportantSites.CBDIgnoredReason",
        "Storage.ImportantSites.CBDIgnoredReasonCount", reason_bitfield);
  }

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  // We use the ignored sites to update our ignore counter only if the user
  // chose to exclude a site.
  if (!excluded_sites.empty()) {
    for (const std::string& ignored_site : ignored_sites) {
      GURL origin("http://" + ignored_site);
      base::Value dict = map->GetWebsiteSetting(
          origin, origin, ContentSettingsType::IMPORTANT_SITE_INFO, nullptr);

      if (!dict.is_dict())
        dict = base::Value(base::Value::Type::DICTIONARY);

      RecordIgnore(dict.GetDict());

      map->SetWebsiteSettingDefaultScope(
          origin, origin, ContentSettingsType::IMPORTANT_SITE_INFO,
          std::move(dict));
    }
  } else {
    // Record that the user did not interact with the dialog.
    PrefService* service = profile->GetPrefs();
    ScopedDictPrefUpdate update(service, prefs::kImportantSitesDialogHistory);
    RecordIgnore(update.Get());
  }

  // We clear our ignore counter for sites that the user chose.
  for (const std::string& excluded_site : excluded_sites) {
    GURL origin("http://" + excluded_site);
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetIntKey(kNumTimesIgnoredName, 0);
    dict.RemoveKey(kTimeLastIgnored);
    map->SetWebsiteSettingDefaultScope(origin, origin,
                                       ContentSettingsType::IMPORTANT_SITE_INFO,
                                       std::move(dict));
  }

  // Finally, record our old crossed-stats.
  // Note: we don't plan on adding new metrics here, this is just for the finch
  // experiment to give us initial data on what signals actually mattered.
  for (int32_t reason_bitfield : excluded_sites_reason_bitfield) {
    UMA_HISTOGRAM_ENUMERATION("Storage.BlacklistedImportantSites.Reason",
                              GetCrossedReasonFromBitfield(reason_bitfield),
                              CROSSED_REASON_BOUNDARY);
  }
}

void ImportantSitesUtil::MarkOriginAsImportantForTesting(Profile* profile,
                                                         const GURL& origin) {
  SiteEngagementScore::SetParamValuesForTesting();
  // First get data from site engagement.
  SiteEngagementService* site_engagement_service =
      SiteEngagementService::Get(profile);
  site_engagement_service->ResetBaseScoreForURL(
      origin, SiteEngagementScore::GetMediumEngagementBoundary());
  double score = site_engagement_service->GetScore(origin);
  DCHECK(SiteEngagementService::IsEngagementAtLeast(
      score, blink::mojom::EngagementLevel::MEDIUM));
}

}  // namespace site_engagement

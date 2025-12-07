// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/external_app_redirect_checking.h"

#include "base/json/values_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace safe_browsing {

namespace {

constexpr base::TimeDelta kRecentVisitThreshold = base::Days(30);

// Keys in base::Value::Dict cannot contain periods.
std::string SanitizeAppName(std::string_view app_name) {
  std::string sanitized;
  sanitized.reserve(app_name.size());
  for (char c : app_name) {
    if (c != '.') {
      sanitized += c;
    } else {
      sanitized += '_';
    }
  }

  return sanitized;
}

bool HasRecentAppVisit(PrefService& prefs, std::string_view app_name) {
  const base::Value::Dict& timestamps =
      prefs.GetDict(prefs::kExternalAppRedirectTimestamps);
  std::optional<base::Time> app_timestamp =
      base::ValueToTime(timestamps.Find(SanitizeAppName(app_name)));
  if (!app_timestamp) {
    return false;
  }
  return base::Time::Now() - *app_timestamp <= kRecentVisitThreshold;
}

void OnAllowlistCheckComplete(
    base::OnceCallback<void(bool)> callback,
    bool is_allowlisted,
    std::optional<SafeBrowsingDatabaseManager::
                      HighConfidenceAllowlistCheckLoggingDetails>) {
  std::move(callback).Run(!is_allowlisted);
}

}  // namespace

void ShouldReportExternalAppRedirect(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    content::WebContents* web_contents,
    std::string_view app_name,
    std::string_view uri,
    base::OnceCallback<void(bool)> callback) {
  if (uri.empty()) {
    // Always run `callback` asynchronously, to make calling code simpler.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  PrefService& prefs = *profile->GetPrefs();
  if (!IsEnhancedProtectionEnabled(prefs)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  if (!base::FeatureList::IsEnabled(kExternalAppRedirectTelemetry)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  if (app_name.empty() || HasRecentAppVisit(prefs, app_name)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  database_manager->CheckUrlForHighConfidenceAllowlist(
      web_contents->GetLastCommittedURL(),
      base::BindOnce(&OnAllowlistCheckComplete, std::move(callback)));
}

void LogExternalAppRedirectTimestamp(PrefService& prefs,
                                     std::string_view app_name) {
  if (app_name.empty()) {
    return;
  }
  prefs::ScopedDictionaryPrefUpdate update(
      &prefs, prefs::kExternalAppRedirectTimestamps);
  update->Set(SanitizeAppName(app_name), base::TimeToValue(base::Time::Now()));
}

void CleanupExternalAppRedirectTimestamps(PrefService& prefs) {
  std::vector<std::string> to_remove;
  for (const auto [app_name, timestamp_value] :
       prefs.GetDict(prefs::kExternalAppRedirectTimestamps)) {
    std::optional<base::Time> timestamp = base::ValueToTime(timestamp_value);
    if (!timestamp || base::Time::Now() - *timestamp >= kRecentVisitThreshold) {
      to_remove.push_back(app_name);
    }
  }

  prefs::ScopedDictionaryPrefUpdate update(
      &prefs, prefs::kExternalAppRedirectTimestamps);
  for (const std::string& key : to_remove) {
    update->Remove(key);
  }
}

std::unique_ptr<ClientSafeBrowsingReportRequest> MakeExternalAppRedirectReport(
    content::WebContents* web_contents,
    std::string_view uri) {
  constexpr int kReferrerChainUserGestureLimit = 2;

  if (!web_contents) {
    return nullptr;
  }

  auto report = std::make_unique<ClientSafeBrowsingReportRequest>();

  report->set_type(ClientSafeBrowsingReportRequest::EXTERNAL_APP_REDIRECT);
  report->set_url(std::string(uri));
  report->set_page_url(web_contents->GetLastCommittedURL().spec());
  *report->mutable_population() = GetUserPopulationForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  auto* navigation_observer_manager =
      SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());

  navigation_observer_manager->IdentifyReferrerChainByRenderFrameHost(
      web_contents->GetPrimaryMainFrame(), kReferrerChainUserGestureLimit,
      report->mutable_referrer_chain());
  return report;
}

}  // namespace safe_browsing

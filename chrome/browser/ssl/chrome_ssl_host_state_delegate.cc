// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"

#include <stdint.h>

#include <set>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/common/content_switches.h"
#include "net/base/hash_value.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace {

// Parameters and defaults for the |kRecurrentInterstitialFeature| field trial.

// This parameter controls whether the count of recurrent errors is
// per-browsing-session or persisted to a pref, accumulating across browsing
// sessions. Default is "in-memory".
constexpr char kRecurrentInterstitialModeParam[] = "mode";
constexpr char kRecurrentInterstitialModeInMemory[] = "in-memory";
constexpr char kRecurrentInterstitialModePref[] = "pref";

#if defined(OS_ANDROID)
const base::FeatureParam<std::string> kRecurrentInterstitialMode{
    &kRecurrentInterstitialFeature, kRecurrentInterstitialModeParam,
    kRecurrentInterstitialModePref};
#else
const base::FeatureParam<std::string> kRecurrentInterstitialMode{
    &kRecurrentInterstitialFeature, kRecurrentInterstitialModeParam,
    kRecurrentInterstitialModeInMemory};
#endif

// The number of times an error must recur before the recurrent error message is
// shown.
constexpr char kRecurrentInterstitialThresholdParam[] = "threshold";
constexpr int kRecurrentInterstitialDefaultThreshold = 3;

// If "mode" is "pref", a pref stores the time at which each error most recently
// occurred, and the recurrent error message is shown if the error has recurred
// more than the threshold number of times with the most recent instance being
// less than |kRecurrentInterstitialResetTimeParam| seconds in the past. The
// default is 3 days.
constexpr char kRecurrentInterstitialResetTimeParam[] = "reset-time";
constexpr int kRecurrentInterstitialDefaultResetTime =
    259200;  // 3 days in seconds

// The default expiration for certificate error bypasses is one week, unless
// overidden by a field trial group.  See https://crbug.com/487270.
const uint64_t kDeltaDefaultExpirationInSeconds = UINT64_C(604800);

// Keys for the per-site error + certificate finger to judgment content
// settings map.
const char kSSLCertDecisionCertErrorMapKey[] = "cert_exceptions_map";
const char kSSLCertDecisionExpirationTimeKey[] = "decision_expiration_time";
const char kSSLCertDecisionVersionKey[] = "version";

const int kDefaultSSLCertDecisionVersion = 1;

// Records a new occurrence of |error|. The occurrence is stored in the
// recurrent interstitial pref, which keeps track of the most recent timestamps
// at which each error type occurred (up to the |threshold| most recent
// instances per error). The list is reset if the clock has gone backwards at
// any point.
void UpdateRecurrentInterstitialPref(Profile* profile,
                                     base::Clock* clock,
                                     int error,
                                     int threshold) {
  double now = clock->Now().ToJsTime();

  DictionaryPrefUpdate pref_update(profile->GetPrefs(),
                                   prefs::kRecurrentSSLInterstitial);
  base::Value* list_value =
      pref_update->FindKey(net::ErrorToShortString(error));
  if (list_value) {
    // Check that the values are in increasing order and wipe out the list if
    // not (presumably because the clock changed).
    base::ListValue::ListStorage& error_list = list_value->GetList();
    double previous = 0;
    for (const auto& error_instance : error_list) {
      double error_time = error_instance.GetDouble();
      if (error_time < previous) {
        list_value = nullptr;
        break;
      }
      previous = error_time;
    }
    if (now < previous)
      list_value = nullptr;
  }

  if (!list_value) {
    // Either there was no list of occurrences of this error, or it was corrupt
    // (i.e. out of order). Save a new list composed of just this one error
    // instance.
    base::ListValue error_list;
    error_list.GetList().push_back(base::Value(now));
    pref_update->SetKey(net::ErrorToShortString(error), std::move(error_list));
  } else {
    // Only up to |threshold| values need to be stored. If the list already
    // contains |threshold| values, pop one off the front and append the new one
    // at the end; otherwise just append the new one.
    base::ListValue::ListStorage& error_list = list_value->GetList();
    while (base::MakeStrictNum(error_list.size()) >= threshold) {
      error_list.erase(error_list.begin());
    }
    error_list.push_back(base::Value(now));
    pref_update->SetKey(net::ErrorToShortString(error),
                        base::ListValue(error_list));
  }
}

bool DoesRecurrentInterstitialPrefMeetThreshold(Profile* profile,
                                                base::Clock* clock,
                                                int error,
                                                int threshold) {
  const base::DictionaryValue* pref =
      profile->GetPrefs()->GetDictionary(prefs::kRecurrentSSLInterstitial);
  const base::Value* list_value = pref->FindKey(net::ErrorToShortString(error));
  if (!list_value)
    return false;

  base::Time cutoff_time =
      clock->Now() -
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          kRecurrentInterstitialFeature, kRecurrentInterstitialResetTimeParam,
          kRecurrentInterstitialDefaultResetTime));
  // Assume that the values in the list are in increasing order;
  // UpdateRecurrentInterstitialPref() maintains this ordering. Check if there
  // are more than |threshold| values after the cutoff time.
  const base::ListValue::ListStorage& error_list = list_value->GetList();
  for (size_t i = 0; i < error_list.size(); i++) {
    if (base::Time::FromJsTime(error_list[i].GetDouble()) >= cutoff_time)
      return base::MakeStrictNum(error_list.size() - i) >= threshold;
  }
  return false;
}

void CloseIdleConnections(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
  url_request_context_getter->
      GetURLRequestContext()->
      http_transaction_factory()->
      GetSession()->
      CloseIdleConnections();
}

// All SSL decisions are per host (and are shared arcoss schemes), so this
// canonicalizes all hosts into a secure scheme GURL to use with content
// settings. The returned GURL will be the passed in host with an empty path and
// https:// as the scheme.
GURL GetSecureGURLForHost(const std::string& host) {
  std::string url = "https://" + host;
  return GURL(url);
}

std::string GetKey(const net::X509Certificate& cert, int error) {
  // Since a security decision will be made based on the fingerprint, Chrome
  // should use the SHA-256 fingerprint for the certificate.
  net::SHA256HashValue fingerprint = cert.CalculateChainFingerprint256();
  std::string base64_fingerprint;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(fingerprint.data),
                        sizeof(fingerprint.data)),
      &base64_fingerprint);
  return base::UintToString(error) + base64_fingerprint;
}

void MigrateOldSettings(HostContentSettingsMap* map) {
  // Migrate old settings. Previously SSL would use the same pattern twice,
  // instead of using ContentSettingsPattern::Wildcard(). This has no impact on
  // lookups using GetWebsiteSetting (because Wildcard matches everything) but
  // it has an impact when trying to change the existing content setting. We
  // need to migrate the old-format keys.
  // TODO(raymes): Remove this after ~M51 when clients have migrated. We should
  // leave in some code to remove old-format settings for a long time.
  // crbug.com/569734.
  ContentSettingsForOneType settings;
  map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
                             std::string(), &settings);
  for (const ContentSettingPatternSource& setting : settings) {
    // Migrate user preference settings only.
    if (setting.source != "preference")
      continue;
    // Migrate old-format settings only.
    if (setting.secondary_pattern != ContentSettingsPattern::Wildcard()) {
      GURL url(setting.primary_pattern.ToString());
      // Pull out the value of the old-format setting. Only do this if the
      // patterns are as we expect them to be, otherwise the setting will just
      // be removed for safety.
      std::unique_ptr<base::Value> value;
      if (setting.primary_pattern == setting.secondary_pattern &&
          url.is_valid()) {
        value = map->GetWebsiteSetting(url, url,
                                       CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
                                       std::string(), nullptr);
      }
      // Remove the old pattern.
      map->SetWebsiteSettingCustomScope(
          setting.primary_pattern, setting.secondary_pattern,
          CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, std::string(), nullptr);
      // Set the new pattern.
      if (value) {
        map->SetWebsiteSettingDefaultScope(
            url, GURL(), CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
            std::string(), std::move(value));
      }
    }
  }
}

bool HostFilterToPatternFilter(
    const base::Callback<bool(const std::string&)>& host_filter,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  // We only ever set origin-scoped exceptions which are of the form
  // "https://<host>:443". That is a valid URL, so we can compare |host_filter|
  // against its host.
  GURL url = GURL(primary_pattern.ToString());
  DCHECK(url.is_valid());
  return host_filter.Run(url.host());
}

}  // namespace

const base::Feature kRecurrentInterstitialFeature{
    "RecurrentInterstitialFeature", base::FEATURE_ENABLED_BY_DEFAULT};

ChromeSSLHostStateDelegate::ChromeSSLHostStateDelegate(Profile* profile)
    : clock_(new base::DefaultClock()),
      profile_(profile) {
  MigrateOldSettings(HostContentSettingsMapFactory::GetForProfile(profile));
}

ChromeSSLHostStateDelegate::~ChromeSSLHostStateDelegate() {
}

void ChromeSSLHostStateDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kRecurrentSSLInterstitial);
}

void ChromeSSLHostStateDelegate::AllowCert(const std::string& host,
                                           const net::X509Certificate& cert,
                                           int error) {
  GURL url = GetSecureGURLForHost(host);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::unique_ptr<base::Value> value(map->GetWebsiteSetting(
      url, url, CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, std::string(), NULL));

  if (!value.get() || !value->is_dict())
    value.reset(new base::DictionaryValue());

  base::DictionaryValue* dict;
  bool success = value->GetAsDictionary(&dict);
  DCHECK(success);

  bool expired_previous_decision;  // unused value in this function
  base::DictionaryValue* cert_dict = GetValidCertDecisionsDict(
      dict, CREATE_DICTIONARY_ENTRIES, &expired_previous_decision);
  // If a a valid certificate dictionary cannot be extracted from the content
  // setting, that means it's in an unknown format. Unfortunately, there's
  // nothing to be done in that case, so a silent fail is the only option.
  if (!cert_dict)
    return;

  dict->SetKey(kSSLCertDecisionVersionKey,
               base::Value(kDefaultSSLCertDecisionVersion));
  cert_dict->SetKey(GetKey(cert, error), base::Value(ALLOWED));

  // The map takes ownership of the value, so it is released in the call to
  // SetWebsiteSettingDefaultScope.
  map->SetWebsiteSettingDefaultScope(url, GURL(),
                                     CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
                                     std::string(), std::move(value));
}

void ChromeSSLHostStateDelegate::Clear(
    const base::Callback<bool(const std::string&)>& host_filter) {
  // Convert host matching to content settings pattern matching. Content
  // settings deletion is done synchronously on the UI thread, so we can use
  // |host_filter| by reference.
  HostContentSettingsMap::PatternSourcePredicate pattern_filter;
  if (!host_filter.is_null()) {
    pattern_filter =
        base::Bind(&HostFilterToPatternFilter, base::ConstRef(host_filter));
  }

  HostContentSettingsMapFactory::GetForProfile(profile_)
      ->ClearSettingsForOneTypeWithPredicate(
          CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, base::Time(),
          base::Time::Max(), pattern_filter);
}

content::SSLHostStateDelegate::CertJudgment
ChromeSSLHostStateDelegate::QueryPolicy(const std::string& host,
                                        const net::X509Certificate& cert,
                                        int error,
                                        bool* expired_previous_decision) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  GURL url = GetSecureGURLForHost(host);
  std::unique_ptr<base::Value> value(map->GetWebsiteSetting(
      url, url, CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, std::string(), NULL));

  // Set a default value in case this method is short circuited and doesn't do a
  // full query.
  *expired_previous_decision = false;

  // If the appropriate flag is set, let requests on localhost go
  // through even if there are certificate errors. Errors on localhost
  // are unlikely to indicate actual security problems.
  bool allow_localhost = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAllowInsecureLocalhost);
  if (allow_localhost && net::IsLocalhost(url))
    return ALLOWED;

  if (!value.get() || !value->is_dict())
    return DENIED;

  base::DictionaryValue* dict;  // Owned by value
  int policy_decision;
  bool success = value->GetAsDictionary(&dict);
  DCHECK(success);

  base::DictionaryValue* cert_error_dict;  // Owned by value
  cert_error_dict = GetValidCertDecisionsDict(
      dict, DO_NOT_CREATE_DICTIONARY_ENTRIES, expired_previous_decision);
  if (!cert_error_dict) {
    // This revoke is necessary to clear any old expired setting that may be
    // lingering in the case that an old decision expried.
    RevokeUserAllowExceptions(host);
    return DENIED;
  }

  success = cert_error_dict->GetIntegerWithoutPathExpansion(GetKey(cert, error),
                                                            &policy_decision);

  // If a policy decision was successfully retrieved and it's a valid value of
  // ALLOWED, return the valid value. Otherwise, return DENIED.
  if (success && policy_decision == ALLOWED)
    return ALLOWED;

  return DENIED;
}

void ChromeSSLHostStateDelegate::HostRanInsecureContent(
    const std::string& host,
    int child_id,
    InsecureContentType content_type) {
  switch (content_type) {
    case MIXED_CONTENT:
      ran_mixed_content_hosts_.insert(BrokenHostEntry(host, child_id));
      return;
    case CERT_ERRORS_CONTENT:
      ran_content_with_cert_errors_hosts_.insert(
          BrokenHostEntry(host, child_id));
      return;
  }
}

bool ChromeSSLHostStateDelegate::DidHostRunInsecureContent(
    const std::string& host,
    int child_id,
    InsecureContentType content_type) const {
  auto entry = BrokenHostEntry(host, child_id);
  switch (content_type) {
    case MIXED_CONTENT:
      return base::ContainsKey(ran_mixed_content_hosts_, entry);
    case CERT_ERRORS_CONTENT:
      return base::ContainsKey(ran_content_with_cert_errors_hosts_, entry);
  }
  NOTREACHED();
  return false;
}

void ChromeSSLHostStateDelegate::RevokeUserAllowExceptions(
    const std::string& host) {
  GURL url = GetSecureGURLForHost(host);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  map->SetWebsiteSettingDefaultScope(url, GURL(),
                                     CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
                                     std::string(), nullptr);
}

bool ChromeSSLHostStateDelegate::HasAllowException(
    const std::string& host) const {
  GURL url = GetSecureGURLForHost(host);
  const ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(url);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  std::unique_ptr<base::Value> value(map->GetWebsiteSetting(
      url, url, CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, std::string(), NULL));

  if (!value.get() || !value->is_dict())
    return false;

  base::DictionaryValue* dict;  // Owned by value
  bool success = value->GetAsDictionary(&dict);
  DCHECK(success);

  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    int policy_decision;  // Owned by dict
    success = it.value().GetAsInteger(&policy_decision);
    if (success && (static_cast<CertJudgment>(policy_decision) == ALLOWED))
      return true;
  }

  return false;
}

// TODO(jww): This will revoke all of the decisions in the browser context.
// However, the networking stack actually keeps track of its own list of
// exceptions per-HttpNetworkTransaction in the SSLConfig structure (see the
// allowed_bad_certs Vector in net/ssl/ssl_config.h). This dual-tracking of
// exceptions introduces a problem where the browser context can revoke a
// certificate, but if a transaction reuses a cached version of the SSLConfig
// (probably from a pooled socket), it may bypass the intestitial layer.
//
// Over time, the cached versions should expire and it should converge on
// showing the interstitial. We probably need to introduce into the networking
// stack a way revoke SSLConfig's allowed_bad_certs lists per socket.
//
// For now, RevokeUserAllowExceptionsHard is our solution for the rare case
// where it is necessary to revoke the preferences immediately. It does so by
// flushing idle sockets, thus it is a big hammer and should be wielded with
// extreme caution as it can have a big, negative impact on network performance.
void ChromeSSLHostStateDelegate::RevokeUserAllowExceptionsHard(
    const std::string& host) {
  RevokeUserAllowExceptions(host);
  scoped_refptr<net::URLRequestContextGetter> getter(
      profile_->GetRequestContext());
  getter->GetNetworkTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&CloseIdleConnections, getter));
}

void ChromeSSLHostStateDelegate::DidDisplayErrorPage(int error) {
  if (error != net::ERR_CERT_SYMANTEC_LEGACY &&
      error != net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED) {
    return;
  }

  if (!base::FeatureList::IsEnabled(kRecurrentInterstitialFeature)) {
    return;
  }

  const std::string mode_param = kRecurrentInterstitialMode.Get();
  const int threshold = base::GetFieldTrialParamByFeatureAsInt(
      kRecurrentInterstitialFeature, kRecurrentInterstitialThresholdParam,
      kRecurrentInterstitialDefaultThreshold);

  if (mode_param.empty() || mode_param == kRecurrentInterstitialModeInMemory) {
    const auto count_it = recurrent_errors_.find(error);
    if (count_it == recurrent_errors_.end()) {
      recurrent_errors_[error] = 1;
      return;
    }
    if (count_it->second >= threshold) {
      return;
    }
    recurrent_errors_[error] = count_it->second + 1;
  } else if (mode_param == kRecurrentInterstitialModePref) {
    UpdateRecurrentInterstitialPref(profile_, clock_.get(), error, threshold);
  }
}

bool ChromeSSLHostStateDelegate::HasSeenRecurrentErrors(int error) const {
  if (!base::FeatureList::IsEnabled(kRecurrentInterstitialFeature)) {
    return false;
  }

  const std::string mode_param = kRecurrentInterstitialMode.Get();
  const int threshold = base::GetFieldTrialParamByFeatureAsInt(
      kRecurrentInterstitialFeature, kRecurrentInterstitialThresholdParam,
      kRecurrentInterstitialDefaultThreshold);

  if (mode_param.empty() || mode_param == kRecurrentInterstitialModeInMemory) {
    const auto count_it = recurrent_errors_.find(error);
    if (count_it == recurrent_errors_.end())
      return false;
    return count_it->second >= threshold;
  } else if (mode_param == kRecurrentInterstitialModePref) {
    return DoesRecurrentInterstitialPrefMeetThreshold(profile_, clock_.get(),
                                                      error, threshold);
  }

  return false;
}

void ChromeSSLHostStateDelegate::ResetRecurrentErrorCountForTesting() {
  recurrent_errors_.clear();
  DictionaryPrefUpdate pref_update(profile_->GetPrefs(),
                                   prefs::kRecurrentSSLInterstitial);
  pref_update->Clear();
}

void ChromeSSLHostStateDelegate::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

// This helper function gets the dictionary of certificate fingerprints to
// errors of certificates that have been accepted by the user from the content
// dictionary that has been passed in. The returned pointer is owned by the the
// argument dict that is passed in.
//
// If create_entries is set to |DO_NOT_CREATE_DICTIONARY_ENTRIES|,
// GetValidCertDecisionsDict will return NULL if there is anything invalid about
// the setting, such as an invalid version or invalid value types (in addition
// to there not being any values in the dictionary). If create_entries is set to
// |CREATE_DICTIONARY_ENTRIES|, if no dictionary is found or the decisions are
// expired, a new dictionary will be created.
base::DictionaryValue* ChromeSSLHostStateDelegate::GetValidCertDecisionsDict(
    base::DictionaryValue* dict,
    CreateDictionaryEntriesDisposition create_entries,
    bool* expired_previous_decision) {
  // This needs to be done first in case the method is short circuited by an
  // early failure.
  *expired_previous_decision = false;

  // Extract the version of the certificate decision structure from the content
  // setting.
  int version;
  bool success = dict->GetInteger(kSSLCertDecisionVersionKey, &version);
  if (!success) {
    if (create_entries == DO_NOT_CREATE_DICTIONARY_ENTRIES)
      return NULL;

    dict->SetInteger(kSSLCertDecisionVersionKey,
                     kDefaultSSLCertDecisionVersion);
    version = kDefaultSSLCertDecisionVersion;
  }

  // If the version is somehow a newer version than Chrome can handle, there's
  // really nothing to do other than fail silently and pretend it doesn't exist
  // (or is malformed).
  if (version > kDefaultSSLCertDecisionVersion) {
    LOG(ERROR) << "Failed to parse a certificate error exception that is in a "
               << "newer version format (" << version << ") than is supported ("
               << kDefaultSSLCertDecisionVersion << ")";
    return NULL;
  }

  // Extract the certificate decision's expiration time from the content
  // setting. If there is no expiration time, that means it should never expire
  // and it should reset only at session restart, so skip all of the expiration
  // checks.
  bool expired = false;
  base::Time now = clock_->Now();
  base::Time decision_expiration;
  if (dict->HasKey(kSSLCertDecisionExpirationTimeKey)) {
    std::string decision_expiration_string;
    int64_t decision_expiration_int64;
    success = dict->GetString(kSSLCertDecisionExpirationTimeKey,
                              &decision_expiration_string);
    if (!base::StringToInt64(base::StringPiece(decision_expiration_string),
                             &decision_expiration_int64)) {
      LOG(ERROR) << "Failed to parse a certificate error exception that has a "
                 << "bad value for an expiration time: "
                 << decision_expiration_string;
      return NULL;
    }
    decision_expiration =
        base::Time::FromInternalValue(decision_expiration_int64);
  }

  // Check to see if the user's certificate decision has expired.
  // - Expired and |create_entries| is DO_NOT_CREATE_DICTIONARY_ENTRIES, return
  // NULL.
  // - Expired and |create_entries| is CREATE_DICTIONARY_ENTRIES, update the
  // expiration time.
  if (decision_expiration.ToInternalValue() <= now.ToInternalValue()) {
    *expired_previous_decision = true;

    if (create_entries == DO_NOT_CREATE_DICTIONARY_ENTRIES)
      return NULL;

    expired = true;
    base::Time expiration_time =
        now + base::TimeDelta::FromSeconds(kDeltaDefaultExpirationInSeconds);
    // Unfortunately, JSON (and thus content settings) doesn't support int64_t
    // values, only doubles. Since this mildly depends on precision, it is
    // better to store the value as a string.
    dict->SetString(kSSLCertDecisionExpirationTimeKey,
                    base::Int64ToString(expiration_time.ToInternalValue()));
  }

  // Extract the map of certificate fingerprints to errors from the setting.
  base::DictionaryValue* cert_error_dict = NULL;  // Will be owned by dict
  if (expired ||
      !dict->GetDictionary(kSSLCertDecisionCertErrorMapKey, &cert_error_dict)) {
    if (create_entries == DO_NOT_CREATE_DICTIONARY_ENTRIES)
      return NULL;

    cert_error_dict =
        dict->SetDictionary(kSSLCertDecisionCertErrorMapKey,
                            std::make_unique<base::DictionaryValue>());
  }

  return cert_error_dict;
}

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/counters/cache_counter.h"
#include "chrome/browser/browsing_data/counters/signin_data_counter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browsing_data/counters/hosted_apps_counter.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/sync/sync_ui_util.h"
#endif

namespace browsing_data_counter_utils {

using BrowsingDataCounter = browsing_data::BrowsingDataCounter;
using SigninDataCounter = browsing_data::SigninDataCounter;
using ResultInt = browsing_data::BrowsingDataCounter::ResultInt;

namespace {
// A helper function to display the size of cache in units of MB or higher.
// We need this, as 1 MB is the lowest nonzero cache size displayed by the
// counter.
std::u16string FormatBytesMBOrHigher(ResultInt bytes) {
  if (ui::GetByteDisplayUnits(bytes) >= ui::DataUnits::DATA_UNITS_MEBIBYTE)
    return ui::FormatBytes(bytes);

  return ui::FormatBytesWithUnits(
      bytes, ui::DataUnits::DATA_UNITS_MEBIBYTE, true);
}
}  // namespace

bool ShouldShowCookieException(Profile* profile) {
  if (AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile)) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    return identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  }
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile)) {
    return GetSyncStatusMessageType(profile) == SyncStatusMessageType::kSynced;
  }
#endif
  return false;
}

std::u16string GetChromeCounterTextFromResult(
    const BrowsingDataCounter::Result* result,
    Profile* profile) {
  std::string pref_name = result->source()->GetPrefName();

  if (!result->Finished()) {
    // The counter is still counting.
    return l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_CALCULATING);
  }

  if (pref_name == browsing_data::prefs::kDeleteCache ||
      pref_name == browsing_data::prefs::kDeleteCacheBasic) {
    // Cache counter.
    const auto* cache_result =
        static_cast<const CacheCounter::CacheResult*>(result);
    int64_t cache_size_bytes = cache_result->cache_size();
    bool is_upper_limit = cache_result->is_upper_limit();
    bool is_basic_tab = pref_name == browsing_data::prefs::kDeleteCacheBasic;

    // Three cases: Nonzero result for the entire cache, nonzero result for
    // a subset of cache (i.e. a finite time interval), and almost zero (< 1MB).
    static const int kBytesInAMegabyte = 1024 * 1024;
    if (cache_size_bytes >= kBytesInAMegabyte) {
      std::u16string formatted_size = FormatBytesMBOrHigher(cache_size_bytes);
      if (!is_upper_limit) {
        return is_basic_tab ? l10n_util::GetStringFUTF16(
                                  IDS_DEL_CACHE_COUNTER_BASIC, formatted_size)
                            : formatted_size;
      }
      return l10n_util::GetStringFUTF16(
          is_basic_tab ? IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE_BASIC
                       : IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
          formatted_size);
    }
    return l10n_util::GetStringUTF16(
        is_basic_tab ? IDS_DEL_CACHE_COUNTER_ALMOST_EMPTY_BASIC
                     : IDS_DEL_CACHE_COUNTER_ALMOST_EMPTY);
  }
  if (pref_name == browsing_data::prefs::kDeleteCookiesBasic) {
    // The basic tab doesn't show cookie counter results.
    NOTREACHED();
  }
  if (pref_name == browsing_data::prefs::kDeleteCookies) {
    // Site data counter.
    ResultInt origins =
        static_cast<const BrowsingDataCounter::FinishedResult*>(result)
            ->Value();

    // Determines whether or not to show the count with exception message.
    int del_cookie_counter_msg_id =
        ShouldShowCookieException(profile)
            ? IDS_DEL_COOKIES_COUNTER_ADVANCED_WITH_EXCEPTION
            : IDS_DEL_COOKIES_COUNTER_ADVANCED;

    return l10n_util::GetPluralStringFUTF16(del_cookie_counter_msg_id, origins);
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (pref_name == browsing_data::prefs::kDeleteHostedAppsData) {
    // Hosted apps counter.
    const HostedAppsCounter::HostedAppsResult* hosted_apps_result =
        static_cast<const HostedAppsCounter::HostedAppsResult*>(result);
    int hosted_apps_count = hosted_apps_result->Value();

    DCHECK_GE(
        hosted_apps_result->Value(),
        base::checked_cast<ResultInt>(hosted_apps_result->examples().size()));

    std::vector<std::u16string> replacements;
    if (hosted_apps_count > 0) {
      replacements.push_back(                                     // App1,
          base::UTF8ToUTF16(hosted_apps_result->examples()[0]));
    }
    if (hosted_apps_count > 1) {
      replacements.push_back(
          base::UTF8ToUTF16(hosted_apps_result->examples()[1]));  // App2,
    }
    if (hosted_apps_count > 2) {
      replacements.push_back(l10n_util::GetPluralStringFUTF16(  // and X-2 more.
          IDS_DEL_HOSTED_APPS_COUNTER_AND_X_MORE,
          hosted_apps_count - 2));
    }

    // The output string has both the number placeholder (#) and substitution
    // placeholders ($1, $2, $3). First fetch the correct plural string first,
    // then substitute the $ placeholders.
    return base::ReplaceStringPlaceholders(
        l10n_util::GetPluralStringFUTF16(
            IDS_DEL_HOSTED_APPS_COUNTER, hosted_apps_count),
        replacements,
        nullptr);
  }
#endif

  if (pref_name == browsing_data::prefs::kDeletePasswords) {
    auto* signin_result =
        static_cast<const SigninDataCounter::SigninDataResult*>(result);

    ResultInt profile_passwords = signin_result->Value();
    ResultInt account_passwords = signin_result->account_passwords();
    ResultInt signin_data_count = signin_result->WebAuthnCredentialsValue();

    std::vector<std::u16string> counts;
    // TODO(crbug.com/1086433): If there are profile passwords, account
    // passwords and other sign-in data, these are combined as
    // "<1>; <2>; <3>" by recursively applying a "<1>; <2>" message.
    // Maybe we should do something more pretty?
    if (profile_passwords || account_passwords) {
      counts.emplace_back(browsing_data::GetCounterTextFromResult(result));
    }
    if (signin_data_count) {
      counts.emplace_back(l10n_util::GetPluralStringFUTF16(
          IDS_DEL_SIGNIN_DATA_COUNTER, signin_data_count));
    }
    switch (counts.size()) {
      case 0:
        return l10n_util::GetStringUTF16(
            IDS_DEL_PASSWORDS_AND_SIGNIN_DATA_COUNTER_NONE);
      case 1:
        return counts[0];
      case 2:
        return l10n_util::GetStringFUTF16(
            IDS_DEL_PASSWORDS_AND_SIGNIN_DATA_COUNTER_COMBINATION, counts[0],
            counts[1]);
      default:
        NOTREACHED();
    }
    NOTREACHED();
  }

  return browsing_data::GetCounterTextFromResult(result);
}

}  // namespace browsing_data_counter_utils

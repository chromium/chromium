// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions BrowsingData API functions, which entail
// clearing browsing data, and clearing the browser's cache (which, let's be
// honest, are the same thing), as specified in the extension API JSON.

#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/plugins/plugin_data_remover_helper.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using content::BrowserThread;
using browsing_data::ClearBrowsingDataTab;
using browsing_data::BrowsingDataType;

namespace extension_browsing_data_api_constants {
// Parameter name keys.
const char kDataRemovalPermittedKey[] = "dataRemovalPermitted";
const char kDataToRemoveKey[] = "dataToRemove";
const char kOptionsKey[] = "options";

// Type keys.
const char kAppCacheKey[] = "appcache";
const char kCacheKey[] = "cache";
const char kCookiesKey[] = "cookies";
const char kDownloadsKey[] = "downloads";
const char kFileSystemsKey[] = "fileSystems";
const char kFormDataKey[] = "formData";
const char kHistoryKey[] = "history";
const char kIndexedDBKey[] = "indexedDB";
const char kLocalStorageKey[] = "localStorage";
const char kPasswordsKey[] = "passwords";
const char kPluginDataKey[] = "pluginData";
const char kServiceWorkersKey[] = "serviceWorkers";
const char kCacheStorageKey[] = "cacheStorage";
const char kWebSQLKey[] = "webSQL";

// Option keys.
const char kExtensionsKey[] = "extension";
const char kOriginTypesKey[] = "originTypes";
const char kProtectedWebKey[] = "protectedWeb";
const char kSinceKey[] = "since";
const char kOriginsKey[] = "origins";
const char kExcludeOriginsKey[] = "excludeOrigins";
const char kUnprotectedWebKey[] = "unprotectedWeb";

// Errors!
// The placeholder will be filled by the name of the affected data type (e.g.,
// "history").
const char kBadDataTypeDetails[] = "Invalid value for data type '%s'.";
const char kDeleteProhibitedError[] =
    "Browsing history and downloads are not "
    "permitted to be removed.";
const char kNonFilterableError[] =
    "At least one data type doesn't support filtering by origin.";
const char kIncompatibleFilterError[] =
    "Don't set both 'origins' and 'excludeOrigins' at the same time.";
const char kInvalidOriginError[] = "'%s' is not a valid origin.";

}  // namespace extension_browsing_data_api_constants

namespace {

const int64_t kFilterableDataTypes =
    ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA |
    content::BrowsingDataRemover::DATA_TYPE_CACHE;

static_assert((kFilterableDataTypes &
               ~ChromeBrowsingDataRemoverDelegate::FILTERABLE_DATA_TYPES) == 0,
              "kFilterableDataTypes must be a subset of "
              "ChromeBrowsingDataRemoverDelegate::FILTERABLE_DATA_TYPES");

int MaskForKey(const char* key) {
  if (strcmp(key, extension_browsing_data_api_constants::kAppCacheKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_APP_CACHE;
  if (strcmp(key, extension_browsing_data_api_constants::kCacheKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_CACHE;
  if (strcmp(key, extension_browsing_data_api_constants::kCookiesKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_COOKIES;
  if (strcmp(key, extension_browsing_data_api_constants::kDownloadsKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS;
  if (strcmp(key, extension_browsing_data_api_constants::kFileSystemsKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS;
  if (strcmp(key, extension_browsing_data_api_constants::kFormDataKey) == 0)
    return ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA;
  if (strcmp(key, extension_browsing_data_api_constants::kHistoryKey) == 0)
    return ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY;
  if (strcmp(key, extension_browsing_data_api_constants::kIndexedDBKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_INDEXED_DB;
  if (strcmp(key, extension_browsing_data_api_constants::kLocalStorageKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE;
  if (strcmp(key, extension_browsing_data_api_constants::kPasswordsKey) == 0)
    return ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS;
  if (strcmp(key, extension_browsing_data_api_constants::kPluginDataKey) == 0)
    return ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA;
  if (strcmp(key, extension_browsing_data_api_constants::kServiceWorkersKey) ==
      0)
    return content::BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS;
  if (strcmp(key, extension_browsing_data_api_constants::kCacheStorageKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE;
  if (strcmp(key, extension_browsing_data_api_constants::kWebSQLKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_WEB_SQL;

  return 0;
}

// Returns false if any of the selected data types are not allowed to be
// deleted.
bool IsRemovalPermitted(int removal_mask, PrefService* prefs) {
  // Enterprise policy or user preference might prohibit deleting browser or
  // download history.
  if ((removal_mask & ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY) ||
      (removal_mask & content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS)) {
    return prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory);
  }
  return true;
}

// Returns true if Sync is currently running (i.e. enabled and not in error).
bool IsSyncRunning(Profile* profile) {
  return sync_ui_util::GetStatus(profile) == sync_ui_util::SYNCED;
}
}  // namespace

bool BrowsingDataSettingsFunction::isDataTypeSelected(
    BrowsingDataType data_type,
    ClearBrowsingDataTab tab) {
  std::string pref_name;
  bool success = GetDeletionPreferenceFromDataType(data_type, tab, &pref_name);
  return success && prefs_->GetBoolean(pref_name);
}

ExtensionFunction::ResponseAction BrowsingDataSettingsFunction::Run() {
  prefs_ = Profile::FromBrowserContext(browser_context())->GetPrefs();

  ClearBrowsingDataTab tab = static_cast<ClearBrowsingDataTab>(
      prefs_->GetInteger(browsing_data::prefs::kLastClearBrowsingDataTab));

  // Fill origin types.
  // The "cookies" and "hosted apps" UI checkboxes both map to
  // REMOVE_SITE_DATA in browsing_data_remover.h, the former for the unprotected
  // web, the latter for  protected web data. There is no UI control for
  // extension data.
  std::unique_ptr<base::DictionaryValue> origin_types(
      new base::DictionaryValue);
  origin_types->SetBoolean(
      extension_browsing_data_api_constants::kUnprotectedWebKey,
      isDataTypeSelected(BrowsingDataType::COOKIES, tab));
  origin_types->SetBoolean(
      extension_browsing_data_api_constants::kProtectedWebKey,
      isDataTypeSelected(BrowsingDataType::HOSTED_APPS_DATA, tab));
  origin_types->SetBoolean(
      extension_browsing_data_api_constants::kExtensionsKey, false);

  // Fill deletion time period.
  int period_pref =
      prefs_->GetInteger(browsing_data::GetTimePeriodPreferenceName(tab));

  browsing_data::TimePeriod period =
      static_cast<browsing_data::TimePeriod>(period_pref);
  double since = 0;
  if (period != browsing_data::TimePeriod::ALL_TIME) {
    base::Time time = browsing_data::CalculateBeginDeleteTime(period);
    since = time.ToJsTime();
  }

  std::unique_ptr<base::DictionaryValue> options(new base::DictionaryValue);
  options->Set(extension_browsing_data_api_constants::kOriginTypesKey,
               std::move(origin_types));
  options->SetDouble(extension_browsing_data_api_constants::kSinceKey, since);

  // Fill dataToRemove and dataRemovalPermitted.
  std::unique_ptr<base::DictionaryValue> selected(new base::DictionaryValue);
  std::unique_ptr<base::DictionaryValue> permitted(new base::DictionaryValue);

  bool delete_site_data =
      isDataTypeSelected(BrowsingDataType::COOKIES, tab) ||
      isDataTypeSelected(BrowsingDataType::HOSTED_APPS_DATA, tab);

  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kAppCacheKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kCookiesKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kFileSystemsKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kIndexedDBKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
      extension_browsing_data_api_constants::kLocalStorageKey,
      delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kWebSQLKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kServiceWorkersKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kCacheStorageKey,
             delete_site_data);

  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kPluginDataKey,
             delete_site_data &&
                 prefs_->GetBoolean(prefs::kClearPluginLSODataEnabled));

  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kHistoryKey,
             isDataTypeSelected(BrowsingDataType::HISTORY, tab));
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kDownloadsKey,
             isDataTypeSelected(BrowsingDataType::DOWNLOADS, tab));
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kCacheKey,
             isDataTypeSelected(BrowsingDataType::CACHE, tab));
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kFormDataKey,
             isDataTypeSelected(BrowsingDataType::FORM_DATA, tab));
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kPasswordsKey,
             isDataTypeSelected(BrowsingDataType::PASSWORDS, tab));

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  result->Set(extension_browsing_data_api_constants::kOptionsKey,
              std::move(options));
  result->Set(extension_browsing_data_api_constants::kDataToRemoveKey,
              std::move(selected));
  result->Set(extension_browsing_data_api_constants::kDataRemovalPermittedKey,
              std::move(permitted));
  return RespondNow(OneArgument(std::move(result)));
}

void BrowsingDataSettingsFunction::SetDetails(
    base::DictionaryValue* selected_dict,
    base::DictionaryValue* permitted_dict,
    const char* data_type,
    bool is_selected) {
  bool is_permitted = IsRemovalPermitted(MaskForKey(data_type), prefs_);
  selected_dict->SetBoolean(data_type, is_selected && is_permitted);
  permitted_dict->SetBoolean(data_type, is_permitted);
}

BrowsingDataRemoverFunction::BrowsingDataRemoverFunction() : observer_(this) {}

void BrowsingDataRemoverFunction::OnBrowsingDataRemoverDone() {
  OnTaskFinished();
}

void BrowsingDataRemoverFunction::OnTaskFinished() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GT(pending_tasks_, 0);
  if (--pending_tasks_ > 0)
    return;
  synced_data_deletion_.reset();
  observer_.RemoveAll();
  this->SendResponse(true);
  Release();  // Balanced in StartRemoving.
}

bool BrowsingDataRemoverFunction::RunAsync() {
  // If we don't have a profile, something's pretty wrong.
  DCHECK(GetProfile());

  // Grab the initial |options| parameter, and parse out the arguments.
  base::DictionaryValue* options;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &options));
  DCHECK(options);

  EXTENSION_FUNCTION_VALIDATE(
      ParseOriginTypeMask(*options, &origin_type_mask_));

  // If |ms_since_epoch| isn't set, default it to 0.
  double ms_since_epoch;
  if (!options->GetDouble(extension_browsing_data_api_constants::kSinceKey,
                          &ms_since_epoch)) {
    ms_since_epoch = 0;
  }
  // base::Time takes a double that represents seconds since epoch. JavaScript
  // gives developers milliseconds, so do a quick conversion before populating
  // the object.
  remove_since_ = base::Time::FromJsTime(ms_since_epoch);

  EXTENSION_FUNCTION_VALIDATE(GetRemovalMask(&removal_mask_));

  base::Value* origins =
      options->FindKeyOfType(extension_browsing_data_api_constants::kOriginsKey,
                             base::Value::Type::LIST);
  base::Value* exclude_origins = options->FindKeyOfType(
      extension_browsing_data_api_constants::kExcludeOriginsKey,
      base::Value::Type::LIST);

  // Check that only |origins| or |excludeOrigins| can be set.
  if (origins && exclude_origins) {
    error_ = extension_browsing_data_api_constants::kIncompatibleFilterError;
    return false;
  }

  if (origins) {
    if (!ParseOrigins(*origins, &origins_))
      return false;
    mode_ = content::BrowsingDataFilterBuilder::WHITELIST;
  } else {
    if (exclude_origins && !ParseOrigins(*exclude_origins, &origins_))
      return false;
    mode_ = content::BrowsingDataFilterBuilder::BLACKLIST;
  }

  // Check if a filter is set but non-filterable types are selected.
  if ((!origins_.empty() && (removal_mask_ & ~kFilterableDataTypes) != 0)) {
    error_ = extension_browsing_data_api_constants::kNonFilterableError;
    return false;
  }

  // Check for prohibited data types.
  if (!IsRemovalPermitted(removal_mask_, GetProfile()->GetPrefs())) {
    error_ = extension_browsing_data_api_constants::kDeleteProhibitedError;
    return false;
  }

  if (removal_mask_ &
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA) {
    // If we're being asked to remove plugin data, check whether it's actually
    // supported.
    PostTask(FROM_HERE,
             {base::ThreadPool(), base::MayBlock(),
              base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
              base::TaskPriority::USER_VISIBLE},
             base::BindOnce(
                 &BrowsingDataRemoverFunction::CheckRemovingPluginDataSupported,
                 this, PluginPrefs::GetForProfile(GetProfile())));
  } else {
    StartRemoving();
  }

  // Will finish asynchronously.
  return true;
}

BrowsingDataRemoverFunction::~BrowsingDataRemoverFunction() {}

bool BrowsingDataRemoverFunction::IsPauseSyncAllowed() {
  return true;
}

void BrowsingDataRemoverFunction::CheckRemovingPluginDataSupported(
    scoped_refptr<PluginPrefs> plugin_prefs) {
  if (!PluginDataRemoverHelper::IsSupported(plugin_prefs.get()))
    removal_mask_ &= ~ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA;

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&BrowsingDataRemoverFunction::StartRemoving, this));
}

void BrowsingDataRemoverFunction::StartRemoving() {
  Profile* profile = GetProfile();
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(profile);

  // Add a ref (Balanced in OnTaskFinished)
  AddRef();

  // Prevent Sync from being paused, if required.
  DCHECK(!synced_data_deletion_);
  if (!IsPauseSyncAllowed() && IsSyncRunning(profile)) {
    synced_data_deletion_ = AccountReconcilorFactory::GetForProfile(profile)
                                ->GetScopedSyncDataDeletion();
  }

  // Create a BrowsingDataRemover, set the current object as an observer (so
  // that we're notified after removal) and call remove() with the arguments
  // we've generated above. We can use a raw pointer here, as the browsing data
  // remover is responsible for deleting itself once data removal is complete.
  observer_.Add(remover);

  DCHECK_EQ(pending_tasks_, 0);
  pending_tasks_ = 1;
  if (removal_mask_ & content::BrowsingDataRemover::DATA_TYPE_COOKIES &&
      !origins_.empty()) {
    pending_tasks_ += 1;
    removal_mask_ &= ~content::BrowsingDataRemover::DATA_TYPE_COOKIES;
    // Cookies are scoped more broadly than origins, so we expand the
    // origin filter to registrable domains in order to match all cookies
    // that could be applied to an origin. This is the same behavior as
    // the Clear-Site-Data header.
    auto filter_builder = content::BrowsingDataFilterBuilder::Create(mode_);
    for (const auto& origin : origins_) {
      std::string domain = GetDomainAndRegistry(
          origin.host(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
      if (domain.empty())
        domain = origin.host();  // IP address or internal hostname.
      filter_builder->AddRegisterableDomain(domain);
    }
    remover->RemoveWithFilterAndReply(
        remove_since_, base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_COOKIES, origin_type_mask_,
        std::move(filter_builder), this);
  }
  if (removal_mask_) {
    pending_tasks_ += 1;
    auto filter_builder = content::BrowsingDataFilterBuilder::Create(mode_);
    for (const auto& origin : origins_) {
      filter_builder->AddOrigin(origin);
    }
    remover->RemoveWithFilterAndReply(remove_since_, base::Time::Max(),
                                      removal_mask_, origin_type_mask_,
                                      std::move(filter_builder), this);
  }
  OnTaskFinished();
}

bool BrowsingDataRemoverFunction::ParseOriginTypeMask(
    const base::DictionaryValue& options,
    int* origin_type_mask) {
  // Parse the |options| dictionary to generate the origin set mask. Default to
  // UNPROTECTED_WEB if the developer doesn't specify anything.
  *origin_type_mask = content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;

  const base::DictionaryValue* d = NULL;
  if (options.HasKey(extension_browsing_data_api_constants::kOriginTypesKey)) {
    if (!options.GetDictionary(
            extension_browsing_data_api_constants::kOriginTypesKey, &d)) {
      return false;
    }
    bool value;

    // The developer specified something! Reset to 0 and parse the dictionary.
    *origin_type_mask = 0;

    // Unprotected web.
    if (d->HasKey(extension_browsing_data_api_constants::kUnprotectedWebKey)) {
      if (!d->GetBoolean(
              extension_browsing_data_api_constants::kUnprotectedWebKey,
              &value)) {
        return false;
      }
      *origin_type_mask |=
          value ? content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB : 0;
    }

    // Protected web.
    if (d->HasKey(extension_browsing_data_api_constants::kProtectedWebKey)) {
      if (!d->GetBoolean(
              extension_browsing_data_api_constants::kProtectedWebKey,
              &value)) {
        return false;
      }
      *origin_type_mask |=
          value ? content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB : 0;
    }

    // Extensions.
    if (d->HasKey(extension_browsing_data_api_constants::kExtensionsKey)) {
      if (!d->GetBoolean(extension_browsing_data_api_constants::kExtensionsKey,
                         &value)) {
        return false;
      }
      *origin_type_mask |=
          value ? ChromeBrowsingDataRemoverDelegate::ORIGIN_TYPE_EXTENSION : 0;
    }
  }

  return true;
}

bool BrowsingDataRemoverFunction::ParseOrigins(
    const base::Value& list_value,
    std::vector<url::Origin>* result) {
  DCHECK(list_value.is_list());
  result->reserve(list_value.GetList().size());
  for (const auto& value : list_value.GetList()) {
    EXTENSION_FUNCTION_VALIDATE(value.is_string());
    url::Origin origin = url::Origin::Create(GURL(value.GetString()));
    if (origin.opaque()) {
      error_ = base::StringPrintf(
          extension_browsing_data_api_constants::kInvalidOriginError,
          value.GetString().c_str());
      return false;
    }
    result->push_back(std::move(origin));
  }
  return true;
}

// Parses the |dataToRemove| argument to generate the removal mask.
// Returns false if parse was not successful, i.e. if 'dataToRemove' is not
// present or any data-type keys don't have supported (boolean) values.
bool BrowsingDataRemoveFunction::GetRemovalMask(int* removal_mask) {
  base::DictionaryValue* data_to_remove;
  if (!args_->GetDictionary(1, &data_to_remove))
    return false;

  *removal_mask = 0;
  for (base::DictionaryValue::Iterator i(*data_to_remove);
       !i.IsAtEnd();
       i.Advance()) {
    bool selected = false;
    if (!i.value().GetAsBoolean(&selected))
      return false;
    if (selected)
      *removal_mask |= MaskForKey(i.key().c_str());
  }

  return true;
}

bool BrowsingDataRemoveFunction::IsPauseSyncAllowed() {
  return false;
}

bool BrowsingDataRemoveAppcacheFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_APP_CACHE;
  return true;
}

bool BrowsingDataRemoveCacheFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_CACHE;
  return true;
}

bool BrowsingDataRemoveCookiesFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_COOKIES;
  return true;
}

bool BrowsingDataRemoveDownloadsFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS;
  return true;
}

bool BrowsingDataRemoveFileSystemsFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS;
  return true;
}

bool BrowsingDataRemoveFormDataFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA;
  return true;
}

bool BrowsingDataRemoveHistoryFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY;
  return true;
}

bool BrowsingDataRemoveIndexedDBFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_INDEXED_DB;
  return true;
}

bool BrowsingDataRemoveLocalStorageFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE;
  return true;
}

bool BrowsingDataRemovePluginDataFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA;
  return true;
}

bool BrowsingDataRemovePasswordsFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS;
  return true;
}

bool BrowsingDataRemoveServiceWorkersFunction::GetRemovalMask(
    int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS;
  return true;
}

bool BrowsingDataRemoveCacheStorageFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE;
  return true;
}

bool BrowsingDataRemoveWebSQLFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_WEB_SQL;
  return true;
}

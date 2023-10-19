// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions BrowsingData API functions, which entail
// clearing browsing data, and clearing the browser's cache (which, let's be
// honest, are the same thing), as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BROWSING_DATA_BROWSING_DATA_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BROWSING_DATA_BROWSING_DATA_API_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "extensions/browser/extension_function.h"

class PrefService;

namespace extension_browsing_data_api_constants {

// Parameter name keys.
inline constexpr char kDataRemovalPermittedKey[] = "dataRemovalPermitted";
inline constexpr char kDataToRemoveKey[] = "dataToRemove";
inline constexpr char kOptionsKey[] = "options";

// Type keys.
inline constexpr char kCacheKey[] = "cache";
inline constexpr char kCookiesKey[] = "cookies";
inline constexpr char kDownloadsKey[] = "downloads";
inline constexpr char kFileSystemsKey[] = "fileSystems";
inline constexpr char kFormDataKey[] = "formData";
inline constexpr char kHistoryKey[] = "history";
inline constexpr char kIndexedDBKey[] = "indexedDB";
inline constexpr char kLocalStorageKey[] = "localStorage";
inline constexpr char kPasswordsKey[] = "passwords";
inline constexpr char kPluginDataKeyDeprecated[] = "pluginData";
inline constexpr char kServiceWorkersKey[] = "serviceWorkers";
inline constexpr char kCacheStorageKey[] = "cacheStorage";
inline constexpr char kWebSQLKey[] = "webSQL";

// Option keys.
inline constexpr char kExtensionsKey[] = "extension";
inline constexpr char kOriginTypesKey[] = "originTypes";
inline constexpr char kProtectedWebKey[] = "protectedWeb";
inline constexpr char kSinceKey[] = "since";
inline constexpr char kOriginsKey[] = "origins";
inline constexpr char kExcludeOriginsKey[] = "excludeOrigins";
inline constexpr char kUnprotectedWebKey[] = "unprotectedWeb";

// Errors!
// The placeholder will be filled by the name of the affected data type (e.g.,
// "history").
inline constexpr char kBadDataTypeDetails[] =
    "Invalid value for data type '%s'.";
inline constexpr char kDeleteProhibitedError[] =
    "Browsing history and downloads are not "
    "permitted to be removed.";
inline constexpr char kNonFilterableError[] =
    "At least one data type doesn't support filtering by origin.";
inline constexpr char kIncompatibleFilterError[] =
    "Don't set both 'origins' and 'excludeOrigins' at the same time.";
inline constexpr char kInvalidOriginError[] = "'%s' is not a valid origin.";

}  // namespace extension_browsing_data_api_constants

class BrowsingDataSettingsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.settings", BROWSINGDATA_SETTINGS)

  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~BrowsingDataSettingsFunction() override = default;

 private:
  // Sets a boolean value in the |selected_dict| with the |data_type| as a key,
  // indicating whether the data type is both selected and permitted to be
  // removed; and a value in the |permitted_dict| with the |data_type| as a
  // key, indicating only whether the data type is permitted to be removed.
  void SetDetails(base::Value::Dict* selected_dict,
                  base::Value::Dict* permitted_dict,
                  const char* data_type,
                  bool is_selected);

  // Returns whether |data_type| is currently selected for deletion on |tab|.
  bool isDataTypeSelected(browsing_data::BrowsingDataType data_type,
                          browsing_data::ClearBrowsingDataTab tab);

  raw_ptr<PrefService> prefs_ = nullptr;
};

// This serves as a base class from which the browsing data API removal
// functions will inherit. Each needs to be an observer of BrowsingDataRemover
// events, and each will handle those events in the same way (by calling the
// passed-in callback function).
//
// Each child class must implement GetRemovalMask(), which returns the bitmask
// of data types to remove.
class BrowsingDataRemoverFunction
    : public ExtensionFunction,
      public content::BrowsingDataRemover::Observer {
 public:
  BrowsingDataRemoverFunction();

  // BrowsingDataRemover::Observer interface method.
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override;

  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~BrowsingDataRemoverFunction() override;

 private:
  // Children should override this method to provide the proper removal mask
  // based on the API call they represent.
  // Returns whether or not removal mask retrieval was successful.
  // |removal_mask| is populated with the result, if successful.
  virtual bool GetRemovalMask(uint64_t* removal_mask) = 0;

  // Returns true if the data removal is allowed to pause Sync. Returns true by
  // default. Subclasses can override it to return false and prevent Sync from
  // being paused. This is important when synced data is being removed, and
  // pausing Sync would prevent the data from being deleted on the server.
  virtual bool IsPauseSyncAllowed();

  // Parse the developer-provided |origin_types| object into |origin_type_mask|
  // that can be used with the BrowsingDataRemover.
  // Returns true if parsing was successful.
  // Pre-condition: `options` is a dictionary.
  bool ParseOriginTypeMask(const base::Value::Dict& options,
                           uint64_t* origin_type_mask);

  // Parses the developer-provided list of origins into |result|.
  // Returns whether or not parsing was successful. In case of parse failure,
  // |error_response| will contain the error response.
  using OriginParsingResult =
      base::expected<std::vector<url::Origin>, ResponseValue>;
  OriginParsingResult ParseOrigins(const base::Value::List& list_value);

  // Called when we're ready to start removing data.
  void StartRemoving();

  // Called when a task is finished. Will finish the extension call when
  // |pending_tasks_| reaches zero.
  void OnTaskFinished();

  base::Time remove_since_;
  uint64_t removal_mask_ = 0;
  uint64_t origin_type_mask_ = 0;
  std::vector<url::Origin> origins_;
  content::BrowsingDataFilterBuilder::Mode mode_ =
      content::BrowsingDataFilterBuilder::Mode::kPreserve;
  int pending_tasks_ = 0;
  base::ScopedObservation<content::BrowsingDataRemover,
                          content::BrowsingDataRemover::Observer>
      observation_{this};
  std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion>
      synced_data_deletion_;
};

class BrowsingDataRemoveAppcacheFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeAppcache",
                             BROWSINGDATA_REMOVEAPPCACHE)

 protected:
  ~BrowsingDataRemoveAppcacheFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

class BrowsingDataRemoveFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.remove", BROWSINGDATA_REMOVE)

 protected:
  ~BrowsingDataRemoveFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
  bool IsPauseSyncAllowed() override;
};

class BrowsingDataRemoveCacheFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeCache",
                             BROWSINGDATA_REMOVECACHE)

 protected:
  ~BrowsingDataRemoveCacheFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

class BrowsingDataRemoveCookiesFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeCookies",
                             BROWSINGDATA_REMOVECOOKIES)

 protected:
  ~BrowsingDataRemoveCookiesFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

class BrowsingDataRemoveDownloadsFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeDownloads",
                             BROWSINGDATA_REMOVEDOWNLOADS)

 protected:
  ~BrowsingDataRemoveDownloadsFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

class BrowsingDataRemoveFileSystemsFunction
    : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeFileSystems",
                             BROWSINGDATA_REMOVEFILESYSTEMS)

 protected:
  ~BrowsingDataRemoveFileSystemsFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

class BrowsingDataRemoveFormDataFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeFormData",
                             BROWSINGDATA_REMOVEFORMDATA)

 protected:
  ~BrowsingDataRemoveFormDataFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

class BrowsingDataRemoveHistoryFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeHistory",
                             BROWSINGDATA_REMOVEHISTORY)

 protected:
  ~BrowsingDataRemoveHistoryFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

class BrowsingDataRemoveIndexedDBFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeIndexedDB",
                             BROWSINGDATA_REMOVEINDEXEDDB)

 protected:
  ~BrowsingDataRemoveIndexedDBFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

class BrowsingDataRemoveLocalStorageFunction
    : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeLocalStorage",
                             BROWSINGDATA_REMOVELOCALSTORAGE)

 protected:
  ~BrowsingDataRemoveLocalStorageFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

class BrowsingDataRemovePluginDataFunction
    : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removePluginData",
                             BROWSINGDATA_REMOVEPLUGINDATA)

 protected:
  ~BrowsingDataRemovePluginDataFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

class BrowsingDataRemovePasswordsFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removePasswords",
                             BROWSINGDATA_REMOVEPASSWORDS)

 protected:
  ~BrowsingDataRemovePasswordsFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

class BrowsingDataRemoveServiceWorkersFunction
    : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeServiceWorkers",
                             BROWSINGDATA_REMOVESERVICEWORKERS)

 protected:
  ~BrowsingDataRemoveServiceWorkersFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

class BrowsingDataRemoveCacheStorageFunction
    : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeCacheStorage",
                             BROWSINGDATA_REMOVECACHESTORAGE)

 protected:
  ~BrowsingDataRemoveCacheStorageFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

class BrowsingDataRemoveWebSQLFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeWebSQL",
                             BROWSINGDATA_REMOVEWEBSQL)

 protected:
  ~BrowsingDataRemoveWebSQLFunction() override = default;

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(uint64_t* removal_mask) override;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_BROWSING_DATA_BROWSING_DATA_API_H_

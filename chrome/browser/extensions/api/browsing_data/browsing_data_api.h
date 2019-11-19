// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions BrowsingData API functions, which entail
// clearing browsing data, and clearing the browser's cache (which, let's be
// honest, are the same thing), as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BROWSING_DATA_BROWSING_DATA_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BROWSING_DATA_BROWSING_DATA_API_H_

#include <string>
#include <vector>

#include "base/scoped_observer.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"

class PluginPrefs;
class PrefService;

namespace extension_browsing_data_api_constants {

// Parameter name keys.
extern const char kDataRemovalPermittedKey[];
extern const char kDataToRemoveKey[];
extern const char kOptionsKey[];

// Type keys.
extern const char kAppCacheKey[];
extern const char kCacheKey[];
extern const char kCookiesKey[];
extern const char kDownloadsKey[];
extern const char kFileSystemsKey[];
extern const char kFormDataKey[];
extern const char kHistoryKey[];
extern const char kIndexedDBKey[];
extern const char kPluginDataKey[];
extern const char kLocalStorageKey[];
extern const char kPasswordsKey[];
extern const char kServiceWorkersKey[];
extern const char kCacheStorageKey[];
extern const char kWebSQLKey[];

// Option keys.
extern const char kExtensionsKey[];
extern const char kOriginTypesKey[];
extern const char kProtectedWebKey[];
extern const char kSinceKey[];
extern const char kUnprotectedWebKey[];

// Errors!
extern const char kBadDataTypeDetails[];
extern const char kDeleteProhibitedError[];
extern const char kNonFilterableError[];
extern const char kIncompatibleFilterError[];
extern const char kInvalidOriginError[];

}  // namespace extension_browsing_data_api_constants

class BrowsingDataSettingsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.settings", BROWSINGDATA_SETTINGS)

  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~BrowsingDataSettingsFunction() override {}

 private:
  // Sets a boolean value in the |selected_dict| with the |data_type| as a key,
  // indicating whether the data type is both selected and permitted to be
  // removed; and a value in the |permitted_dict| with the |data_type| as a
  // key, indicating only whether the data type is permitted to be removed.
  void SetDetails(base::DictionaryValue* selected_dict,
                  base::DictionaryValue* permitted_dict,
                  const char* data_type,
                  bool is_selected);

  // Returns whether |data_type| is currently selected for deletion on |tab|.
  bool isDataTypeSelected(browsing_data::BrowsingDataType data_type,
                          browsing_data::ClearBrowsingDataTab tab);

  PrefService* prefs_ = nullptr;
};

// This serves as a base class from which the browsing data API removal
// functions will inherit. Each needs to be an observer of BrowsingDataRemover
// events, and each will handle those events in the same way (by calling the
// passed-in callback function).
//
// Each child class must implement GetRemovalMask(), which returns the bitmask
// of data types to remove.
class BrowsingDataRemoverFunction
    : public ChromeAsyncExtensionFunction,
      public content::BrowsingDataRemover::Observer {
 public:
  BrowsingDataRemoverFunction();

  // BrowsingDataRemover::Observer interface method.
  void OnBrowsingDataRemoverDone() override;

  // ExtensionFunction:
  bool RunAsync() override;

 protected:
  ~BrowsingDataRemoverFunction() override;

 private:
  // Children should override this method to provide the proper removal mask
  // based on the API call they represent.
  // Returns whether or not removal mask retrieval was successful.
  // |removal_mask| is populated with the result, if successful.
  virtual bool GetRemovalMask(int* removal_mask) = 0;

  // Returns true if the data removal is allowed to pause Sync. Returns true by
  // default. Subclasses can override it to return false and prevent Sync from
  // being paused. This is important when synced data is being removed, and
  // pausing Sync would prevent the data from being deleted on the server.
  virtual bool IsPauseSyncAllowed();

  // Updates the removal bitmask according to whether removing plugin data is
  // supported or not.
  void CheckRemovingPluginDataSupported(
      scoped_refptr<PluginPrefs> plugin_prefs);

  // Parse the developer-provided |origin_types| object into |origin_type_mask|
  // that can be used with the BrowsingDataRemover.
  // Returns true if parsing was successful.
  bool ParseOriginTypeMask(const base::DictionaryValue& options,
                           int* origin_type_mask);

  // Parse the developer-provided list of origins into |result|.
  // Returns true if parsing was successful.
  bool ParseOrigins(const base::Value& list_value,
                    std::vector<url::Origin>* result);

  // Called when we're ready to start removing data.
  void StartRemoving();

  // Called when a task is finished. Will finish the extension call when
  // |pending_tasks_| reaches zero.
  void OnTaskFinished();

  base::Time remove_since_;
  int removal_mask_ = 0;
  int origin_type_mask_ = 0;
  std::vector<url::Origin> origins_;
  content::BrowsingDataFilterBuilder::Mode mode_ =
      content::BrowsingDataFilterBuilder::Mode::BLACKLIST;
  int pending_tasks_ = 0;
  ScopedObserver<content::BrowsingDataRemover,
                 content::BrowsingDataRemover::Observer>
      observer_;
  std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion>
      synced_data_deletion_;
};

class BrowsingDataRemoveAppcacheFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeAppcache",
                             BROWSINGDATA_REMOVEAPPCACHE)

 protected:
  ~BrowsingDataRemoveAppcacheFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

class BrowsingDataRemoveFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.remove", BROWSINGDATA_REMOVE)

 protected:
  ~BrowsingDataRemoveFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
  bool IsPauseSyncAllowed() override;
};

class BrowsingDataRemoveCacheFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeCache",
                             BROWSINGDATA_REMOVECACHE)

 protected:
  ~BrowsingDataRemoveCacheFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

class BrowsingDataRemoveCookiesFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeCookies",
                             BROWSINGDATA_REMOVECOOKIES)

 protected:
  ~BrowsingDataRemoveCookiesFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

class BrowsingDataRemoveDownloadsFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeDownloads",
                             BROWSINGDATA_REMOVEDOWNLOADS)

 protected:
  ~BrowsingDataRemoveDownloadsFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

class BrowsingDataRemoveFileSystemsFunction
    : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeFileSystems",
                             BROWSINGDATA_REMOVEFILESYSTEMS)

 protected:
  ~BrowsingDataRemoveFileSystemsFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

class BrowsingDataRemoveFormDataFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeFormData",
                             BROWSINGDATA_REMOVEFORMDATA)

 protected:
  ~BrowsingDataRemoveFormDataFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

class BrowsingDataRemoveHistoryFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeHistory",
                             BROWSINGDATA_REMOVEHISTORY)

 protected:
  ~BrowsingDataRemoveHistoryFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

class BrowsingDataRemoveIndexedDBFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeIndexedDB",
                             BROWSINGDATA_REMOVEINDEXEDDB)

 protected:
  ~BrowsingDataRemoveIndexedDBFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

class BrowsingDataRemoveLocalStorageFunction
    : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeLocalStorage",
                             BROWSINGDATA_REMOVELOCALSTORAGE)

 protected:
  ~BrowsingDataRemoveLocalStorageFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

class BrowsingDataRemovePluginDataFunction
    : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removePluginData",
                             BROWSINGDATA_REMOVEPLUGINDATA)

 protected:
  ~BrowsingDataRemovePluginDataFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

class BrowsingDataRemovePasswordsFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removePasswords",
                             BROWSINGDATA_REMOVEPASSWORDS)

 protected:
  ~BrowsingDataRemovePasswordsFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

class BrowsingDataRemoveServiceWorkersFunction
    : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeServiceWorkers",
                             BROWSINGDATA_REMOVESERVICEWORKERS)

 protected:
  ~BrowsingDataRemoveServiceWorkersFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

class BrowsingDataRemoveCacheStorageFunction
    : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeCacheStorage",
                             BROWSINGDATA_REMOVECACHESTORAGE)

 protected:
  ~BrowsingDataRemoveCacheStorageFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

class BrowsingDataRemoveWebSQLFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeWebSQL",
                             BROWSINGDATA_REMOVEWEBSQL)

 protected:
  ~BrowsingDataRemoveWebSQLFunction() override {}

  // BrowsingDataRemoverFunction:
  bool GetRemovalMask(int* removal_mask) override;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_BROWSING_DATA_BROWSING_DATA_API_H_

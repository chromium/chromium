// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CWS_INFO_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_CWS_INFO_SERVICE_H_

#include <optional>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"

class PrefService;
class Profile;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace extensions {

class Extension;
class ExtensionPrefs;
class ExtensionRegistry;
class BatchGetStoreMetadatasResponse;

BASE_DECLARE_FEATURE(kCWSInfoService);
BASE_DECLARE_FEATURE(kCWSInfoFastCheck);

// This is an interface class to allow for easy mocking.
class CWSInfoServiceInterface {
 public:
  virtual ~CWSInfoServiceInterface() = default;

  // Synchronously checks if the extension is currently live in CWS.
  // If the information is not available immediately (i.e., not stored in local
  // cache), does not return a value.
  virtual std::optional<bool> IsLiveInCWS(const Extension& extension) const = 0;

  enum class CWSViolationType {
    kNone = 0,
    kMalware = 1,
    kPolicy = 2,
    kMinorPolicy = 3,
    // New enum values must go above here
    kUnknown
  };
  struct CWSInfo {
    // This extension is present in CWS.
    bool is_present = false;
    // This extension is currently published and downloadable from CWS.
    bool is_live = false;
    // The last time the extension was updated in CWS. Only valid if |is_live|
    // is true.
    base::Time last_update_time;
    // The following fields are only valid if |is_present| is true.
    // If the extension has been taken down, i.e., no longer live, this
    // represents the violation type that caused the take-down.
    CWSViolationType violation_type = CWSViolationType::kNone;
    // The extension was unpublished from CWS by the developer a while ago.
    bool unpublished_long_ago = false;
    // The extension does not display proper privacy practice information in
    // CWS.
    bool no_privacy_practice = false;
  };
  virtual std::optional<CWSInfo> GetCWSInfo(
      const Extension& extension) const = 0;

  // Initiates a fetch from CWS if:
  // - at least one installed extension is missing CWS metadata information
  // - Enough time (default: 24 hours) has elapsed since the last time the
  //   metadata was fetched.
  virtual void CheckAndMaybeFetchInfo() = 0;

  class Observer : public base::CheckedObserver {
   public:
    // This callback is invoked when there is a change in store metadata
    // saved by the service.
    virtual void OnCWSInfoChanged() {}
  };
  // Use these methods to (de)register for changes in the CWS metadata retrieved
  // by the service.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

// This service retrieves information about installed extensions from CWS
// periodically (default: every 24 hours). It is used exclusively on the
// browser UI thread. The service also supports out-of-cycle fetch requests for
// use cases where waiting for up to 24 hours for fresh state is not desirable
// (for example, when the ExtensionsUnpublishedAvailability policy setting
// changes). Only extensions that update from CWS are queried.
class CWSInfoService : public CWSInfoServiceInterface, public KeyedService {
 public:
  // Convenience method to get the service for a profile.
  static CWSInfoService* Get(Profile* profile);

  explicit CWSInfoService(Profile* profile);

  CWSInfoService(const CWSInfoService&) = delete;
  CWSInfoService& operator=(const CWSInfoService&) = delete;
  ~CWSInfoService() override;

  // CWSInfoServiceInterface:
  std::optional<bool> IsLiveInCWS(const Extension& extension) const override;
  std::optional<CWSInfo> GetCWSInfo(const Extension& extension) const override;
  void CheckAndMaybeFetchInfo() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // KeyedService:
  void Shutdown() override;

  // Helpers
  static CWSInfoService::CWSViolationType GetViolationTypeFromString(
      const std::string& violation_type_str);

  // Testing helpers
  std::string GetRequestURLForTesting() const;
  int GetStartupDelayForTesting() const;
  int GetCheckIntervalForTesting() const;
  int GetFetchIntervalForTesting() const;
  base::Time GetCWSInfoTimestampForTesting() const;
  base::Time GetCWSInfoFetchErrorTimestampForTesting() const;
  void SetMaxExtensionIdsPerRequestForTesting(int max);
  static void SetSkipApiCheckForTesting(bool skip_api_key_check);

 protected:
  // Only used for testing to create a fake derived class.
  CWSInfoService();

  // This method schedules an info check after specified |seconds|.
  void ScheduleCheck(int seconds);

  // This method prepares request protos to fetch CWS metadata. A CWS fetch
  // operation can consist of multiple request protos when the number of
  // installed extensions exceeds the max ids supported per request (100). The
  // request protos, extension ids and other data associated with the fetch are
  // returned in a |FetchContext|. The method also outputs a
  // |new_info_requested| that indicates if at least one of the installed
  // extensions is missing CWS metadata information.
  struct FetchContext;
  std::unique_ptr<FetchContext> CreateRequests(
      bool& /*output=*/new_info_requested);

  // Sends a single network request associated with a CWS info fetch.
  void SendRequest();

  // Handles the server response associated with a single network request.
  void OnResponseReceived(std::unique_ptr<std::string> response);

  // Saves data to prefs if the response data is different from the saved data.
  // Returns true if the response data is saved, false otherwise.
  bool MaybeSaveResponseToPrefs(
      const BatchGetStoreMetadatasResponse& response_proto);

  const raw_ptr<Profile> profile_ = nullptr;
  const raw_ptr<PrefService> pref_service_ = nullptr;
  const raw_ptr<ExtensionPrefs> extension_prefs_ = nullptr;
  const raw_ptr<ExtensionRegistry> extension_registry_ = nullptr;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Stores context about a fetch operation in progress. The service only
  // supports one fetch operation at a time.
  std::unique_ptr<FetchContext> active_fetch_;
  // Each request associated with a fetch can have a maximum of 100 extension
  // ids. This parameter can be changed for testing.
  int max_ids_per_request_;

  // Stats for requests, responses and errors.
  uint32_t info_requests_ = 0;
  uint32_t info_responses_ = 0;
  uint32_t info_errors_ = 0;
  // Counts the number of times the downloaded metadata was different from that
  // currently saved.
  uint32_t info_changes_ = 0;
  // A timer used to periodically check if CWS information needs to be fetched.
  base::OneShotTimer info_check_timer_;
  // Time from startup to first check of CWS information.
  int startup_delay_secs_ = 0;
  // Time interval between fetches from CWS info server. The interval value
  // varies +/-25% from default of 24 hours for every fetch.
  int current_fetch_interval_secs_ = 0;

  // List of observers that are notified whenever new CWS information is saved.
  base::ObserverList<Observer> observers_;

  friend class CWSInfoServiceTest;

  base::WeakPtrFactory<CWSInfoService> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CWS_INFO_SERVICE_H_

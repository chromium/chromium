// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_MODEL_H_
#define ASH_BIRCH_BIRCH_MODEL_H_

#include <map>
#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "ash/birch/birch_client.h"
#include "ash/birch/birch_coral_item.h"
#include "ash/birch/birch_item.h"
#include "ash/public/cpp/coral_util.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;

namespace ash {

class BirchDataProvider;
class BirchIconCache;
class BirchItemRemover;
class CoralItemRemover;

// Birch model, which is used to aggregate and store relevant information from
// different providers. Both data and prefs are associated with the primary user
// account.
class ASH_EXPORT BirchModel : public SessionObserver,
                              public SimpleGeolocationProvider::Observer {
 public:
  // The callback for lost media data changes. The argument is the updated lost
  // media item.
  using LostMediaDataChangedCallback =
      base::RepeatingCallback<void(std::unique_ptr<BirchItem>)>;

  // BirchModel Observers are notified when the BirchClient has been set.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnBirchClientSet() = 0;
  };

  BirchModel();
  BirchModel(const BirchModel&) = delete;
  BirchModel& operator=(const BirchModel&) = delete;
  ~BirchModel() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Sends a request to the birch keyed service to fetch data into the model.
  // `is_post_login` determines fetch timeout depending on whether this request
  // is made post login.
  // `callback` will run once either all data is fresh or the request timeout
  // has expired.
  void RequestBirchDataFetch(bool is_post_login, base::OnceClosure callback);

  void SetCalendarItems(const std::vector<BirchCalendarItem>& calendar_items);
  void SetAttachmentItems(
      const std::vector<BirchAttachmentItem>& attachment_items);
  void SetFileSuggestItems(
      const std::vector<BirchFileItem>& file_suggest_items);
  void SetRecentTabItems(const std::vector<BirchTabItem>& recent_tab_items);
  void SetLastActiveItems(const std::vector<BirchLastActiveItem>& items);
  void SetMostVisitedItems(const std::vector<BirchMostVisitedItem>& items);
  void SetSelfShareItems(
      const std::vector<BirchSelfShareItem>& self_share_items);
  void SetLostMediaItems(const std::vector<BirchLostMediaItem>& items);
  void SetReleaseNotesItems(
      const std::vector<BirchReleaseNotesItem>& release_notes_items);
  void SetWeatherItems(const std::vector<BirchWeatherItem>& weather_items);
  void SetCoralItems(const std::vector<BirchCoralItem>& coral_items);

  // Sets the BirchClient and begins initializing the BirchItemRemover.
  void SetClientAndInit(BirchClient* client);

  BirchClient* birch_client() { return birch_client_; }
  BirchIconCache* icon_cache() { return icon_cache_.get(); }

  const std::vector<BirchCalendarItem>& GetCalendarItemsForTest() const {
    return calendar_data_.items;
  }

  const std::vector<BirchAttachmentItem>& GetAttachmentItemsForTest() const {
    return attachment_data_.items;
  }
  const std::vector<BirchFileItem>& GetFileSuggestItemsForTest() const {
    return file_suggest_data_.items;
  }
  const std::vector<BirchTabItem>& GetTabsForTest() const {
    return recent_tab_data_.items;
  }
  const std::vector<BirchLastActiveItem>& GetLastActiveItemsForTest() const {
    return last_active_data_.items;
  }
  const std::vector<BirchMostVisitedItem>& GetMostVisitedItemsForTest() const {
    return most_visited_data_.items;
  }
  std::vector<BirchSelfShareItem>& GetSelfShareItemsForTest() {
    return self_share_data_.items;
  }
  std::vector<BirchLostMediaItem>& GetLostMediaItemsForTest() {
    return lost_media_data_.items;
  }
  const std::vector<BirchReleaseNotesItem>& GetReleaseNotesItemsForTest()
      const {
    return release_notes_data_.items;
  }
  const std::vector<BirchWeatherItem>& GetWeatherForTest() const {
    return weather_data_.items;
  }
  std::vector<BirchCoralItem>& GetCoralItemsForTest() {
    return coral_data_.items;
  }

  BirchItemRemover* GetItemRemoverForTest() { return item_remover_.get(); }
  CoralItemRemover* GetCoralItemRemoverForTest();

  // Returns all items, sorted by ranking. Includes unranked items.
  std::vector<std::unique_ptr<BirchItem>> GetAllItems();

  // Returns all items, sorted by ranking.
  std::vector<std::unique_ptr<BirchItem>> GetItemsForDisplay();

  // Returns whether all data in the model is currently fresh.
  bool IsDataFresh();

  // Add the BirchItem to the list of persistenly removed items.
  void RemoveItem(BirchItem* item);

  void SetLostMediaDataChangedCallback(LostMediaDataChangedCallback callback);

  void ResetLostMediaDataChangedCallback();

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // SimpleGeolocationProvider::Observer:
  void OnGeolocationPermissionChanged(bool enabled) override;

  BirchDataProvider* GetWeatherProviderForTest();
  void OverrideWeatherProviderForTest(
      std::unique_ptr<BirchDataProvider> weather_provider);
  BirchDataProvider* GetCoralProviderForTest();
  void OverrideCoralProviderForTest(
      std::unique_ptr<BirchDataProvider> coral_provider);

  void OverrideClockForTest(base::Clock* clock);
  void SetDataFetchCallbackForTest(base::OnceClosure callback);

 private:
  friend class BirchModelTest;

  // Timer and callback for a pending data fetch request.
  // The callback will be run if the timer expires before all data is fetched.
  struct PendingRequest {
    PendingRequest();
    ~PendingRequest();

    base::OnceClosure callback;
    std::unique_ptr<base::OneShotTimer> timer;
  };

  // Contains information related to fetching and storing data for a single
  // BirchItem type.
  template <typename T>
  struct DataTypeInfo {
    DataTypeInfo(const std::string& pref_name,
                 const std::string& metric_suffix);
    ~DataTypeInfo();

    // Whether a data fetch is in progress.
    bool fetch_in_progress = false;

    // When the fetch for data was started. Used for metrics.
    base::Time fetch_start_time;

    // List of items for this data type.
    std::vector<T> items;

    // Whether the data is fresh.
    bool is_fresh = false;

    // The name of the pref accossiated with this data type.
    std::string pref_name;

    // The suffix for metrics recorded for this data type.
    std::string metric_suffix;

    // The data update request to update existing items.
    std::optional<PendingRequest> update_request;
  };

  template <typename T>
  void SetItems(DataTypeInfo<T>& data_info,
                const std::vector<T>& items,
                bool record_latency);

  // Called when a pending data fetch request timeout expires.
  void HandleRequestTimeout(size_t request_id);

  // Called when the update request completes or timeout expires.
  // `lost_media_data_changed_callback_` will be called with the current lost
  // media items.
  void HandleLostMediaUpdateRequest();

  // Runs data fetch callbacks after a data fetch request when all data items
  // have been refreshed.
  void MaybeRespondToDataFetchRequest();

  // Get current time. The clock may be overridden for testing purposes.
  base::Time GetNow() const;

  // Clears all items.
  void ClearAllItems();

  // Marks all data types as not fresh.
  void MarkDataNotFresh();

  // Initializes the pref change registrars to observe for pref changes.
  void InitPrefChangeRegistrars();

  // Called when a data provider pref changes.
  void OnCalendarPrefChanged();
  void OnFileSuggestPrefChanged();
  void OnChromeTabsPrefChanged();
  void OnLostMediaPrefChanged();
  void OnWeatherPrefChanged();
  void OnReleaseNotesPrefChanged();
  void OnCoralPrefChanged();

  // Records metrics on which providers are hidden based on prefs.
  void RecordProviderHiddenHistograms();

  // Whether `item_remover_` is created and initialized.
  bool IsItemRemoverInitialized();

  // Requests a data fetch from `data_provider` depending on the fetch state.
  template <typename T>
  void StartDataFetchIfNeeded(DataTypeInfo<T>& data_info,
                              BirchDataProvider* data_provider);

  // Returns true if last active items should be included in the results.
  bool ShouldShowLastActive();

  // Returns true if most visited items should be included in the results.
  bool ShouldShowMostVisited();

  // Called when the lost media data provider changed.
  void OnLostMediaDataProviderChanged();

  // Whether this is a post-login fetch (occurring right after login).
  bool is_post_login_fetch_ = false;

  size_t next_request_id_ = 0u;
  // Pending data fetched requests mapped by their request IDs. IDs are
  // generated by incrementing `next_request_id_`.
  std::map<size_t, PendingRequest> pending_requests_;

  // When the last fetch was started. Used for metrics.
  base::Time fetch_start_time_;

  DataTypeInfo<BirchCalendarItem> calendar_data_;
  DataTypeInfo<BirchAttachmentItem> attachment_data_;
  DataTypeInfo<BirchFileItem> file_suggest_data_;
  DataTypeInfo<BirchTabItem> recent_tab_data_;
  DataTypeInfo<BirchLastActiveItem> last_active_data_;
  DataTypeInfo<BirchMostVisitedItem> most_visited_data_;
  DataTypeInfo<BirchSelfShareItem> self_share_data_;
  DataTypeInfo<BirchLostMediaItem> lost_media_data_;
  DataTypeInfo<BirchReleaseNotesItem> release_notes_data_;
  DataTypeInfo<BirchWeatherItem> weather_data_;
  DataTypeInfo<BirchCoralItem> coral_data_;

  raw_ptr<BirchClient> birch_client_ = nullptr;

  std::unique_ptr<BirchIconCache> icon_cache_;

  std::unique_ptr<BirchDataProvider> weather_provider_;
  std::unique_ptr<BirchDataProvider> coral_provider_;

  // When set, this clock is used to ensure a consistent current time is used
  // for testing.
  raw_ptr<base::Clock> clock_override_ = nullptr;

  // Whether an active user session changed notification has been seen. Used to
  // detect the initial notification on signin.
  bool has_active_user_session_changed_ = false;

  PrefChangeRegistrar calendar_pref_registrar_;
  PrefChangeRegistrar file_suggest_pref_registrar_;
  PrefChangeRegistrar chrome_tabs_pref_registrar_;
  PrefChangeRegistrar lost_media_pref_registrar_;
  PrefChangeRegistrar weather_pref_registrar_;
  PrefChangeRegistrar release_notes_pref_registrar_;
  PrefChangeRegistrar coral_pref_registrar_;

  // Used to filter out items which have previously been removed by the user.
  std::unique_ptr<BirchItemRemover> item_remover_;

  // A list of current BirchModel::Observers.
  base::ObserverList<Observer> observers_;

  // Invoked when a data fetch completes.
  base::OnceClosure data_fetch_callback_for_test_;

  // When we last returned a last active item. Used to suppress showing the
  // last active items too often.
  base::Time last_active_last_shown_;

  // When we last returned a most visited item. Used to suppress showing the
  // most visited items too often.
  base::Time most_visited_last_shown_;

  // The callback which runs when the lost media data is changed.
  LostMediaDataChangedCallback lost_media_data_changed_callback_;

  base::WeakPtrFactory<BirchModel> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_MODEL_H_

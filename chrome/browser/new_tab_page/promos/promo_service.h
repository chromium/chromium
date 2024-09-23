// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_PROMOS_PROMO_SERVICE_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_PROMOS_PROMO_SERVICE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/new_tab_page/promos/promo_data.h"
#include "chrome/browser/new_tab_page/promos/promo_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

class GURL;
class Profile;

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

// A service that downloads, caches, and hands out PromoData for middle-slot
// promos. It never initiates a download automatically, only when Refresh is
// called.
class PromoService : public KeyedService {
 public:
  enum class Status {
    // Promo service initialized and no new response has been registered.
    NOT_UPDATED,
    // Received a valid response and there is a promo running.
    OK_WITH_PROMO,
    // Received a valid response but there is no promo running.
    OK_WITHOUT_PROMO,
    // Some transient error occurred, e.g. the network request failed because
    // there is no network connectivity. A previously cached response may still
    // be used.
    TRANSIENT_ERROR,
    // A fatal error occurred, such as the server responding with an error code
    // or with invalid data. Any previously cached response should be cleared.
    FATAL_ERROR,
    // There's a valid promo coming back from the promo server, but it's been
    // locally blocked by the user client-side. TODO(crbug.com/40098612): send
    // blocked promo IDs to the server so this doesn't happen / they can do a
    // better job ranking?
    OK_BUT_BLOCKED,
  };

  PromoService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile);
  ~PromoService() override;

  // KeyedService implementation.
  void Shutdown() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns the currently cached middle-slot PromoData, if any. Virtual for
  // testing.
  virtual const std::optional<PromoData>& promo_data() const;
  Status promo_status() const { return promo_status_; }

  // Requests an asynchronous refresh from the network. After the update
  // completes, OnPromoDataUpdated will be called on the observers.
  virtual void Refresh();

  // Add/remove observers. All observers must unregister themselves before the
  // PromoService is destroyed. Virtual for testing.
  virtual void AddObserver(PromoServiceObserver* observer);
  void RemoveObserver(PromoServiceObserver* observer);

  // Marks |promo_id| as blocked from being shown again.
  void BlocklistPromo(const std::string& promo_id);

  // Unmarks |promo_id| as blocked and allows it to be shown again.
  void UndoBlocklistPromo(const std::string& promo_id);

  GURL GetLoadURLForTesting() const;

 protected:
  void PromoDataLoaded(Status status, const std::optional<PromoData>& data);

 private:
  void OnLoadDone(std::unique_ptr<std::string> response_body);
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);

  void NotifyObservers();

  // Clears any expired blocklist entries and determines whether |promo_id| has
  // been blocked by the user.
  bool IsBlockedAfterClearingExpired(const std::string& promo_id) const;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  base::ObserverList<PromoServiceObserver, true>::Unchecked observers_;

  std::optional<PromoData> promo_data_;
  Status promo_status_ = Status::NOT_UPDATED;

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<PromoService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_PROMOS_PROMO_SERVICE_H_

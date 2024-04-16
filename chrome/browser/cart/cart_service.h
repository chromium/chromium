// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CART_CART_SERVICE_H_
#define CHROME_BROWSER_CART_CART_SERVICE_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/cart/cart_db.h"
#include "chrome/browser/cart/cart_discount_link_fetcher.h"
#include "chrome/browser/cart/cart_metrics_tracker.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/cart/chrome_cart.mojom.h"
#include "chrome/browser/cart/discount_url_loader.h"
#include "chrome/browser/cart/fetch_discount_worker.h"
#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/commerce/core/discount_consent_handler.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/prefs/pref_registry_simple.h"

class DiscountURLLoader;
class FetchDiscountWorker;

namespace commerce {
class ShoppingService;
}

// Service to maintain and read/write data for chrome cart module.
// TODO(crbug.com/40199234) Make this BrowserContext-based and get rid of
// Profile usage so that we can modularize this.
class CartService : public history::HistoryServiceObserver,
                    public KeyedService,
                    public commerce::DiscountConsentHandler {
 public:
  // The maximum number of times that cart welcome surface shows.
  static constexpr int kWelcomSurfaceShowLimit = 3;
  // The number of days since creation for a cart to be considered expired.
  static constexpr int kCartExpirationTimeInDays = 14;

  // Use |CartServiceFactory::GetForProfile(...)| to get an instance of this
  // service.
  explicit CartService(Profile* profile);
  CartService(const CartService&) = delete;
  CartService& operator=(const CartService&) = delete;
  ~CartService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  // Gets called when cart module is temporarily hidden.
  void Hide();
  // Gets called when restoring the temporarily hidden cart module.
  void RestoreHidden();
  // Returns whether cart module has been temporarily hidden.
  bool IsHidden();
  // Get the proto database owned by the service.
  CartDB* GetDB();
  // Load the cart for a domain.
  void LoadCart(const std::string& domain, CartDB::LoadCallback callback);
  // Load all active carts in this service.
  virtual void LoadAllActiveCarts(CartDB::LoadCallback callback);
  // Add a cart to the cart service.
  void AddCart(const GURL& navigation_url,
               const std::optional<GURL>& cart_url,
               const cart_db::ChromeCartContentProto& proto);
  // Delete the cart from the same domain as |url| in the cart service. When not
  // |ignore_remove_status|, we keep the cart if it has been permanently
  // removed.
  void DeleteCart(const GURL& url, bool ignore_remove_status);
  // Only load carts with fake data in the database.
  void LoadCartsWithFakeData(CartDB::LoadCallback callback);
  // Gets called when discounts are available for the given cart_url.
  void UpdateDiscounts(const GURL& cart_url,
                       cart_db::ChromeCartContentProto new_proto,
                       const bool is_tester);
  // Gets called when a single cart in module is temporarily hidden.
  void HideCart(const GURL& cart_url, CartDB::OperationCallback callback);
  // Gets called when restoring the temporarily hidden single cart.
  void RestoreHiddenCart(const GURL& cart_url,
                         CartDB::OperationCallback callback);
  // Gets called when a single cart in module is permanently removed.
  void RemoveCart(const GURL& cart_url, CartDB::OperationCallback callback);
  // Gets called when restoring the permanently removed single cart.
  void RestoreRemovedCart(const GURL& cart_url,
                          CartDB::OperationCallback callback);
  // Gets called when module shows welcome surface and increases the counter by
  // one.
  void IncreaseWelcomeSurfaceCounter();
  // Returns whether to show the welcome surface in module. It is related to how
  // many times the welcome surface has shown.
  bool ShouldShowWelcomeSurface();
  // Decides whether to show the consent card in module for rule-based discount,
  // and returns it in the callback.
  void ShouldShowDiscountConsent(base::OnceCallback<void(bool)> callback);
  // Decides whether to show the discount toggle in the customize_modules
  // setting page.
  bool ShouldShowDiscountToggle();
  // Returns whether the rule-based discount feature in cart module is enabled,
  // and user has chosen to opt-in the feature.
  bool IsCartDiscountEnabled();
  // Updates whether the rule-based discount feature is enabled.
  void SetCartDiscountEnabled(bool enabled);
  // Gets called when cart with |cart_url| is clicked in NTP module. It is used
  // to get discount URL and return it in the |callback|. It is only called when
  // rule-based discount is enabled.
  void GetDiscountURL(const GURL& cart_url,
                      base::OnceCallback<void(const GURL&)> callback);
  // Gets called when a navigation to |cart_url| is happening or might happen.
  // |is_navigating| indicates whether the navigation is happening (e.g. left
  // click on the cart item) or might happen later (e.g. right click to open
  // context menu). This method 1) Record the latest interacted cart,
  // and then use that to identify whether a navigation originated from cart
  // module has happened. 2) Help identify whether to load discount URL.
  void PrepareForNavigation(const GURL& cart_url, bool is_navigating);
  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  // Returns whether a discount with |rule_id| is used or not.
  bool IsDiscountUsed(const std::string& rule_id);
  // Records timestamp of the latest fetch for discount.
  void RecordFetchTimestamp();
  // Called by discount worker to pass new coupons to CouponService.
  void UpdateFreeListingCoupons(const CouponService::CouponsMap& map);
  // KeyedService:
  void Shutdown() override;

  // commerce::DiscountConsentHandler:
  // Gets called when user has acknowledged the discount consent in cart module.
  // `shouldEnable` indicates whether user has chosen to opt-in or opt-out the
  // feature.
  void AcknowledgeDiscountConsent(bool should_enable) override;
  // Gets called when user has dismissed the discount consent in cart module.
  void DismissedDiscountConsent() override;
  // Gets called when user has click the 'Continue' button in the discount
  // consent.
  void InterestedInDiscountConsent() override;

  // This is used when the NativeDialog variation is active. It gets called
  // when user has clicked the 'Continue' button in the discount consent.
  void ShowNativeConsentDialog(
      Browser* browser,
      base::OnceCallback<void(chrome_cart::mojom::ConsentStatus)>
          consent_status_callback);
  // Appends UTM tags to the end of |base_url|. It will always append
  // "utm_source=chrome" and "utm_medium=app". It will also append
  // "utm_campaign" which can be one of the below three values:
  // * "utm_campaign=chrome-cart" for non-partner merchant carts.
  // * "utm_campaign=chrome-cart-discount-on" for partner merchant carts with
  //   discount enabled.
  // * "utm_campaign=chrome-cart-discount-off" for partner merchant carts with
  //   discount disabled.
  const GURL AppendUTM(const GURL& base_url);
  // Checks if there is an active abandoned cart for the domain of |url|, and
  // returns the result in the callback.
  virtual void HasActiveCartForURL(const GURL& url,
                                   base::OnceCallback<void(bool)> callback);
  // Checks if the cart feature is enabled based on user's setting.
  virtual bool IsCartEnabled();

 private:
  friend class CartServiceFactory;
  friend class CartServiceTest;
  friend class CartServiceDiscountConsentV2Test;
  friend class CartServiceDiscountTest;
  friend class CartServiceBrowserDiscountTest;
  friend class CartServiceDiscountFetchTest;
  friend class CartServiceCouponTest;
  friend class FetchDiscountWorkerBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(CartHandlerNtpModuleFakeDataTest,
                           TestEnableFakeData);

  // Callback when a database operation (e.g. insert or delete) is finished.
  void OnOperationFinished(bool success);
  // Callback when a database operation (e.g. insert or delete) is finished.
  // A callback will be passed in to notify whether the operation is successful.
  void OnOperationFinishedWithCallback(CartDB::OperationCallback callback,
                                       bool success);
  // Add carts with fake data to database.
  void AddCartsWithFakeData();
  // Delete carts with fake data from database.
  void DeleteCartsWithFakeData();
  // Delete content of carts that are removed from database.
  void DeleteRemovedCartsContent(bool success,
                                 std::vector<CartDB::KeyAndValue> proto_pairs);
  // A callback to filter out inactive carts for cart data loading.
  void OnLoadCarts(CartDB::LoadCallback callback,
                   bool success,
                   std::vector<CartDB::KeyAndValue> proto_pairs);
  // A callback to set the hidden status of a cart.
  void SetCartHiddenStatus(bool isHidden,
                           CartDB::OperationCallback callback,
                           bool success,
                           std::vector<CartDB::KeyAndValue> proto_pairs);
  // A callback to set the removed status of a cart.
  void SetCartRemovedStatus(bool isRemoved,
                            CartDB::OperationCallback callback,
                            bool success,
                            std::vector<CartDB::KeyAndValue> proto_pairs);
  // A callback to handle adding a cart.
  void OnAddCart(const GURL& navigation_url,
                 const std::optional<GURL>& cart_url,
                 cart_db::ChromeCartContentProto proto,
                 bool success,
                 std::vector<CartDB::KeyAndValue> proto_pairs);

  // Gets called when users has enabled the rule-based discount feature.
  void StartGettingDiscount();
  // A callback to fetch discount URL.
  void OnGetDiscountURL(const GURL& default_cart_url,
                        base::OnceCallback<void(const ::GURL&)> callback,
                        bool success,
                        std::vector<CartDB::KeyAndValue> proto_pairs);
  // A callback to return discount URL when it is fetched.
  void OnDiscountURLFetched(const GURL& default_cart_url,
                            base::OnceCallback<void(const ::GURL&)> callback,
                            const cart_db::ChromeCartContentProto& cart_proto,
                            const GURL& discount_url);

  // A callback to decide whether to show discount consent or not.
  void ShouldShowDiscountConsentCallback(
      base::OnceCallback<void(bool)> callback,
      bool success,
      std::vector<CartDB::KeyAndValue> proto_pairs);
  // Set discount_link_fetcher_ for testing purpose.
  void SetCartDiscountLinkFetcherForTesting(
      std::unique_ptr<CartDiscountLinkFetcher> discount_link_fetcher);
  // Set fetch_discount_worker_ for testing purpose.
  void SetFetchDiscountWorkerForTesting(
      std::unique_ptr<FetchDiscountWorker> fetch_discount_worker);
  // Set coupon_service_ for testing purpose.
  void SetCouponServiceForTesting(CouponService* coupon_service);
  // Returns whether a URL should be skipped based on server-side bloom filter.
  bool ShouldSkip(const GURL& url);
  void CacheUsedDiscounts(const cart_db::ChromeCartContentProto& proto);
  void CleanUpDiscounts(cart_db::ChromeCartContentProto proto);
  // A callback to to keep entries of removed carts when deletion.
  void OnDeleteCart(bool success, std::vector<CartDB::KeyAndValue> proto_pairs);
  // A callback for when enable status for cart-related features has changed.
  void OnCartFeaturesChanged(const std::string& pref_name);
  // Get if cart and discount feature are both enabled.
  bool IsCartAndDiscountEnabled();
  // Get calls when Cart module loads.
  void RecordDiscountConsentStatusAtLoad(bool should_show_consent);
  // Checks if a cart has expired.
  bool IsCartExpired(const cart_db::ChromeCartContentProto& proto);
  void HasActiveCartForURLCallback(
      base::OnceCallback<void(bool)> callback,
      bool success,
      std::vector<CartDB::KeyAndValue> proto_pairs);
  void MaybeCommitDeletion(GURL url);

  raw_ptr<Profile> profile_;
  std::unique_ptr<CartDB> cart_db_;
  base::ScopedObservation<history::HistoryService, HistoryServiceObserver>
      history_service_observation_{this};
  base::Value::Dict domain_name_mapping_;
  base::Value::Dict domain_cart_url_mapping_;
  std::unique_ptr<FetchDiscountWorker> fetch_discount_worker_;
  std::unique_ptr<FetchDiscountWorker> fetch_discount_worker_for_testing_;
  std::unique_ptr<CartDiscountLinkFetcher> discount_link_fetcher_;
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;
  std::unique_ptr<CartMetricsTracker> metrics_tracker_;
  std::unique_ptr<DiscountURLLoader> discount_url_loader_;
  raw_ptr<CouponService> coupon_service_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<commerce::ShoppingService, DanglingUntriaged> shopping_service_;
  base::flat_map<std::string, cart_db::ChromeCartContentProto>
      pending_deletion_map_;
  base::WeakPtrFactory<CartService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_CART_CART_SERVICE_H_

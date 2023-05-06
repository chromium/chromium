// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/cart/cart_processor.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/cart/cart_db.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/cart/cart.mojom.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters.mojom.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "components/search/ntp_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kMockMerchantName[] = "FOO";
const char kMockMerchantDomain[] = "foo.com";
const char kMockMerchantCartURL[] = "https://www.foo.com/cart";
const char kMockProductImageURLA[] = "https://www.foo.com/imageA";
const char kMockProductImageURLB[] = "https://www.foo.com/imageB";
const char kMockMerchantPageURL[] = "https://www.foo.com/test";
const char kMockMerchantDiscountText[] = "15% off";

class MockCartService : public CartService {
 public:
  explicit MockCartService(Profile* profile) : CartService(profile) {}

  MOCK_METHOD1(LoadAllActiveCarts, void(CartDB::LoadCallback callback));
  MOCK_METHOD0(IsCartEnabled, bool());
};
}  // namespace

class CartProcessorTest : public BrowserWithTestWindowTest {
 public:
  CartProcessorTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    mock_cart_service_ = static_cast<MockCartService*>(
        CartServiceFactory::GetForProfile(profile()));
    cart_processor_ = std::make_unique<CartProcessor>(mock_cart_service_.get());
  }

  CartProcessor& cart_processor() { return *cart_processor_; }

  MockCartService& mock_cart_service() { return *mock_cart_service_; }

 private:
  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{HistoryServiceFactory::GetInstance(),
             HistoryServiceFactory::GetDefaultFactory()},
            {CartServiceFactory::GetInstance(),
             base::BindRepeating([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
               return std::make_unique<MockCartService>(
                   Profile::FromBrowserContext(context));
             })}};
  }

  raw_ptr<MockCartService> mock_cart_service_;
  std::unique_ptr<CartProcessor> cart_processor_;
};

TEST_F(CartProcessorTest, TestFindCartForCluster) {
  // Create a fake cluster with one visit.
  auto cluster_mojom = history_clusters::mojom::Cluster::New();
  auto visit_mojom = history_clusters::mojom::URLVisit::New();
  visit_mojom->normalized_url = GURL(kMockMerchantPageURL);
  cluster_mojom->visits.push_back(std::move(visit_mojom));

  // Mock a fake cart that belongs to the same domain as the visit.
  MockCartService& cart_service = mock_cart_service();
  cart_db::ChromeCartContentProto cart_proto;
  cart_proto.set_key(kMockMerchantDomain);
  std::vector<CartDB::KeyAndValue> carts = {{kMockMerchantDomain, cart_proto}};
  EXPECT_CALL(cart_service, LoadAllActiveCarts(testing::_))
      .Times(1)
      .WillOnce(testing::WithArgs<0>(
          testing::Invoke([&carts](CartDB::LoadCallback callback) -> void {
            std::move(callback).Run(true, carts);
          })));
  EXPECT_CALL(cart_service, IsCartEnabled())
      .Times(1)
      .WillOnce(testing::Return(true));

  // Capture the cart mojom that is finally returned.
  ntp::history_clusters::cart::mojom::CartPtr cart_mojom;
  base::MockCallback<
      ntp::history_clusters::mojom::PageHandler::GetCartForClusterCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cart_mojom](ntp::history_clusters::cart::mojom::CartPtr cart) {
            cart_mojom = std::move(cart);
          }));

  cart_processor().GetCartForCluster(std::move(cluster_mojom), callback.Get());

  ASSERT_TRUE(cart_mojom);
  ASSERT_EQ(cart_mojom->domain, kMockMerchantDomain);
}

TEST_F(CartProcessorTest, TestNoCartForCluster) {
  // Create a fake cluster with one visit.
  auto cluster_mojom = history_clusters::mojom::Cluster::New();
  auto visit_mojom = history_clusters::mojom::URLVisit::New();
  visit_mojom->normalized_url = GURL(kMockMerchantPageURL);
  cluster_mojom->visits.push_back(std::move(visit_mojom));

  // Mock a fake cart that belongs to a different domain as the visit.
  const std::string domain = "bar.com";
  MockCartService& cart_service = mock_cart_service();
  cart_db::ChromeCartContentProto cart_proto;
  cart_proto.set_key(domain);
  std::vector<CartDB::KeyAndValue> carts = {{domain, cart_proto}};
  EXPECT_CALL(cart_service, LoadAllActiveCarts(testing::_))
      .Times(1)
      .WillOnce(testing::WithArgs<0>(
          testing::Invoke([&carts](CartDB::LoadCallback callback) -> void {
            std::move(callback).Run(true, carts);
          })));
  EXPECT_CALL(cart_service, IsCartEnabled())
      .Times(1)
      .WillOnce(testing::Return(true));

  // Capture the cart mojom that is finally returned.
  ntp::history_clusters::cart::mojom::CartPtr cart_mojom;
  base::MockCallback<
      ntp::history_clusters::mojom::PageHandler::GetCartForClusterCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cart_mojom](ntp::history_clusters::cart::mojom::CartPtr cart) {
            cart_mojom = std::move(cart);
          }));

  cart_processor().GetCartForCluster(std::move(cluster_mojom), callback.Get());

  ASSERT_FALSE(cart_mojom);
}

TEST_F(CartProcessorTest, TestNoCartForFailedLoad) {
  // Create a fake cluster with one visit.
  auto cluster_mojom = history_clusters::mojom::Cluster::New();
  auto visit_mojom = history_clusters::mojom::URLVisit::New();
  visit_mojom->normalized_url = GURL(kMockMerchantPageURL);
  cluster_mojom->visits.push_back(std::move(visit_mojom));

  // Mock the DB load fails.
  MockCartService& cart_service = mock_cart_service();
  cart_db::ChromeCartContentProto cart_proto;
  cart_proto.set_key(kMockMerchantDomain);
  std::vector<CartDB::KeyAndValue> carts = {{kMockMerchantDomain, cart_proto}};
  EXPECT_CALL(cart_service, LoadAllActiveCarts(testing::_))
      .Times(1)
      .WillOnce(testing::WithArgs<0>(
          testing::Invoke([&carts](CartDB::LoadCallback callback) -> void {
            std::move(callback).Run(false, carts);
          })));
  EXPECT_CALL(cart_service, IsCartEnabled())
      .Times(1)
      .WillOnce(testing::Return(true));

  // Capture the cart mojom that is finally returned.
  ntp::history_clusters::cart::mojom::CartPtr cart_mojom;
  base::MockCallback<
      ntp::history_clusters::mojom::PageHandler::GetCartForClusterCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cart_mojom](ntp::history_clusters::cart::mojom::CartPtr cart) {
            cart_mojom = std::move(cart);
          }));

  cart_processor().GetCartForCluster(std::move(cluster_mojom), callback.Get());

  ASSERT_FALSE(cart_mojom);
}

TEST_F(CartProcessorTest, TestCartToMojom) {
  // Create a fake cluster with one visit.
  auto cluster_mojom = history_clusters::mojom::Cluster::New();
  auto visit_mojom = history_clusters::mojom::URLVisit::New();
  visit_mojom->normalized_url = GURL(kMockMerchantPageURL);
  cluster_mojom->visits.push_back(std::move(visit_mojom));

  // Mock a fake cart that belongs to the same domain as the visit, and add full
  // information to the fake cart.
  MockCartService& cart_service = mock_cart_service();
  cart_db::ChromeCartContentProto cart_proto;
  cart_proto.set_key(kMockMerchantDomain);
  cart_proto.set_merchant(kMockMerchantName);
  cart_proto.set_merchant_cart_url(kMockMerchantCartURL);
  cart_proto.add_product_image_urls(kMockProductImageURLA);
  cart_proto.add_product_image_urls(kMockProductImageURLB);
  cart_proto.mutable_discount_info()->set_discount_text(
      kMockMerchantDiscountText);
  std::vector<CartDB::KeyAndValue> carts = {{kMockMerchantDomain, cart_proto}};
  EXPECT_CALL(cart_service, LoadAllActiveCarts(testing::_))
      .Times(1)
      .WillOnce(testing::WithArgs<0>(
          testing::Invoke([&carts](CartDB::LoadCallback callback) -> void {
            std::move(callback).Run(true, carts);
          })));
  EXPECT_CALL(cart_service, IsCartEnabled())
      .Times(1)
      .WillOnce(testing::Return(true));

  // Capture the cart mojom that is finally returned.
  ntp::history_clusters::cart::mojom::CartPtr cart_mojom;
  base::MockCallback<
      ntp::history_clusters::mojom::PageHandler::GetCartForClusterCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cart_mojom](ntp::history_clusters::cart::mojom::CartPtr cart) {
            cart_mojom = std::move(cart);
          }));

  cart_processor().GetCartForCluster(std::move(cluster_mojom), callback.Get());

  ASSERT_TRUE(cart_mojom);
  ASSERT_EQ(cart_mojom->domain, kMockMerchantDomain);
  ASSERT_EQ(cart_mojom->merchant, kMockMerchantName);
  ASSERT_EQ(cart_mojom->cart_url, GURL(kMockMerchantCartURL));
  ASSERT_EQ(cart_mojom->product_image_urls.size(), 2u);
  ASSERT_EQ(cart_mojom->product_image_urls[0], GURL(kMockProductImageURLA));
  ASSERT_EQ(cart_mojom->product_image_urls[1], GURL(kMockProductImageURLB));
  ASSERT_EQ(cart_mojom->discount_text, kMockMerchantDiscountText);
}

TEST_F(CartProcessorTest, TestFakeCart) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpChromeCartInHistoryClusterModule,
           {{ntp_features::kNtpChromeCartInHistoryClustersModuleDataParam,
             "6"}}},
      },
      {});
  auto cluster_mojom = history_clusters::mojom::Cluster::New();
  // Capture the cart mojom that is finally returned.
  ntp::history_clusters::cart::mojom::CartPtr cart_mojom;
  base::MockCallback<
      ntp::history_clusters::mojom::PageHandler::GetCartForClusterCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cart_mojom](ntp::history_clusters::cart::mojom::CartPtr cart) {
            cart_mojom = std::move(cart);
          }));
  EXPECT_CALL(mock_cart_service(), IsCartEnabled())
      .Times(1)
      .WillOnce(testing::Return(true));

  cart_processor().GetCartForCluster(std::move(cluster_mojom), callback.Get());

  ASSERT_TRUE(cart_mojom);
  ASSERT_EQ(cart_mojom->product_image_urls.size(), 6u);
}

TEST_F(CartProcessorTest, TestNoCartWhenFeatureDisabled) {
  // Create a fake cluster with one visit.
  auto cluster_mojom = history_clusters::mojom::Cluster::New();
  auto visit_mojom = history_clusters::mojom::URLVisit::New();
  visit_mojom->normalized_url = GURL(kMockMerchantPageURL);
  cluster_mojom->visits.push_back(std::move(visit_mojom));

  // Mock that the cart feature has been turned off.
  MockCartService& cart_service = mock_cart_service();
  EXPECT_CALL(cart_service, IsCartEnabled())
      .Times(1)
      .WillOnce(testing::Return(false));
  EXPECT_CALL(cart_service, LoadAllActiveCarts(testing::_)).Times(0);

  // Capture the cart mojom that is finally returned.
  ntp::history_clusters::cart::mojom::CartPtr cart_mojom;
  base::MockCallback<
      ntp::history_clusters::mojom::PageHandler::GetCartForClusterCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cart_mojom](ntp::history_clusters::cart::mojom::CartPtr cart) {
            cart_mojom = std::move(cart);
          }));

  cart_processor().GetCartForCluster(std::move(cluster_mojom), callback.Get());

  ASSERT_FALSE(cart_mojom);
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_impl.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/autofill_assistant/browser/public/mock_autofill_assistant.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using autofill_assistant::AutofillAssistant;
using base::Bucket;
using base::BucketsAre;
using BundleCapabilitiesInformation =
    autofill_assistant::AutofillAssistant::BundleCapabilitiesInformation;
using CapabilitiesInfo =
    autofill_assistant::AutofillAssistant::CapabilitiesInfo;
using autofill_assistant::MockAutofillAssistant;
using base::test::RunOnceCallback;
using CacheStateForIsTriggerFormSupported =
    FastCheckoutCapabilitiesFetcherImpl::CacheStateForIsTriggerFormSupported;
using testing::_;

constexpr uint32_t kHashPrefixSize = 10u;
constexpr char kIntent[] = "CHROME_FAST_CHECKOUT";
constexpr char kUmaKeyCacheStateIsTriggerFormSupported[] =
    "Autofill.FastCheckout.CapabilitiesFetcher."
    "CacheStateForIsTriggerFormSupported";
constexpr char kUmaKeyHttpCode[] =
    "Autofill.FastCheckout.CapabilitiesFetcher.HttpResponseCode";
constexpr char kUmaKeyResponseTime[] =
    "Autofill.FastCheckout.CapabilitiesFetcher.ResponseTime";

constexpr char kUrl1[] = "https://wwww.firstpage.com/";
constexpr char kUrl2[] = "https://wwww.another-domain.co.uk/";

constexpr autofill::FormSignature kFormSignature1{123ull};
constexpr autofill::FormSignature kFormSignature2{45363456756ull};
constexpr autofill::FormSignature kFormSignature3{6736345675456ull};

class FastCheckoutCapabilitiesFetcherImplTest : public ::testing::Test {
 public:
  FastCheckoutCapabilitiesFetcherImplTest() {
    auto autofill_assistant = std::make_unique<MockAutofillAssistant>();
    autofill_assistant_ = autofill_assistant.get();

    fetcher_ = std::make_unique<FastCheckoutCapabilitiesFetcherImpl>(
        std::move(autofill_assistant));
  }
  ~FastCheckoutCapabilitiesFetcherImplTest() override = default;

 protected:
  MockAutofillAssistant* autofill_assistant() { return autofill_assistant_; }
  FastCheckoutCapabilitiesFetcher* fetcher() { return fetcher_.get(); }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  // Test support.
  base::HistogramTester histogram_tester_;
  raw_ptr<MockAutofillAssistant> autofill_assistant_;

  // The object to be tested.
  std::unique_ptr<FastCheckoutCapabilitiesFetcher> fetcher_;
};

TEST_F(FastCheckoutCapabilitiesFetcherImplTest, GetCapabilitiesEmptyResponse) {
  url::Origin origin1 = url::Origin::Create(GURL(kUrl1));
  uint64_t hash1 = AutofillAssistant::GetHashPrefix(kHashPrefixSize, origin1);

  // The cache is empty.
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));

  EXPECT_CALL(*autofill_assistant(),
              GetCapabilitiesByHashPrefix(
                  kHashPrefixSize, std::vector<uint64_t>{hash1}, kIntent, _))
      .WillOnce(RunOnceCallback<3>(net::HttpStatusCode::HTTP_OK,
                                   std::vector<CapabilitiesInfo>()));

  base::MockCallback<FastCheckoutCapabilitiesFetcher::Callback> callback;
  EXPECT_CALL(callback, Run(true));
  fetcher()->FetchAvailability(origin1, callback.Get());

  // The form is still not supported.
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));

  // The network metric and the response time were recorded.
  histogram_tester().ExpectUniqueSample(kUmaKeyHttpCode,
                                        net::HttpStatusCode::HTTP_OK, 1u);
  histogram_tester().ExpectTotalCount(kUmaKeyResponseTime, 1u);
}

TEST_F(FastCheckoutCapabilitiesFetcherImplTest,
       GetCapabilitiesResponseWithForm) {
  url::Origin origin1 = url::Origin::Create(GURL(kUrl1));
  url::Origin origin2 = url::Origin::Create(GURL(kUrl2));
  uint64_t hash1 = AutofillAssistant::GetHashPrefix(kHashPrefixSize, origin1);

  // The cache is empty.
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin2, kFormSignature1));

  BundleCapabilitiesInformation capabilities;
  capabilities.trigger_form_signatures.push_back(kFormSignature1);
  CapabilitiesInfo info{kUrl1, {}, capabilities};
  EXPECT_CALL(*autofill_assistant(),
              GetCapabilitiesByHashPrefix(
                  kHashPrefixSize, std::vector<uint64_t>{hash1}, kIntent, _))
      .WillOnce(RunOnceCallback<3>(net::HttpStatusCode::HTTP_OK,
                                   std::vector<CapabilitiesInfo>{info}));

  base::MockCallback<FastCheckoutCapabilitiesFetcher::Callback> callback;
  EXPECT_CALL(callback, Run(true));
  fetcher()->FetchAvailability(origin1, callback.Get());

  // The first origin now has a supported form.
  EXPECT_TRUE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature2));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin2, kFormSignature1));
}

TEST_F(FastCheckoutCapabilitiesFetcherImplTest, GetCapabilitiesNetworkError) {
  url::Origin origin1 = url::Origin::Create(GURL(kUrl1));
  uint64_t hash1 = AutofillAssistant::GetHashPrefix(kHashPrefixSize, origin1);

  // The cache is empty.
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));

  AutofillAssistant::GetCapabilitiesResponseCallback response_callback;
  EXPECT_CALL(*autofill_assistant(),
              GetCapabilitiesByHashPrefix(
                  kHashPrefixSize, std::vector<uint64_t>{hash1}, kIntent, _))
      .Times(1)
      .WillOnce(MoveArg<3>(&response_callback));

  base::MockCallback<FastCheckoutCapabilitiesFetcher::Callback> callback1;
  base::MockCallback<FastCheckoutCapabilitiesFetcher::Callback> callback2;
  fetcher()->FetchAvailability(origin1, callback1.Get());
  // Send the same request again (while the first one is still ongoing).
  fetcher()->FetchAvailability(origin1, callback2.Get());

  EXPECT_CALL(callback1, Run(false));
  EXPECT_CALL(callback2, Run(false));

  BundleCapabilitiesInformation capabilities;
  capabilities.trigger_form_signatures.push_back(kFormSignature1);
  CapabilitiesInfo info{kUrl1, {}, capabilities};
  std::move(response_callback)
      .Run(net::HttpStatusCode::HTTP_NOT_FOUND,
           std::vector<CapabilitiesInfo>{info});

  // The cache is still empty - the content of the message was ignored.
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));

  // However, the network metric and the response time were recorded.
  histogram_tester().ExpectUniqueSample(
      kUmaKeyHttpCode, net::HttpStatusCode::HTTP_NOT_FOUND, 1u);
  histogram_tester().ExpectTotalCount(kUmaKeyResponseTime, 1u);
}

TEST_F(FastCheckoutCapabilitiesFetcherImplTest,
       GetCapabilitiesSubsequentRequests) {
  url::Origin origin1 = url::Origin::Create(GURL(kUrl1));
  uint64_t hash1 = AutofillAssistant::GetHashPrefix(kHashPrefixSize, origin1);

  // The cache is empty.
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));

  // The first request times out.
  EXPECT_CALL(*autofill_assistant(),
              GetCapabilitiesByHashPrefix(
                  kHashPrefixSize, std::vector<uint64_t>{hash1}, kIntent, _))
      .WillOnce(RunOnceCallback<3>(net::HttpStatusCode::HTTP_REQUEST_TIMEOUT,
                                   std::vector<CapabilitiesInfo>{}));

  base::MockCallback<FastCheckoutCapabilitiesFetcher::Callback> callback1;
  EXPECT_CALL(callback1, Run(false));
  fetcher()->FetchAvailability(origin1, callback1.Get());
  // The cache is still empty.
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));

  // The second request is successful.
  BundleCapabilitiesInformation capabilities;
  capabilities.trigger_form_signatures.push_back(kFormSignature1);
  CapabilitiesInfo info{kUrl1, {}, capabilities};
  EXPECT_CALL(*autofill_assistant(),
              GetCapabilitiesByHashPrefix(
                  kHashPrefixSize, std::vector<uint64_t>{hash1}, kIntent, _))
      .WillOnce(RunOnceCallback<3>(net::HttpStatusCode::HTTP_OK,
                                   std::vector<CapabilitiesInfo>{info}));

  base::MockCallback<FastCheckoutCapabilitiesFetcher::Callback> callback2;
  EXPECT_CALL(callback2, Run(true));
  fetcher()->FetchAvailability(origin1, callback2.Get());
  // The cache is now filled.
  EXPECT_TRUE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));

  // A third request returns immediately.
  base::MockCallback<FastCheckoutCapabilitiesFetcher::Callback> callback3;
  EXPECT_CALL(callback3, Run(true));
  fetcher()->FetchAvailability(origin1, callback3.Get());
  EXPECT_TRUE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));

  // All network metrics were recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kUmaKeyHttpCode),
              BucketsAre(Bucket(net::HttpStatusCode::HTTP_REQUEST_TIMEOUT, 1u),
                         Bucket(net::HttpStatusCode::HTTP_OK, 1u)));
  histogram_tester().ExpectTotalCount(kUmaKeyResponseTime, 2u);
}

TEST_F(FastCheckoutCapabilitiesFetcherImplTest,
       GetCapabilitiesMultipleRequests) {
  url::Origin origin1 = url::Origin::Create(GURL(kUrl1));
  url::Origin origin2 = url::Origin::Create(GURL(kUrl2));
  uint64_t hash1 = AutofillAssistant::GetHashPrefix(kHashPrefixSize, origin1);
  uint64_t hash2 = AutofillAssistant::GetHashPrefix(kHashPrefixSize, origin2);

  // The cache is empty.
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature2));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin2, kFormSignature3));

  AutofillAssistant::GetCapabilitiesResponseCallback response_callback1;
  AutofillAssistant::GetCapabilitiesResponseCallback response_callback2;
  EXPECT_CALL(*autofill_assistant(),
              GetCapabilitiesByHashPrefix(
                  kHashPrefixSize, std::vector<uint64_t>{hash1}, kIntent, _))
      .Times(1)
      .WillOnce(MoveArg<3>(&response_callback1));
  EXPECT_CALL(*autofill_assistant(),
              GetCapabilitiesByHashPrefix(
                  kHashPrefixSize, std::vector<uint64_t>{hash2}, kIntent, _))
      .Times(1)
      .WillOnce(MoveArg<3>(&response_callback2));

  base::MockCallback<FastCheckoutCapabilitiesFetcher::Callback> callback1;
  base::MockCallback<FastCheckoutCapabilitiesFetcher::Callback> callback2;
  base::MockCallback<FastCheckoutCapabilitiesFetcher::Callback> callback3;
  fetcher()->FetchAvailability(origin1, callback1.Get());
  fetcher()->FetchAvailability(origin2, callback2.Get());
  fetcher()->FetchAvailability(origin1, callback3.Get());

  EXPECT_CALL(callback1, Run(true));
  EXPECT_CALL(callback3, Run(true));

  BundleCapabilitiesInformation capabilities;
  capabilities.trigger_form_signatures.push_back(kFormSignature1);
  capabilities.trigger_form_signatures.push_back(kFormSignature2);
  CapabilitiesInfo info1{kUrl1, {}, capabilities};
  std::move(response_callback1)
      .Run(net::HttpStatusCode::HTTP_OK, std::vector<CapabilitiesInfo>{info1});

  // The cache contains information for the first domain.
  EXPECT_TRUE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));
  EXPECT_TRUE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature2));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature3));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin2, kFormSignature1));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin2, kFormSignature2));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin2, kFormSignature3));

  EXPECT_CALL(callback2, Run(true));
  capabilities.trigger_form_signatures.clear();
  capabilities.trigger_form_signatures.push_back(kFormSignature3);
  CapabilitiesInfo info2{kUrl2, {}, capabilities};
  std::move(response_callback2)
      .Run(net::HttpStatusCode::HTTP_OK, std::vector<CapabilitiesInfo>{info2});

  // The cache now contains all domain information.
  EXPECT_TRUE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));
  EXPECT_TRUE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature2));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature3));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin2, kFormSignature1));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin2, kFormSignature2));
  EXPECT_TRUE(fetcher()->IsTriggerFormSupported(origin2, kFormSignature3));
}

TEST_F(FastCheckoutCapabilitiesFetcherImplTest,
       EnableFastCheckoutCapabilitiesFlag) {
  // `kEnableFastCheckoutCapabilitiesFlag` flag is disabled,
  // `IsTriggerFormSupported` returns the default value (false).
  url::Origin origin1 = url::Origin::Create(GURL(kUrl1));
  url::Origin origin2 = url::Origin::Create(GURL(kUrl2));

  // The cache is empty.
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature2));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin2, kFormSignature3));

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kForceEnableFastCheckoutCapabilities);

  EXPECT_TRUE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));
  EXPECT_TRUE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature2));
  EXPECT_TRUE(fetcher()->IsTriggerFormSupported(origin2, kFormSignature3));
}

TEST_F(FastCheckoutCapabilitiesFetcherImplTest,
       IsTriggerFormSupportedRecordsUmaMetrics) {
  url::Origin origin1 = url::Origin::Create(GURL(kUrl1));
  uint64_t hash1 = AutofillAssistant::GetHashPrefix(kHashPrefixSize, origin1);

  // The cache is empty.
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature2));
  histogram_tester().ExpectUniqueSample(
      kUmaKeyCacheStateIsTriggerFormSupported,
      CacheStateForIsTriggerFormSupported::kNeverFetched, 2u);

  AutofillAssistant::GetCapabilitiesResponseCallback response_callback1;
  EXPECT_CALL(*autofill_assistant(),
              GetCapabilitiesByHashPrefix(
                  kHashPrefixSize, std::vector<uint64_t>{hash1}, kIntent, _))
      .Times(1)
      .WillOnce(MoveArg<3>(&response_callback1));

  base::MockCallback<FastCheckoutCapabilitiesFetcher::Callback> callback1;
  fetcher()->FetchAvailability(origin1, callback1.Get());

  // While the fetch is still ongoing, there is no availability yet.
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(kUmaKeyCacheStateIsTriggerFormSupported),
      BucketsAre(
          Bucket(CacheStateForIsTriggerFormSupported::kNeverFetched, 2u),
          Bucket(CacheStateForIsTriggerFormSupported::kFetchOngoing, 1u)));

  EXPECT_CALL(callback1, Run(true));
  BundleCapabilitiesInformation capabilities;
  capabilities.trigger_form_signatures.push_back(kFormSignature1);
  CapabilitiesInfo info1{kUrl1, {}, capabilities};
  std::move(response_callback1)
      .Run(net::HttpStatusCode::HTTP_OK, std::vector<CapabilitiesInfo>{info1});

  // The cache contains information for the first domain.
  EXPECT_TRUE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature1));
  histogram_tester().ExpectTotalCount(kUmaKeyCacheStateIsTriggerFormSupported,
                                      4u);
  histogram_tester().ExpectBucketCount(
      kUmaKeyCacheStateIsTriggerFormSupported,
      CacheStateForIsTriggerFormSupported::kEntryAvailableAndFormSupported, 1u);

  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(origin1, kFormSignature2));
  histogram_tester().ExpectTotalCount(kUmaKeyCacheStateIsTriggerFormSupported,
                                      5u);
  histogram_tester().ExpectBucketCount(
      kUmaKeyCacheStateIsTriggerFormSupported,
      CacheStateForIsTriggerFormSupported::kEntryAvailableAndFormNotSupported,
      1u);
}

}  // namespace

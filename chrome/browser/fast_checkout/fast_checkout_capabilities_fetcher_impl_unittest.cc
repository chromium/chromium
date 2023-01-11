// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_impl.h"

#include "base/test/task_environment.h"
#include "chrome/browser/fast_checkout/fast_checkout_funnels.pb.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace {
constexpr char kFastCheckoutFunnelsUrl[] =
    "https://www.gstatic.com/autofill/fast_checkout/funnels.binarypb";

constexpr char kDomain[] = "https://www.example.com";
constexpr char kUnsupportedDomain[] = "https://www.example2.com";
constexpr char kInvalidDomain[] = "invaliddomain";
constexpr char kNonHttpSDomain[] = "file://path/to/a/file";
const auto kSupportedDomains =
    std::vector<std::string>({kDomain, kInvalidDomain, kNonHttpSDomain});
constexpr autofill::FormSignature kTriggerFormSignature =
    autofill::FormSignature(1234567890UL);
constexpr autofill::FormSignature kFillFormSignature =
    autofill::FormSignature(9876543210UL);

std::string CreateBinaryProtoResponse() {
  ::fast_checkout::FastCheckoutFunnels funnels;
  for (const std::string& domain : kSupportedDomains) {
    ::fast_checkout::FastCheckoutFunnels_FastCheckoutFunnel* funnel =
        funnels.add_funnels();
    funnel->add_domains(domain);
    funnel->add_trigger(kTriggerFormSignature.value());
    funnel->add_fill(kFillFormSignature.value());
  }
  return funnels.SerializeAsString();
}
}  // namespace

class FastCheckoutCapabilitiesFetcherImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  FastCheckoutCapabilitiesFetcherImplTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    origin_ = url::Origin::Create(GURL(kDomain));
    unsupported_origin_ = url::Origin::Create(GURL(kUnsupportedDomain));
    invalid_origin_ = url::Origin::Create(GURL(kInvalidDomain));
    non_http_s_origin_ = url::Origin::Create(GURL(kNonHttpSDomain));
  }

 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());
    fetcher_ = std::make_unique<FastCheckoutCapabilitiesFetcherImpl>(
        test_shared_loader_factory_);

    binary_proto_response_ = CreateBinaryProtoResponse();
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return test_url_loader_factory_.get();
  }
  FastCheckoutCapabilitiesFetcherImpl* fetcher() { return fetcher_.get(); }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }

  const std::string& GetBinaryProtoResponse() { return binary_proto_response_; }
  const url::Origin& GetOrigin() { return origin_; }
  const url::Origin& GetUnsupportedOrigin() { return unsupported_origin_; }
  const url::Origin& GetInvalidOrigin() { return invalid_origin_; }
  const url::Origin& GetNonHttpSOrigin() { return non_http_s_origin_; }

  bool FetchCapabilitiesAndSimulateResponse(
      net::HttpStatusCode status = net::HTTP_OK) {
    fetcher()->FetchCapabilities();
    return url_loader_factory()->SimulateResponseForPendingRequest(
        kFastCheckoutFunnelsUrl, GetBinaryProtoResponse(), status);
  }

  bool IsTriggerFormSupportedOnSupportedDomain() {
    return fetcher()->IsTriggerFormSupported(GetOrigin(),
                                             kTriggerFormSignature);
  }

 private:
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<FastCheckoutCapabilitiesFetcherImpl> fetcher_;
  std::string binary_proto_response_;
  url::Origin origin_;
  url::Origin unsupported_origin_;
  url::Origin invalid_origin_;
  url::Origin non_http_s_origin_;
};

TEST_F(FastCheckoutCapabilitiesFetcherImplTest,
       FetchCapabilities_AllowsOnlyOnePendingRequest) {
  fetcher()->FetchCapabilities();
  fetcher()->FetchCapabilities();
  EXPECT_EQ(url_loader_factory()->NumPending(), 1);
}

TEST_F(FastCheckoutCapabilitiesFetcherImplTest,
       FetchCapabilities_OnlyFetchesIfCacheTimeouted) {
  // Fetch funnels successfully.
  EXPECT_TRUE(FetchCapabilitiesAndSimulateResponse());

  // Not able to perform another fetch because cache is still valid.
  fetcher()->FetchCapabilities();
  EXPECT_EQ(url_loader_factory()->NumPending(), 0);

  // Travel through time beyond cache timeout.
  FastForwardBy(base::TimeDelta(base::Minutes(11)));

  // `FetchCapabilities()` is possible again because cache got stale.
  fetcher()->FetchCapabilities();
  EXPECT_EQ(url_loader_factory()->NumPending(), 1);
}

TEST_F(FastCheckoutCapabilitiesFetcherImplTest,
       IsTriggerFormSupported_ReturnsTrueOnlyForTriggerFormOnSupportedDomain) {
  EXPECT_TRUE(FetchCapabilitiesAndSimulateResponse());
  EXPECT_TRUE(IsTriggerFormSupportedOnSupportedDomain());
  EXPECT_FALSE(
      fetcher()->IsTriggerFormSupported(GetOrigin(), kFillFormSignature));
}

TEST_F(FastCheckoutCapabilitiesFetcherImplTest,
       IsTriggerFormSupported_ReturnsFalseOnUnsupportedDomain) {
  EXPECT_TRUE(FetchCapabilitiesAndSimulateResponse());
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(GetUnsupportedOrigin(),
                                                 kTriggerFormSignature));
  // `IsTriggerFormSupported()` on the supported domain should still return
  // `true` (for `kTriggerFormSignature).
  EXPECT_TRUE(IsTriggerFormSupportedOnSupportedDomain());
}

TEST_F(FastCheckoutCapabilitiesFetcherImplTest,
       IsTriggerFormSupported_ReturnsFalseOnInvalidDomain) {
  EXPECT_TRUE(FetchCapabilitiesAndSimulateResponse());
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(GetInvalidOrigin(),
                                                 kTriggerFormSignature));
  // `IsTriggerFormSupported()` on the supported domain should still return
  // `true` (for `kTriggerFormSignature).
  EXPECT_TRUE(IsTriggerFormSupportedOnSupportedDomain());
}

TEST_F(FastCheckoutCapabilitiesFetcherImplTest,
       IsTriggerFormSupported_ReturnsFalseOnNonHttpSDomain) {
  EXPECT_TRUE(FetchCapabilitiesAndSimulateResponse());
  EXPECT_FALSE(fetcher()->IsTriggerFormSupported(GetNonHttpSOrigin(),
                                                 kTriggerFormSignature));
  // `IsTriggerFormSupported()` on the supported domain should still return
  // `true` (for `kTriggerFormSignature).
  EXPECT_TRUE(IsTriggerFormSupportedOnSupportedDomain());
}

TEST_F(FastCheckoutCapabilitiesFetcherImplTest,
       IsTriggerFormSupported_ReturnsFalseIfRequestFailed) {
  EXPECT_TRUE(FetchCapabilitiesAndSimulateResponse(net::HTTP_BAD_REQUEST));
  EXPECT_FALSE(IsTriggerFormSupportedOnSupportedDomain());
}

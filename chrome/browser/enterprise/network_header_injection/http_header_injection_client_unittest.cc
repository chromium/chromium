// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/network_header_injection/http_header_injection_client.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/enterprise/network_header_injection/http_header_injection_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_rule.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_service.h"
#include "components/enterprise/network_header_injection/core/network_header_injection_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_custom_headers {

namespace {

void ValidateSingleHeaderLogEvent(
    const std::optional<base::DictValue>& extended_net_log_events,
    const std::string& header_name,
    const std::string& expected_value,
    bool expected_is_override) {
  ASSERT_TRUE(extended_net_log_events.has_value());

  const base::DictValue* policy_dict =
      extended_net_log_events->FindDict("http_header_injection_policy");
  ASSERT_TRUE(policy_dict);

  const base::DictValue* header_entry = policy_dict->FindDict(header_name);
  ASSERT_TRUE(header_entry);

  const std::string* header_val = header_entry->FindString("value");
  ASSERT_TRUE(header_val);
  EXPECT_EQ(expected_value, *header_val);

  std::optional<bool> is_override = header_entry->FindBool("is_override");
  ASSERT_TRUE(is_override.has_value());
  EXPECT_EQ(expected_is_override, is_override.value());
}

}  // namespace

class MockTrustedHeaderClient : public network::mojom::TrustedHeaderClient {
 public:
  MockTrustedHeaderClient() {
    using testing::_;
    ON_CALL(*this, OnBeforeSendHeaders(_, _, _))
        .WillByDefault([](const GURL& request_url,
                          const net::HttpRequestHeaders& headers,
                          OnBeforeSendHeadersCallback callback) {
          std::move(callback).Run(net::OK, std::nullopt, std::nullopt);
        });
    ON_CALL(*this, OnHeadersReceived(_, _, _, _))
        .WillByDefault([](const std::string& headers,
                          const net::IPEndPoint& remote_endpoint,
                          const std::optional<net::SSLInfo>& ssl_info,
                          OnHeadersReceivedCallback callback) {
          std::move(callback).Run(net::OK, std::nullopt, std::nullopt);
        });
  }
  ~MockTrustedHeaderClient() override = default;

  mojo::PendingRemote<network::mojom::TrustedHeaderClient> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnBeforeSendHeaders,
              (const GURL& request_url,
               const net::HttpRequestHeaders& headers,
               OnBeforeSendHeadersCallback callback),
              (override));

  MOCK_METHOD(void,
              OnHeadersReceived,
              (const std::string& headers,
               const net::IPEndPoint& remote_endpoint,
               const std::optional<net::SSLInfo>& ssl_info,
               OnHeadersReceivedCallback callback),
              (override));

 private:
  mojo::Receiver<network::mojom::TrustedHeaderClient> receiver_{this};
};

class HttpHeaderInjectionClientTest : public testing::Test {
 public:
  HttpHeaderInjectionClientTest() = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = builder.Build();
  }

  void SetPolicy(
      const std::string& domain,
      const std::vector<std::pair<std::string, std::string>>& headers_list) {
    base::ListValue headers;
    for (const auto& [name, value] : headers_list) {
      headers.Append(base::DictValue()
                         .Set(kKeyHeaderName, name)
                         .Set(kKeyHeaderValue, value));
    }

    base::ListValue rules = base::ListValue().Append(
        base::DictValue()
            .Set(kKeyPatterns, base::ListValue().Append(domain))
            .Set(kKeyHeaders, std::move(headers)));

    profile_->GetPrefs()->SetList(prefs::kHttpHeaderInjection,
                                  std::move(rules));
  }

 protected:
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

// Tests that original headers are passed through unchanged when there is no
// valid `HttpHeaderInjectionService` (e.g. during teardown).
TEST_F(HttpHeaderInjectionClientTest, PassThroughWithoutInjectionService) {
  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  HttpHeaderInjectionClient::Create(
      nullptr, remote.BindNewPipeAndPassReceiver(), mojo::NullRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Original", "Value");

  base::test::TestFuture<int32_t, std::optional<net::HttpRequestHeaders>,
                         std::optional<base::DictValue>>
      future;
  remote->OnBeforeSendHeaders(
      GURL("https://example.com"), headers,
      future.GetCallback<int32_t, const std::optional<net::HttpRequestHeaders>&,
                         std::optional<base::DictValue>>());
  int32_t out_result = future.Get<0>();
  std::optional<net::HttpRequestHeaders> out_headers = future.Get<1>();

  EXPECT_EQ(net::OK, out_result);
  EXPECT_FALSE(out_headers.has_value());
}

// Tests that headers modified by an extension (target client) flow through
// properly when there is no enterprise policy.
TEST_F(HttpHeaderInjectionClientTest, TargetClientModifiesHeaders) {
  MockTrustedHeaderClient target_client;
  EXPECT_CALL(target_client,
              OnBeforeSendHeaders(testing::_, testing::_, testing::_))
      .WillOnce(
          [](const GURL& request_url, const net::HttpRequestHeaders& headers,
             network::mojom::TrustedHeaderClient::OnBeforeSendHeadersCallback
                 callback) {
            net::HttpRequestHeaders modified_headers;
            modified_headers.SetHeader("X-Modified", "ModifiedValue");
            std::move(callback).Run(net::OK, modified_headers, std::nullopt);
          });

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  HttpHeaderInjectionClient::Create(
      HttpHeaderInjectionServiceFactory::GetForProfile(profile_.get())
          ->GetWeakPtr(),
      remote.BindNewPipeAndPassReceiver(), target_client.BindAndGetRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Original", "Value");

  base::test::TestFuture<int32_t, std::optional<net::HttpRequestHeaders>,
                         std::optional<base::DictValue>>
      future;
  remote->OnBeforeSendHeaders(
      GURL("https://example.com"), headers,
      future.GetCallback<int32_t, const std::optional<net::HttpRequestHeaders>&,
                         std::optional<base::DictValue>>());
  int32_t out_result = future.Get<0>();
  std::optional<net::HttpRequestHeaders> out_headers = future.Get<1>();

  EXPECT_EQ(net::OK, out_result);
  ASSERT_TRUE(out_headers.has_value());
  std::optional<std::string> value = out_headers->GetHeader("X-Modified");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ("ModifiedValue", value.value());
  // It shouldn't have X-Original because the target client completely replaced
  // it.
  EXPECT_FALSE(out_headers->GetHeader("X-Original").has_value());
}

// Tests that if the target client returns an error, the error is immediately
// propagated and no injection occurs.
TEST_F(HttpHeaderInjectionClientTest, TargetClientFails) {
  MockTrustedHeaderClient target_client;
  EXPECT_CALL(target_client,
              OnBeforeSendHeaders(testing::_, testing::_, testing::_))
      .WillOnce(
          [](const GURL& request_url, const net::HttpRequestHeaders& headers,
             network::mojom::TrustedHeaderClient::OnBeforeSendHeadersCallback
                 callback) {
            std::move(callback).Run(net::ERR_ABORTED, std::nullopt,
                                    std::nullopt);
          });

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  HttpHeaderInjectionClient::Create(
      HttpHeaderInjectionServiceFactory::GetForProfile(profile_.get())
          ->GetWeakPtr(),
      remote.BindNewPipeAndPassReceiver(), target_client.BindAndGetRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Original", "Value");

  base::test::TestFuture<int32_t, std::optional<net::HttpRequestHeaders>,
                         std::optional<base::DictValue>>
      future;
  remote->OnBeforeSendHeaders(
      GURL("https://example.com"), headers,
      future.GetCallback<int32_t, const std::optional<net::HttpRequestHeaders>&,
                         std::optional<base::DictValue>>());
  int32_t out_result = future.Get<0>();
  std::optional<net::HttpRequestHeaders> out_headers = future.Get<1>();

  EXPECT_EQ(net::ERR_ABORTED, out_result);
  EXPECT_FALSE(out_headers.has_value());
}

// Tests that the enterprise policy successfully injects headers when matched.
TEST_F(HttpHeaderInjectionClientTest, InjectsHeaders) {
  base::HistogramTester histogram_tester;
  SetPolicy("example.com", {{"X-Enterprise", "PolicyValue"}});

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  HttpHeaderInjectionClient::Create(
      HttpHeaderInjectionServiceFactory::GetForProfile(profile_.get())
          ->GetWeakPtr(),
      remote.BindNewPipeAndPassReceiver(), mojo::NullRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Original", "Value");

  base::test::TestFuture<int32_t, std::optional<net::HttpRequestHeaders>,
                         std::optional<base::DictValue>>
      future;
  remote->OnBeforeSendHeaders(
      GURL("https://example.com"), headers,
      future.GetCallback<int32_t, const std::optional<net::HttpRequestHeaders>&,
                         std::optional<base::DictValue>>());
  int32_t out_result = future.Get<0>();
  std::optional<net::HttpRequestHeaders> out_headers = future.Get<1>();

  EXPECT_EQ(net::OK, out_result);
  ASSERT_TRUE(out_headers.has_value());
  std::optional<std::string> value1 = out_headers->GetHeader("X-Original");
  ASSERT_TRUE(value1.has_value());
  EXPECT_EQ("Value", value1.value());
  std::optional<std::string> value2 = out_headers->GetHeader("X-Enterprise");
  ASSERT_TRUE(value2.has_value());
  EXPECT_EQ("PolicyValue", value2.value());

  ValidateSingleHeaderLogEvent(future.Get<2>(), "X-Enterprise", "PolicyValue",
                               /*expected_is_override=*/false);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.HttpHeaderInjection.RequestModified", true, 1);
}

// Tests that enterprise headers are correctly merged with modifications
// made by a target client (like an extension).
TEST_F(HttpHeaderInjectionClientTest, InjectsHeadersWithTargetClient) {
  SetPolicy("example.com", {{"X-Enterprise", "PolicyValue"}});

  MockTrustedHeaderClient target_client;
  EXPECT_CALL(target_client,
              OnBeforeSendHeaders(testing::_, testing::_, testing::_))
      .WillOnce(
          [](const GURL& request_url, const net::HttpRequestHeaders& headers,
             network::mojom::TrustedHeaderClient::OnBeforeSendHeadersCallback
                 callback) {
            net::HttpRequestHeaders modified_headers;
            modified_headers.SetHeader("X-Modified", "ModifiedValue");
            std::move(callback).Run(net::OK, modified_headers, std::nullopt);
          });

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  HttpHeaderInjectionClient::Create(
      HttpHeaderInjectionServiceFactory::GetForProfile(profile_.get())
          ->GetWeakPtr(),
      remote.BindNewPipeAndPassReceiver(), target_client.BindAndGetRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Original", "Value");

  base::test::TestFuture<int32_t, std::optional<net::HttpRequestHeaders>,
                         std::optional<base::DictValue>>
      future;
  remote->OnBeforeSendHeaders(
      GURL("https://example.com"), headers,
      future.GetCallback<int32_t, const std::optional<net::HttpRequestHeaders>&,
                         std::optional<base::DictValue>>());
  int32_t out_result = future.Get<0>();
  std::optional<net::HttpRequestHeaders> out_headers = future.Get<1>();

  EXPECT_EQ(net::OK, out_result);
  ASSERT_TRUE(out_headers.has_value());

  // X-Original is gone because target_client replaced it.
  EXPECT_FALSE(out_headers->GetHeader("X-Original").has_value());

  // X-Modified is present from target_client.
  std::optional<std::string> value1 = out_headers->GetHeader("X-Modified");
  ASSERT_TRUE(value1.has_value());
  EXPECT_EQ("ModifiedValue", value1.value());

  // X-Enterprise is present from policy injection.
  std::optional<std::string> value2 = out_headers->GetHeader("X-Enterprise");
  ASSERT_TRUE(value2.has_value());
  EXPECT_EQ("PolicyValue", value2.value());
}

// Tests that enterprise headers injected by policy overwrite any conflicting
// headers that were added or modified by the target client.
TEST_F(HttpHeaderInjectionClientTest, PolicyOverwritesTargetClientHeaders) {
  SetPolicy("example.com", {{"X-Enterprise", "PolicyValue"}});

  MockTrustedHeaderClient target_client;
  EXPECT_CALL(target_client,
              OnBeforeSendHeaders(testing::_, testing::_, testing::_))
      .WillOnce(
          [](const GURL& request_url, const net::HttpRequestHeaders& headers,
             network::mojom::TrustedHeaderClient::OnBeforeSendHeadersCallback
                 callback) {
            net::HttpRequestHeaders modified_headers = headers;
            // Target client tries to inject the same header the policy uses!
            modified_headers.SetHeader("X-Enterprise", "RogueValue");
            std::move(callback).Run(net::OK, modified_headers, std::nullopt);
          });

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  HttpHeaderInjectionClient::Create(
      HttpHeaderInjectionServiceFactory::GetForProfile(profile_.get())
          ->GetWeakPtr(),
      remote.BindNewPipeAndPassReceiver(), target_client.BindAndGetRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Original", "Value");

  base::test::TestFuture<int32_t, std::optional<net::HttpRequestHeaders>,
                         std::optional<base::DictValue>>
      future;
  remote->OnBeforeSendHeaders(
      GURL("https://example.com"), headers,
      future.GetCallback<int32_t, const std::optional<net::HttpRequestHeaders>&,
                         std::optional<base::DictValue>>());
  int32_t out_result = future.Get<0>();
  std::optional<net::HttpRequestHeaders> out_headers = future.Get<1>();

  EXPECT_EQ(net::OK, out_result);
  ASSERT_TRUE(out_headers.has_value());

  // X-Enterprise must be overwritten by the policy value.
  std::optional<std::string> value = out_headers->GetHeader("X-Enterprise");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ("PolicyValue", value.value());

  ValidateSingleHeaderLogEvent(future.Get<2>(), "X-Enterprise", "PolicyValue",
                               /*expected_is_override=*/true);
}

// Tests that if the target client returns no modifications (std::nullopt),
// those are passed through as-is when there is no enterprise policy.
TEST_F(HttpHeaderInjectionClientTest, TargetClientReturnsNullopt) {
  base::HistogramTester histogram_tester;
  MockTrustedHeaderClient target_client;
  // target_client returns nullopt by default.

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  HttpHeaderInjectionClient::Create(
      HttpHeaderInjectionServiceFactory::GetForProfile(profile_.get())
          ->GetWeakPtr(),
      remote.BindNewPipeAndPassReceiver(), target_client.BindAndGetRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Original", "Value");

  base::test::TestFuture<int32_t, std::optional<net::HttpRequestHeaders>,
                         std::optional<base::DictValue>>
      future;
  remote->OnBeforeSendHeaders(
      GURL("https://example.com"), headers,
      future.GetCallback<int32_t, const std::optional<net::HttpRequestHeaders>&,
                         std::optional<base::DictValue>>());
  int32_t out_result = future.Get<0>();
  std::optional<net::HttpRequestHeaders> out_headers = future.Get<1>();

  EXPECT_EQ(net::OK, out_result);
  EXPECT_FALSE(out_headers.has_value());
  histogram_tester.ExpectUniqueSample(
      "Enterprise.HttpHeaderInjection.RequestModified", false, 1);
}

// Tests that if the target client returns no modifications (std::nullopt),
// enterprise policy modifications are correctly applied to the original
// headers.
TEST_F(HttpHeaderInjectionClientTest, TargetClientReturnsNulloptWithPolicy) {
  SetPolicy("example.com", {{"X-Enterprise", "PolicyValue"}});

  MockTrustedHeaderClient target_client;
  // target_client returns nullopt by default.

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  HttpHeaderInjectionClient::Create(
      HttpHeaderInjectionServiceFactory::GetForProfile(profile_.get())
          ->GetWeakPtr(),
      remote.BindNewPipeAndPassReceiver(), target_client.BindAndGetRemote());

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Original", "Value");

  base::test::TestFuture<int32_t, std::optional<net::HttpRequestHeaders>,
                         std::optional<base::DictValue>>
      future;
  remote->OnBeforeSendHeaders(
      GURL("https://example.com"), headers,
      future.GetCallback<int32_t, const std::optional<net::HttpRequestHeaders>&,
                         std::optional<base::DictValue>>());
  int32_t out_result = future.Get<0>();
  std::optional<net::HttpRequestHeaders> out_headers = future.Get<1>();

  EXPECT_EQ(net::OK, out_result);
  ASSERT_TRUE(out_headers.has_value());

  std::optional<std::string> value1 = out_headers->GetHeader("X-Original");
  ASSERT_TRUE(value1.has_value());
  EXPECT_EQ("Value", value1.value());

  std::optional<std::string> value2 = out_headers->GetHeader("X-Enterprise");
  ASSERT_TRUE(value2.has_value());
  EXPECT_EQ("PolicyValue", value2.value());
}

// Tests that when an admin accidentally configures a policy with multiple
// values for the same header name, the client gracefully handles it and injects
// only the last value (via the matcher's deduplication logic).
TEST_F(HttpHeaderInjectionClientTest, BadConfigMultipleValuesForSameHeader) {
  SetPolicy("example.com",
            {{"X-Duplicate", "FirstValue"}, {"X-Duplicate", "SecondValue"}});

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  HttpHeaderInjectionClient::Create(
      HttpHeaderInjectionServiceFactory::GetForProfile(profile_.get())
          ->GetWeakPtr(),
      remote.BindNewPipeAndPassReceiver(), mojo::NullRemote());

  net::HttpRequestHeaders headers;
  base::test::TestFuture<int32_t, std::optional<net::HttpRequestHeaders>,
                         std::optional<base::DictValue>>
      future;
  remote->OnBeforeSendHeaders(
      GURL("https://example.com/"), headers,
      future.GetCallback<int32_t, const std::optional<net::HttpRequestHeaders>&,
                         std::optional<base::DictValue>>());

  int32_t out_result = future.Get<0>();
  std::optional<net::HttpRequestHeaders> out_headers = future.Get<1>();

  EXPECT_EQ(net::OK, out_result);
  ASSERT_TRUE(out_headers.has_value());

  std::optional<std::string> duplicate_value =
      out_headers->GetHeader("X-Duplicate");
  ASSERT_TRUE(duplicate_value.has_value());
  EXPECT_EQ("SecondValue", duplicate_value.value());

  // Verify only one header is set.
  net::HttpRequestHeaders::Iterator it(*out_headers);
  int header_count = 0;
  while (it.GetNext()) {
    header_count++;
    EXPECT_EQ("X-Duplicate", it.name());
    EXPECT_EQ("SecondValue", it.value());
  }
  EXPECT_EQ(1, header_count);
}

// Tests that OnHeadersReceived passes through properly without a target client.
TEST_F(HttpHeaderInjectionClientTest, HeadersReceivedPassThrough) {
  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  HttpHeaderInjectionClient::Create(
      HttpHeaderInjectionServiceFactory::GetForProfile(profile_.get())
          ->GetWeakPtr(),
      remote.BindNewPipeAndPassReceiver(), mojo::NullRemote());

  base::test::TestFuture<int32_t, std::optional<std::string>,
                         std::optional<GURL>>
      future;
  remote->OnHeadersReceived(
      "HTTP/1.1 200 OK\\n\\n", net::IPEndPoint(), std::nullopt,
      future.GetCallback<int32_t, const std::optional<std::string>&,
                         const std::optional<GURL>&>());
  int32_t out_result = future.Get<0>();
  std::optional<std::string> out_headers = future.Get<1>();

  EXPECT_EQ(net::OK, out_result);
  EXPECT_FALSE(out_headers.has_value());
}

// Tests that OnHeadersReceived passes through modifications from the target
// client.
TEST_F(HttpHeaderInjectionClientTest, HeadersReceivedTargetClientModifies) {
  MockTrustedHeaderClient target_client;
  EXPECT_CALL(target_client,
              OnHeadersReceived(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(
          [](const std::string& headers, const net::IPEndPoint& remote_endpoint,
             const std::optional<net::SSLInfo>& ssl_info,
             network::mojom::TrustedHeaderClient::OnHeadersReceivedCallback
                 callback) {
            std::move(callback).Run(net::OK, "HTTP/1.1 200 OK\nModified: 1\n\n",
                                    std::nullopt);
          });

  mojo::Remote<network::mojom::TrustedHeaderClient> remote;
  HttpHeaderInjectionClient::Create(
      HttpHeaderInjectionServiceFactory::GetForProfile(profile_.get())
          ->GetWeakPtr(),
      remote.BindNewPipeAndPassReceiver(), target_client.BindAndGetRemote());

  base::test::TestFuture<int32_t, std::optional<std::string>,
                         std::optional<GURL>>
      future;
  remote->OnHeadersReceived(
      "HTTP/1.1 200 OK\\n\\n", net::IPEndPoint(), std::nullopt,
      future.GetCallback<int32_t, const std::optional<std::string>&,
                         const std::optional<GURL>&>());
  int32_t out_result = future.Get<0>();
  std::optional<std::string> out_headers = future.Get<1>();

  EXPECT_EQ(net::OK, out_result);
  ASSERT_TRUE(out_headers.has_value());
  EXPECT_EQ("HTTP/1.1 200 OK\nModified: 1\n\n", out_headers.value());
}

}  // namespace enterprise_custom_headers

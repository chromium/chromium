// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/test/test_helper.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/media_router/media_source.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !defined(OS_ANDROID)
#include "base/json/json_reader.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"
#endif

namespace media_router {

MockIssuesObserver::MockIssuesObserver(IssueManager* issue_manager)
    : IssuesObserver(issue_manager) {}
MockIssuesObserver::~MockIssuesObserver() {}

MockMediaSinksObserver::MockMediaSinksObserver(MediaRouter* router,
                                               const MediaSource& source,
                                               const url::Origin& origin)
    : MediaSinksObserver(router, source, origin) {}
MockMediaSinksObserver::~MockMediaSinksObserver() {}

MockMediaRoutesObserver::MockMediaRoutesObserver(
    MediaRouter* router,
    const MediaSource::Id source_id)
    : MediaRoutesObserver(router, source_id) {}
MockMediaRoutesObserver::~MockMediaRoutesObserver() {}

MockPresentationConnectionProxy::MockPresentationConnectionProxy() {}
MockPresentationConnectionProxy::~MockPresentationConnectionProxy() {}

#if !defined(OS_ANDROID)
MockDialMediaSinkService::MockDialMediaSinkService() : DialMediaSinkService() {}
MockDialMediaSinkService::~MockDialMediaSinkService() = default;

MockCastMediaSinkService::MockCastMediaSinkService() : CastMediaSinkService() {}
MockCastMediaSinkService::~MockCastMediaSinkService() = default;

MockCastAppDiscoveryService::MockCastAppDiscoveryService() {}
MockCastAppDiscoveryService::~MockCastAppDiscoveryService() = default;

CastAppDiscoveryService::Subscription
MockCastAppDiscoveryService::StartObservingMediaSinks(
    const CastMediaSource& source,
    const CastAppDiscoveryService::SinkQueryCallback& callback) {
  DoStartObservingMediaSinks(source);
  return callbacks_.Add(callback);
}

MockDialAppDiscoveryService::MockDialAppDiscoveryService() = default;

MockDialAppDiscoveryService::~MockDialAppDiscoveryService() = default;

void MockDialAppDiscoveryService::FetchDialAppInfo(
    const MediaSinkInternal& sink,
    const std::string& app_name,
    DialAppInfoCallback app_info_cb) {
  DoFetchDialAppInfo(sink.sink().id(), app_name);
  app_info_cb_ = std::move(app_info_cb);
}

DialAppDiscoveryService::DialAppInfoCallback
MockDialAppDiscoveryService::PassCallback() {
  return std::move(app_info_cb_);
}

TestDialURLFetcher::TestDialURLFetcher(
    DialURLFetcher::SuccessCallback success_cb,
    DialURLFetcher::ErrorCallback error_cb,
    network::TestURLLoaderFactory* factory)
    : DialURLFetcher(std::move(success_cb), std::move(error_cb)),
      factory_(factory) {}
TestDialURLFetcher::~TestDialURLFetcher() = default;

void TestDialURLFetcher::Start(const GURL& url,
                               const std::string& method,
                               const base::Optional<std::string>& post_data,
                               int max_retries) {
  DoStart(url, method, post_data, max_retries);
  DialURLFetcher::Start(url, method, post_data, max_retries);
}

void TestDialURLFetcher::StartDownload() {
  loader_->DownloadToString(
      factory_,
      base::BindOnce(&DialURLFetcher::ProcessResponse, base::Unretained(this)),
      256 * 1024);
}

TestDialActivityManager::TestDialActivityManager(
    network::TestURLLoaderFactory* factory)
    : DialActivityManager(), factory_(factory) {}
TestDialActivityManager::~TestDialActivityManager() = default;

std::unique_ptr<DialURLFetcher> TestDialActivityManager::CreateFetcher(
    DialURLFetcher::SuccessCallback success_cb,
    DialURLFetcher::ErrorCallback error_cb) {
  OnFetcherCreated();
  auto fetcher = std::make_unique<TestDialURLFetcher>(
      std::move(success_cb), std::move(error_cb), factory_);
  EXPECT_CALL(*fetcher, DoStart(expected_url_, expected_method_,
                                expected_post_data_, testing::_));
  return fetcher;
}

void TestDialActivityManager::SetExpectedRequest(
    const GURL& url,
    const std::string& method,
    const base::Optional<std::string>& post_data) {
  EXPECT_CALL(*this, OnFetcherCreated());
  expected_url_ = url;
  expected_method_ = method;
  expected_post_data_ = post_data;
}

net::IPEndPoint CreateIPEndPoint(int num) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(
      base::StringPrintf("192.168.0.%d", 100 + num)));
  return net::IPEndPoint(ip_address, 8009 + num);
}

MediaSinkInternal CreateDialSink(int num) {
  std::string friendly_name = base::StringPrintf("friendly name %d", num);
  std::string unique_id = base::StringPrintf("dial:<id%d>", num);
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(num);

  media_router::MediaSink sink(unique_id, friendly_name,
                               media_router::SinkIconType::GENERIC,
                               MediaRouteProviderId::EXTENSION);
  media_router::DialSinkExtraData extra_data;
  extra_data.ip_address = ip_endpoint.address();
  extra_data.model_name = base::StringPrintf("model name %d", num);
  extra_data.app_url =
      GURL(base::StringPrintf("http://192.168.0.%d/apps", 100 + num));
  return media_router::MediaSinkInternal(sink, extra_data);
}

MediaSinkInternal CreateCastSink(int num) {
  std::string friendly_name = base::StringPrintf("friendly name %d", num);
  std::string unique_id = base::StringPrintf("cast:<id%d>", num);
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(num);

  MediaSink sink(unique_id, friendly_name, SinkIconType::CAST);
  CastSinkExtraData extra_data;
  extra_data.ip_endpoint = ip_endpoint;
  extra_data.port = ip_endpoint.port();
  extra_data.model_name = base::StringPrintf("model name %d", num);
  extra_data.cast_channel_id = num;
  extra_data.capabilities = cast_channel::CastDeviceCapability::AUDIO_OUT |
                            cast_channel::CastDeviceCapability::VIDEO_OUT;
  return MediaSinkInternal(sink, extra_data);
}

ParsedDialAppInfo CreateParsedDialAppInfo(const std::string& name,
                                          DialAppState app_state) {
  ParsedDialAppInfo app_info;
  app_info.name = name;
  app_info.state = app_state;
  return app_info;
}

std::unique_ptr<ParsedDialAppInfo> CreateParsedDialAppInfoPtr(
    const std::string& name,
    DialAppState app_state) {
  return std::make_unique<ParsedDialAppInfo>(
      CreateParsedDialAppInfo(name, app_state));
}

std::unique_ptr<DialInternalMessage> ParseDialInternalMessage(
    const std::string& message) {
  auto message_value = base::JSONReader::ReadDeprecated(message);
  std::string error_unused;
  return message_value ? DialInternalMessage::From(std::move(*message_value),
                                                   &error_unused)
                       : nullptr;
}

#endif  // !defined(OS_ANDROID)

}  // namespace media_router

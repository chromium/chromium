// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/test/provider_test_helpers.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/router/discovery/dial/dial_app_discovery_service.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/providers/cast/channel/cast_device_capability.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using ::testing::NiceMock;

namespace media_router {

MockDualMediaSinkService::MockDualMediaSinkService()
    : DualMediaSinkService(std::unique_ptr<CastMediaSinkService>(nullptr),
                           std::unique_ptr<DialMediaSinkService>(nullptr),
                           std::unique_ptr<CastAppDiscoveryService>(nullptr)) {}
MockDualMediaSinkService::~MockDualMediaSinkService() = default;

MockDialMediaSinkService::MockDialMediaSinkService() = default;
MockDialMediaSinkService::~MockDialMediaSinkService() = default;

MockCastMediaSinkService::MockCastMediaSinkService() : CastMediaSinkService() {}
MockCastMediaSinkService::~MockCastMediaSinkService() = default;

MockCastAppDiscoveryService::MockCastAppDiscoveryService() = default;
MockCastAppDiscoveryService::~MockCastAppDiscoveryService() = default;

base::CallbackListSubscription
MockCastAppDiscoveryService::StartObservingMediaSinks(
    const CastMediaSource& source,
    const CastAppDiscoveryService::SinkQueryCallback& callback) {
  DoStartObservingMediaSinks(source);
  return callbacks_.Add(callback);
}
scoped_refptr<base::SequencedTaskRunner>
MockCastAppDiscoveryService::task_runner() {
  return content::GetIOThreadTaskRunner({});
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
                               const std::optional<std::string>& post_data,
                               int max_retries,
                               bool set_origin_header) {
  DoStart(url, method, post_data, max_retries);
  DialURLFetcher::Start(url, method, post_data, max_retries, set_origin_header);
}

void TestDialURLFetcher::StartDownload() {
  loader_->DownloadToString(
      factory_,
      base::BindOnce(&DialURLFetcher::ProcessResponse, base::Unretained(this)),
      256 * 1024);
}

TestDeviceDescriptionFetcher::TestDeviceDescriptionFetcher(
    const DialDeviceData& device_data,
    base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb,
    base::OnceCallback<void(const std::string&)> error_cb,
    network::TestURLLoaderFactory* factory)
    : DeviceDescriptionFetcher(device_data,
                               std::move(success_cb),
                               std::move(error_cb)),
      factory_(factory) {}

TestDeviceDescriptionFetcher::~TestDeviceDescriptionFetcher() = default;

void TestDeviceDescriptionFetcher::Start() {
  fetcher_ = std::make_unique<NiceMock<TestDialURLFetcher>>(
      base::BindOnce(&DeviceDescriptionFetcher::ProcessResponse,
                     base::Unretained(this)),
      base::BindOnce(&DeviceDescriptionFetcher::ReportError,
                     base::Unretained(this)),
      factory_);
  fetcher_->Get(device_data_.device_description_url());
}

TestDialActivityManager::TestDialActivityManager(
    DialAppDiscoveryService* app_discovery_service,
    network::TestURLLoaderFactory* factory)
    : DialActivityManager(app_discovery_service), factory_(factory) {}
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
    const std::optional<std::string>& post_data) {
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
  std::string unique_id = base::StringPrintf("dial:id%d", num);
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(num);

  media_router::MediaSink sink(unique_id, friendly_name,
                               media_router::SinkIconType::GENERIC,
                               mojom::MediaRouteProviderId::DIAL);
  media_router::DialSinkExtraData extra_data;
  extra_data.ip_address = ip_endpoint.address();
  extra_data.model_name = base::StringPrintf("model name %d", num);
  extra_data.app_url =
      GURL(base::StringPrintf("http://192.168.0.%d/apps", 100 + num));
  return media_router::MediaSinkInternal(sink, extra_data);
}

MediaSinkInternal CreateCastSink(int num) {
  std::string friendly_name = base::StringPrintf("friendly name %d", num);
  std::string unique_id = base::StringPrintf("cast:id%d", num);
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(num);

  MediaSink sink{CreateCastSink(unique_id, friendly_name)};
  CastSinkExtraData extra_data;
  extra_data.ip_endpoint = ip_endpoint;
  extra_data.port = ip_endpoint.port();
  extra_data.model_name = base::StringPrintf("model name %d", num);
  extra_data.cast_channel_id = num;
  extra_data.capabilities = {cast_channel::CastDeviceCapability::kAudioOut,
                             cast_channel::CastDeviceCapability::kVideoOut};
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
  auto message_value = base::JSONReader::Read(message);
  std::string error_unused;
  return message_value && message_value->is_dict()
             ? DialInternalMessage::From(std::move(message_value->GetDict()),
                                         &error_unused)
             : nullptr;
}

}  // namespace media_router

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_app_discovery_service.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/router/discovery/dial/dial_url_fetcher.h"
#include "chrome/browser/media/router/discovery/dial/parsed_dial_device_description.h"
#include "chrome/browser/media/router/discovery/dial/safe_dial_app_info_parser.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

namespace media_router {

namespace {

constexpr char kYouTubeName[] = "YouTube";

class TestSafeDialAppInfoParser : public SafeDialAppInfoParser {
 public:
  TestSafeDialAppInfoParser() = default;
  ~TestSafeDialAppInfoParser() override = default;

  MOCK_METHOD(void, ParseInternal, (const std::string& xml_text));

  void Parse(const std::string& xml_text, ParseCallback callback) override {
    parse_callback_ = std::move(callback);
    ParseInternal(xml_text);
  }

  void InvokeParseCallback(std::unique_ptr<ParsedDialAppInfo> app_info,
                           ParsingResult parsing_result) {
    if (!parse_callback_)
      return;
    std::move(parse_callback_).Run(std::move(app_info), parsing_result);
  }

 private:
  ParseCallback parse_callback_;
};

}  // namespace

class DialAppDiscoveryServiceTest : public ::testing::Test {
 public:
  DialAppDiscoveryServiceTest()
      : test_parser_(new TestSafeDialAppInfoParser()) {
    dial_app_discovery_service_.SetParserForTest(
        std::unique_ptr<TestSafeDialAppInfoParser>(test_parser_));
  }

  MOCK_METHOD4(OnAppInfoSuccess,
               void(const MediaSink::Id&,
                    const std::string&,
                    const ParsedDialAppInfo&,
                    DialAppInfoResultCode));
  MOCK_METHOD3(OnAppInfoFailure,
               void(const MediaSink::Id&,
                    const std::string&,
                    DialAppInfoResultCode));

  void OnAppInfo(const MediaSink::Id& sink_id,
                 const std::string& app_name,
                 DialAppInfoResult result) {
    if (result.app_info)
      OnAppInfoSuccess(sink_id, app_name, *result.app_info, result.result_code);
    else
      OnAppInfoFailure(sink_id, app_name, result.result_code);
  }

  // Returns a raw pointer to the PendingRequest tracked in
  // |dial_app_discovery_service_|. Guaranteed to live until either
  // |OnDialAppInfoFetchComplete()| or |OnDialAppInfoFetchError()| is called.
  DialAppDiscoveryService::PendingRequest* AddFetchRequest(
      const MediaSinkInternal& sink,
      const std::string& app_name) {
    auto request = std::make_unique<DialAppDiscoveryService::PendingRequest>(
        sink, app_name,
        base::BindOnce(&DialAppDiscoveryServiceTest::OnAppInfo,
                       base::Unretained(this)),
        &dial_app_discovery_service_);
    auto* request_ptr = request.get();
    dial_app_discovery_service_.pending_requests_.push_back(std::move(request));
    return request_ptr;
  }

  void OnDialAppInfoFetchComplete(
      DialAppDiscoveryService::PendingRequest* request,
      const std::string& xml) {
    request->OnDialAppInfoFetchComplete(xml);
  }

  void OnDialAppInfoFetchError(DialAppDiscoveryService::PendingRequest* request,
                               std::optional<int> response_code,
                               const std::string& error_text) {
    request->OnDialAppInfoFetchError(error_text, response_code);
  }

  void TearDown() override {
    dial_app_discovery_service_.pending_requests_.clear();
  }

 protected:
  raw_ptr<TestSafeDialAppInfoParser, DanglingUntriaged> test_parser_;
  DialAppDiscoveryService dial_app_discovery_service_;

  // Must be on Chrome_UIThread, as `OnDialAppInfoFetchComplete` uses a
  // LoggerList instance which requires UI thread.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DialAppDiscoveryServiceTest, TestFetchDialAppInfoFetchURL) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  const MediaSink::Id& sink_id = dial_sink.sink().id();
  ParsedDialAppInfo parsed_app_info =
      CreateParsedDialAppInfo(kYouTubeName, DialAppState::kRunning);
  auto* request = AddFetchRequest(dial_sink, kYouTubeName);

  EXPECT_CALL(*test_parser_, ParseInternal(_))
      .WillOnce(Invoke([&](const std::string& xml_text) {
        test_parser_->InvokeParseCallback(
            std::make_unique<ParsedDialAppInfo>(parsed_app_info),
            SafeDialAppInfoParser::ParsingResult::kSuccess);
      }));
  EXPECT_CALL(*this, OnAppInfoSuccess(sink_id, kYouTubeName, parsed_app_info,
                                      DialAppInfoResultCode::kOk));
  OnDialAppInfoFetchComplete(request, "<xml>appInfo</xml>");
}

TEST_F(DialAppDiscoveryServiceTest,
       TestFetchDialAppInfoFetchURLTransientError) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  const MediaSink::Id& sink_id = dial_sink.sink().id();
  auto* request = AddFetchRequest(dial_sink, kYouTubeName);

  EXPECT_CALL(*this, OnAppInfoFailure(sink_id, _,
                                      DialAppInfoResultCode::kNetworkError));
  OnDialAppInfoFetchError(request, std::nullopt, "Temporarily throttled");
}

TEST_F(DialAppDiscoveryServiceTest, TestFetchDialAppInfoFetchURLError) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  const MediaSink::Id& sink_id = dial_sink.sink().id();
  auto* request = AddFetchRequest(dial_sink, kYouTubeName);
  EXPECT_CALL(*this,
              OnAppInfoFailure(sink_id, _, DialAppInfoResultCode::kHttpError));
  OnDialAppInfoFetchError(request, net::HTTP_NOT_FOUND, "Not found");
}

TEST_F(DialAppDiscoveryServiceTest, TestFetchDialAppInfoErrorWithHttpSuccess) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  const MediaSink::Id& sink_id = dial_sink.sink().id();
  auto* request = AddFetchRequest(dial_sink, kYouTubeName);

  // If the HTTP response code unexpectedly is in the 200s, we treat it as a
  // parsing error.
  EXPECT_CALL(*this, OnAppInfoFailure(sink_id, _,
                                      DialAppInfoResultCode::kParsingError));
  OnDialAppInfoFetchError(request, net::HTTP_OK, "Bad encoding");
}

TEST_F(DialAppDiscoveryServiceTest, TestFetchDialAppInfoParseError) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  const MediaSink::Id& sink_id = dial_sink.sink().id();
  auto* request = AddFetchRequest(dial_sink, kYouTubeName);
  EXPECT_CALL(*test_parser_, ParseInternal(_))
      .WillOnce(Invoke([&](const std::string& xml_text) {
        test_parser_->InvokeParseCallback(
            nullptr, SafeDialAppInfoParser::ParsingResult::kMissingName);
      }));
  EXPECT_CALL(*this, OnAppInfoFailure(sink_id, kYouTubeName,
                                      DialAppInfoResultCode::kParsingError));
  OnDialAppInfoFetchComplete(request, "<xml>appInfo</xml>");
}

}  // namespace media_router

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_url_loader.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace cloud_print {

namespace {

const char kSamplePrivetURL[] =
    "http://10.0.0.8:7676/privet/register?action=start";
const char kSamplePrivetToken[] = "MyToken";
const char kEmptyPrivetToken[] = "\"\"";

const char kSampleParsableJSON[] = "{ \"hello\" : 2 }";
const char kSampleUnparsableJSON[] = "{ \"hello\" : }";
const char kSampleJSONWithError[] = "{ \"error\" : \"unittest_example\" }";
const net::HttpStatusCode kHTTPErrorCodeInvalidXPrivetToken =
    static_cast<net::HttpStatusCode>(418);

class MockPrivetURLLoaderDelegate : public PrivetURLLoader::Delegate {
 public:
  MockPrivetURLLoaderDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : raw_mode_(false), task_runner_(task_runner) {}

  ~MockPrivetURLLoaderDelegate() override {}

  MOCK_METHOD2(OnError,
               void(int response_code, PrivetURLLoader::ErrorType error));

  void OnParsedJson(int response_code,
                    const base::DictionaryValue& value,
                    bool has_error) override {
    saved_value_.reset(value.DeepCopy());
    OnParsedJsonInternal(has_error);
  }

  MOCK_METHOD1(OnParsedJsonInternal, void(bool has_error));

  void OnNeedPrivetToken(PrivetURLLoader::TokenCallback callback) override {
    auto closure = base::BindOnce(
        [](PrivetURLLoader::TokenCallback callback) {
          std::move(callback).Run(kSamplePrivetToken);
        },
        std::move(callback));
    task_runner_->PostTask(FROM_HERE, std::move(closure));
    OnNeedPrivetTokenInternal();
  }

  MOCK_METHOD0(OnNeedPrivetTokenInternal, void());

  bool OnRawData(bool response_is_file,
                 const std::string& data,
                 const base::FilePath& response_file) override {
    if (!raw_mode_)
      return false;

    if (response_is_file) {
      EXPECT_TRUE(response_file != base::FilePath());
      OnFileInternal(response_file);
    } else {
      OnRawDataInternal(data);
    }

    return true;
  }

  MOCK_METHOD1(OnRawDataInternal, void(const std::string& data));

  MOCK_METHOD1(OnFileInternal, void(const base::FilePath& response_file));

  const base::DictionaryValue* saved_value() { return saved_value_.get(); }

  void SetRawMode(bool raw_mode) { raw_mode_ = raw_mode; }

 private:
  std::unique_ptr<base::DictionaryValue> saved_value_;
  bool raw_mode_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class PrivetURLLoaderTest : public ::testing::Test {
 public:
  PrivetURLLoaderTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        delegate_(task_environment_.GetMainThreadTaskRunner()) {
    privet_url_loader_ = std::make_unique<PrivetURLLoader>(
        GURL(kSamplePrivetURL), "POST", test_shared_url_loader_factory_,
        TRAFFIC_ANNOTATION_FOR_TESTS, &delegate_);

    PrivetURLLoader::SetTokenForHost(GURL(kSamplePrivetURL).GetOrigin().spec(),
                                     kSamplePrivetToken);
  }
  ~PrivetURLLoaderTest() override {}

 protected:
  void StartPrivetURLLoaderAndCaptureHeaders(net::HttpRequestHeaders* headers) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          *headers = request.headers;
        }));
    privet_url_loader_->Start();
    base::RunLoop().RunUntilIdle();
    test_url_loader_factory_.SetInterceptor(base::NullCallback());
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  std::unique_ptr<PrivetURLLoader> privet_url_loader_;
  testing::StrictMock<MockPrivetURLLoaderDelegate> delegate_;
};

TEST_F(PrivetURLLoaderTest, FetchSuccess) {
  test_url_loader_factory_.AddResponse(kSamplePrivetURL, kSampleParsableJSON);

  EXPECT_CALL(delegate_, OnParsedJsonInternal(false));
  privet_url_loader_->Start();
  base::RunLoop().RunUntilIdle();

  const base::DictionaryValue* value = delegate_.saved_value();
  int hello_value;
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->GetInteger("hello", &hello_value));
  EXPECT_EQ(2, hello_value);
}

// An interceptor used by the HTTP503Retry test that returns multiple failures
// then success.
class RetryURLLoaderInterceptor {
 public:
  explicit RetryURLLoaderInterceptor(
      network::TestURLLoaderFactory* test_url_loader_factory)
      : test_url_loader_factory_(test_url_loader_factory) {
    test_url_loader_factory_->SetInterceptor(base::BindRepeating(
        &RetryURLLoaderInterceptor::InterceptURL, base::Unretained(this)));
  }

  void InterceptURL(const network::ResourceRequest& resource_request) {
    if (resource_request.url != kSamplePrivetURL)
      return;
    net::HttpStatusCode status =
        counter_++ < 3 ? net::HTTP_SERVICE_UNAVAILABLE : net::HTTP_OK;
    test_url_loader_factory_->AddResponse(kSamplePrivetURL, kSampleParsableJSON,
                                          status);
  }

 private:
  network::TestURLLoaderFactory* test_url_loader_factory_;
  int counter_ = 0;
  GURL url_;
};

TEST_F(PrivetURLLoaderTest, HTTP503Retry) {
  PrivetURLLoader::RetryImmediatelyForTest retry_immediately;

  // The interceptor simulates the service being unavailable and then available.
  RetryURLLoaderInterceptor interceptor(&test_url_loader_factory_);

  EXPECT_CALL(delegate_, OnParsedJsonInternal(false));
  privet_url_loader_->Start();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PrivetURLLoaderTest, ResponseCodeError) {
  test_url_loader_factory_.AddResponse(kSamplePrivetURL, kSampleParsableJSON,
                                       net::HTTP_NOT_FOUND);
  EXPECT_CALL(delegate_, OnError(_, PrivetURLLoader::RESPONSE_CODE_ERROR));
  privet_url_loader_->Start();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PrivetURLLoaderTest, JsonParseError) {
  test_url_loader_factory_.AddResponse(kSamplePrivetURL, kSampleUnparsableJSON);
  EXPECT_CALL(delegate_, OnError(_, PrivetURLLoader::JSON_PARSE_ERROR));
  privet_url_loader_->Start();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PrivetURLLoaderTest, Header) {
  net::HttpRequestHeaders request_headers;
  StartPrivetURLLoaderAndCaptureHeaders(&request_headers);

  std::string header_token;
  ASSERT_TRUE(request_headers.GetHeader("X-Privet-Token", &header_token));
  EXPECT_EQ(kSamplePrivetToken, header_token);
}

TEST_F(PrivetURLLoaderTest, Header2) {
  PrivetURLLoader::SetTokenForHost(GURL(kSamplePrivetURL).GetOrigin().spec(),
                                   "");
  privet_url_loader_->SendEmptyPrivetToken();

  net::HttpRequestHeaders request_headers;
  StartPrivetURLLoaderAndCaptureHeaders(&request_headers);

  std::string header_token;
  ASSERT_TRUE(request_headers.GetHeader("X-Privet-Token", &header_token));
  EXPECT_EQ(kEmptyPrivetToken, header_token);
}

TEST_F(PrivetURLLoaderTest, AlwaysSendEmpty) {
  PrivetURLLoader::SetTokenForHost(GURL(kSamplePrivetURL).GetOrigin().spec(),
                                   "SampleToken");

  privet_url_loader_->SendEmptyPrivetToken();

  net::HttpRequestHeaders request_headers;
  StartPrivetURLLoaderAndCaptureHeaders(&request_headers);

  std::string header_token;
  ASSERT_TRUE(request_headers.GetHeader("X-Privet-Token", &header_token));
  EXPECT_EQ(kEmptyPrivetToken, header_token);
}

TEST_F(PrivetURLLoaderTest, HandleInvalidToken) {
  privet_url_loader_->SetMaxRetriesForTest(1);
  test_url_loader_factory_.AddResponse(kSamplePrivetURL, kSampleParsableJSON,
                                       kHTTPErrorCodeInvalidXPrivetToken);
  EXPECT_CALL(delegate_, OnNeedPrivetTokenInternal());
  EXPECT_CALL(delegate_, OnError(0, PrivetURLLoader::UNKNOWN_ERROR));
  privet_url_loader_->Start();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PrivetURLLoaderTest, FetchHasError) {
  test_url_loader_factory_.AddResponse(kSamplePrivetURL, kSampleJSONWithError);
  EXPECT_CALL(delegate_, OnParsedJsonInternal(true));
  privet_url_loader_->Start();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PrivetURLLoaderTest, LoaderRawData) {
  delegate_.SetRawMode(true);
  test_url_loader_factory_.AddResponse(kSamplePrivetURL, kSampleJSONWithError);
  EXPECT_CALL(delegate_, OnRawDataInternal(kSampleJSONWithError));
  privet_url_loader_->Start();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PrivetURLLoaderTest, RangeRequest) {
  delegate_.SetRawMode(true);
  privet_url_loader_->SetByteRange(200, 300);

  net::HttpRequestHeaders request_headers;
  StartPrivetURLLoaderAndCaptureHeaders(&request_headers);

  std::string header_range;
  ASSERT_TRUE(request_headers.GetHeader("Range", &header_range));
  EXPECT_EQ("bytes=200-300", header_range);
}

TEST_F(PrivetURLLoaderTest, LoaderToFile) {
  test_url_loader_factory_.AddResponse(kSamplePrivetURL, kSampleParsableJSON);
  delegate_.SetRawMode(true);
  privet_url_loader_->SaveResponseToFile();
  // Downloading to a file bounce to another thread for the write so we cannot
  // just use RunLoop::RunUntilIdle() and have to wait for the callback.
  base::FilePath response_file;
  base::RunLoop run_loop;
  EXPECT_CALL(delegate_, OnFileInternal(testing::_))
      .WillOnce(testing::Invoke([&](const base::FilePath& response_file_param) {
        response_file = response_file_param;
        run_loop.Quit();
      }));
  privet_url_loader_->Start();
  run_loop.Run();

  std::string file_content;
  ASSERT_TRUE(base::ReadFileToString(response_file, &file_content));
  EXPECT_EQ(kSampleParsableJSON, file_content);
}

}  // namespace

}  // namespace cloud_print

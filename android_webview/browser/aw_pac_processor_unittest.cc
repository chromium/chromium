// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <android/multinetwork.h>
#include <string>

#include "android_webview/browser/aw_pac_processor.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

namespace {
const std::string kScript =
    "function FindProxyForURL(url, host)\n"
    "{\n"
    "\treturn \"PROXY localhost:8080; PROXY localhost:8081; DIRECT \";\n"
    "}\n";

const std::string kScriptDnsResolve =
    "var x = dnsResolveEx(\"localhost\");\n"
    "function FindProxyForURL(url, host) {\n"
    "\treturn \"PROXY \" + x + \":80\";\n"
    "}";

const std::string kRequestUrl = "http://testurl.test";
}  // namespace

class AwPacProcessorTest : public testing::Test {
 public:
  void TearDown() override { delete pac_processor_; }

 protected:
  base::test::TaskEnvironment task_environment_{
           base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<AwPacProcessor> pac_processor_ = new AwPacProcessor();
};

TEST_F(AwPacProcessorTest, MakeProxyRequest) {
  EXPECT_TRUE(pac_processor_->SetProxyScript(kScript));
  std::string result;
  EXPECT_TRUE(pac_processor_->MakeProxyRequest(kRequestUrl, &result));
  EXPECT_EQ("PROXY localhost:8080;PROXY localhost:8081;DIRECT", result);
}

TEST_F(AwPacProcessorTest, MakeProxyRequestDnsResolve) {
  EXPECT_TRUE(pac_processor_->SetProxyScript(kScriptDnsResolve));
  std::string result;
  EXPECT_TRUE(pac_processor_->MakeProxyRequest(kRequestUrl, &result));
  EXPECT_EQ("PROXY 127.0.0.1:80", result);
}

TEST_F(AwPacProcessorTest, MultipleProxyRequest) {
  AwPacProcessor* other_pac_processor_ = new AwPacProcessor();
  EXPECT_TRUE(pac_processor_->SetProxyScript(kScript));
  EXPECT_TRUE(other_pac_processor_->SetProxyScript(kScriptDnsResolve));

  std::string result;
  EXPECT_TRUE(pac_processor_->MakeProxyRequest(kRequestUrl, &result));
  EXPECT_EQ("PROXY localhost:8080;PROXY localhost:8081;DIRECT", result);

  EXPECT_TRUE(other_pac_processor_->MakeProxyRequest(kRequestUrl, &result));
  EXPECT_EQ("PROXY 127.0.0.1:80", result);
  delete other_pac_processor_;
}

TEST_F(AwPacProcessorTest, UnparseableScript) {
  EXPECT_FALSE(pac_processor_->SetProxyScript(""));
  std::string result;
  EXPECT_FALSE(pac_processor_->MakeProxyRequest(kRequestUrl, &result));
}

}  // namespace android_webview

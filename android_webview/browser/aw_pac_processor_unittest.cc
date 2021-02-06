// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <android/multinetwork.h>
#include <string>

#include "android_webview/browser/aw_pac_processor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "base/test/task_environment.h"

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
  AwPacProcessor* pac_processor_ = new AwPacProcessor();
};

TEST_F(AwPacProcessorTest, MakeProxyRequest) {
  pac_processor_->SetProxyScript(kScript);
  EXPECT_EQ("PROXY localhost:8080;PROXY localhost:8081;DIRECT",
            pac_processor_->MakeProxyRequest(kRequestUrl));
}

TEST_F(AwPacProcessorTest, MakeProxyRequestDnsResolve) {
  pac_processor_->SetProxyScript(kScriptDnsResolve);
  EXPECT_EQ("PROXY 127.0.0.1:80",
            pac_processor_->MakeProxyRequest(kRequestUrl));
}

TEST_F(AwPacProcessorTest, MultipleProxyRequest) {
  AwPacProcessor* other_pac_processor_ = new AwPacProcessor();
  pac_processor_->SetProxyScript(kScript);
  other_pac_processor_->SetProxyScript(kScriptDnsResolve);

  EXPECT_EQ("PROXY localhost:8080;PROXY localhost:8081;DIRECT",
            pac_processor_->MakeProxyRequest(kRequestUrl));

  EXPECT_EQ("PROXY 127.0.0.1:80",
            other_pac_processor_->MakeProxyRequest(kRequestUrl));
  delete other_pac_processor_;
}

}  // namespace android_webview

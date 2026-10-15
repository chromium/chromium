// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "android_webview/browser/aw_pac_processor.h"

#include <android/multinetwork.h>

#include <array>
#include <cstddef>
#include <string>
#include <thread>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/spin_wait.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
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

const std::string kScriptMyIpAddress =
    "function FindProxyForURL(url, host) {\n"
    "\treturn \"PROXY \" + myIpAddress() + \":80\";\n"
    "}";

const std::string kScriptBlockingLoop =
    "function FindProxyForURL(url, host) {\n"
    "\twhile (true) {\n"
    "\t\tdnsResolve(\"\");\n"
    "\t}\n"
    "\treturn \"DIRECT\";\n"
    "}\n";

const std::string kRequestUrl = "http://testurl.test";
}  // namespace

class AwPacProcessorTest : public testing::Test {
 public:
  void TearDown() override {
    pac_processor_.ExtractAsDangling()->DestroyNative();
  }

 protected:
  size_t GetNumPendingJobs(const AwPacProcessor& processor) const {
    // `jobs_` is only accessed on the AwPacProcessor task runner.
    size_t num_jobs = 0;
    base::WaitableEvent event;
    AwPacProcessor::GetTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          num_jobs = processor.jobs_.size();
          event.Signal();
        }));
    event.Wait();
    return num_jobs;
  }

  base::test::TaskEnvironment task_environment_;
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

TEST_F(AwPacProcessorTest, MyIpAddressWithUnspecifiedNetwork) {
  pac_processor_->SetNetworkAndLinkAddresses(NETWORK_UNSPECIFIED, {});
  EXPECT_TRUE(pac_processor_->SetProxyScript(kScriptMyIpAddress));

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
  other_pac_processor_->DestroyNative();
}

TEST_F(AwPacProcessorTest, UnparseableScript) {
  EXPECT_FALSE(pac_processor_->SetProxyScript(""));
  std::string result;
  EXPECT_FALSE(pac_processor_->MakeProxyRequest(kRequestUrl, &result));
}

TEST_F(AwPacProcessorTest, DestroyWithPendingJobs) {
  AwPacProcessor* processor = new AwPacProcessor();
  ASSERT_TRUE(processor->SetProxyScript(kScriptBlockingLoop));

  constexpr size_t kNumJobs = 3;
  std::array<std::thread, kNumJobs> threads;
  std::array<bool, kNumJobs> results{};

  for (size_t i = 0; i < kNumJobs; ++i) {
    threads[i] = std::thread([processor, &results, i]() {
      std::string proxy_result;
      results[i] = processor->MakeProxyRequest(kRequestUrl, &proxy_result);
    });
  }

  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(TestTimeouts::action_timeout(),
                                   GetNumPendingJobs(*processor) == kNumJobs);

  processor->DestroyNative();

  for (size_t i = 0; i < kNumJobs; ++i) {
    threads[i].join();
    EXPECT_FALSE(results[i]);
  }
}

}  // namespace android_webview

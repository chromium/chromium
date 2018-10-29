// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_runner.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/net/dns_probe_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::RunLoop;
using content::TestBrowserThreadBundle;
using net::DnsClient;
using net::DnsConfig;
using net::MockDnsClientRule;

namespace chrome_browser_net {

namespace {

class TestDnsProbeRunnerCallback {
 public:
  TestDnsProbeRunnerCallback()
      : callback_(base::Bind(&TestDnsProbeRunnerCallback::OnCalled,
                             base::Unretained(this))),
        called_(false) {}

  const base::Closure& callback() const { return callback_; }
  bool called() const { return called_; }

 private:
  void OnCalled() {
    EXPECT_FALSE(called_);
    called_ = true;
  }

  base::Closure callback_;
  bool called_;
};

class DnsProbeRunnerTest : public testing::Test {
 protected:
  void RunTest(MockDnsClientRule::ResultType query_result,
               DnsProbeRunner::Result expected_probe_result);

  TestBrowserThreadBundle bundle_;
  DnsProbeRunner runner_;
};

void DnsProbeRunnerTest::RunTest(
    MockDnsClientRule::ResultType query_result,
    DnsProbeRunner::Result expected_probe_result) {
  TestDnsProbeRunnerCallback callback;

  runner_.SetClient(CreateMockDnsClientForProbes(query_result));
  runner_.RunProbe(callback.callback());
  EXPECT_TRUE(runner_.IsRunning());

  RunLoop().RunUntilIdle();
  EXPECT_FALSE(runner_.IsRunning());
  EXPECT_TRUE(callback.called());
  EXPECT_EQ(expected_probe_result, runner_.result());
}

TEST_F(DnsProbeRunnerTest, Probe_OK) {
  RunTest(MockDnsClientRule::OK, DnsProbeRunner::CORRECT);
}

TEST_F(DnsProbeRunnerTest, Probe_EMPTY) {
  RunTest(MockDnsClientRule::EMPTY, DnsProbeRunner::INCORRECT);
}

TEST_F(DnsProbeRunnerTest, Probe_TIMEOUT) {
  RunTest(MockDnsClientRule::TIMEOUT, DnsProbeRunner::UNREACHABLE);
}

TEST_F(DnsProbeRunnerTest, Probe_FAIL) {
  RunTest(MockDnsClientRule::FAIL, DnsProbeRunner::INCORRECT);
}

TEST_F(DnsProbeRunnerTest, TwoProbes) {
  RunTest(MockDnsClientRule::OK, DnsProbeRunner::CORRECT);
  RunTest(MockDnsClientRule::EMPTY, DnsProbeRunner::INCORRECT);
}

TEST_F(DnsProbeRunnerTest, InvalidDnsConfig) {
  std::unique_ptr<DnsClient> dns_client(DnsClient::CreateClient(NULL));
  DnsConfig empty_config;
  dns_client->SetConfig(empty_config);
  ASSERT_EQ(NULL, dns_client->GetTransactionFactory());
  runner_.SetClient(std::move(dns_client));

  TestDnsProbeRunnerCallback callback;

  runner_.RunProbe(callback.callback());
  EXPECT_TRUE(runner_.IsRunning());

  RunLoop().RunUntilIdle();
  EXPECT_FALSE(runner_.IsRunning());
  EXPECT_TRUE(callback.called());
  EXPECT_EQ(DnsProbeRunner::UNKNOWN, runner_.result());
}

}  // namespace

}  // namespace chrome_browser_net

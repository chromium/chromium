// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"

// Base class for tests that want to record a net log. The subclass should
// implement the VerifyNetLog method which will be called after the test body
// completes.
class NetLogPlatformBrowserTestBase : public PlatformBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    net_log_path_ = tmp_dir_.GetPath().AppendASCII("netlog.json");
    command_line->AppendSwitchPath(network::switches::kLogNetLog,
                                   net_log_path_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    // When using the --log-net-log command line param, the net log is
    // finalized during the destruction of the network service, which is
    // started before this method is called, but completes asynchronously.
    //
    // Try for up to 5 seconds to read the netlog file.
    constexpr auto kMaxWaitTime = base::Seconds(5);
    constexpr auto kWaitInterval = base::Milliseconds(50);
    int tries_left = kMaxWaitTime / kWaitInterval;

    std::optional<base::Value> parsed_net_log;
    while (true) {
      std::string file_contents;
      ASSERT_TRUE(base::ReadFileToString(net_log_path_, &file_contents));

      parsed_net_log = base::JSONReader::Read(file_contents);
      if (parsed_net_log)
        break;

      if (--tries_left <= 0)
        break;
      // The netlog file did not parse as valid JSON. Probably the Network
      // Service is still shutting down. Wait a bit and try again.
      base::PlatformThread::Sleep(kWaitInterval);
    }
    ASSERT_TRUE(parsed_net_log);

    VerifyNetLog(&parsed_net_log.value());

    PlatformBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  // Subclasses should override this to implement the test verification
  // conditions. It will be called after the test fixture has been torn down.
  virtual void VerifyNetLog(base::Value* parsed_net_log) = 0;

 protected:
  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  base::FilePath net_log_path_;
  base::ScopedTempDir tmp_dir_;
};

// This is an integration test to ensure that CertVerifyProc netlog events
// continue to be logged even though cert verification is no longer performed in
// the network process.
class CertVerifyProcNetLogBrowserTest : public NetLogPlatformBrowserTestBase {
 public:
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.ServeFilesFromSourceDirectory("chrome/test/data/");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  }

  void VerifyNetLog(base::Value* parsed_net_log) override {
    base::Value::Dict* main = parsed_net_log->GetIfDict();
    ASSERT_TRUE(main);

    base::Value::List* events = main->FindList("events");
    ASSERT_TRUE(events);

    bool found_cert_verify_proc_event = false;
    for (const auto& event_val : *events) {
      ASSERT_TRUE(event_val.is_dict());
      const base::Value::Dict& event = event_val.GetDict();
      std::optional<int> event_type = event.FindInt("type");
      ASSERT_TRUE(event_type.has_value());
      if (event_type ==
          static_cast<int>(net::NetLogEventType::CERT_VERIFY_PROC)) {
        std::optional<int> phase = event.FindInt("phase");
        if (!phase.has_value() ||
            *phase != static_cast<int>(net::NetLogEventPhase::BEGIN)) {
          continue;
        }
        const base::Value::Dict* params = event.FindDict("params");
        if (!params)
          continue;
        const std::string* host = params->FindString("host");
        if (host && *host == kTestHost) {
          found_cert_verify_proc_event = true;
          break;
        }
      }
    }

    EXPECT_TRUE(found_cert_verify_proc_event);
  }

  const std::string kTestHost = "netlog-example.a.test";

 protected:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(CertVerifyProcNetLogBrowserTest, Test) {
  ASSERT_TRUE(https_server_.Start());

  // Request using a unique host name to ensure that the cert verification wont
  // use a cached result for 127.0.0.1 that happened before the test starts
  // logging.
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      https_server_.GetURL(kTestHost, "/ssl/google.html")));

  // Technically there is no guarantee that if the cert verifier is running out
  // of process that the netlog mojo messages will be delivered before the cert
  // verification mojo result. See:
  // https://chromium.googlesource.com/chromium/src/+/main/docs/mojo_ipc_conversion.md#Ordering-Considerations
  // Hopefully this won't be flaky.
  base::RunLoop().RunUntilIdle();
  content::FlushNetworkServiceInstanceForTesting();
}

using AIABrowserTest = PlatformBrowserTest;

IN_PROC_BROWSER_TEST_F(AIABrowserTest, TestHTTPSAIA) {
  net::EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate = net::EmbeddedTestServer::IntermediateType::kByAIA;

  net::EmbeddedTestServer https_server{net::EmbeddedTestServer::TYPE_HTTPS};
  https_server.SetSSLConfig(cert_config);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data/");
  ASSERT_TRUE(https_server.Start());

  EXPECT_TRUE(
      content::NavigateToURL(chrome_test_utils::GetActiveWebContents(this),
                             https_server.GetURL("/simple.html")));
  ssl_test_util::CheckAuthenticatedState(
      chrome_test_utils::GetActiveWebContents(this),
      ssl_test_util::AuthState::NONE);
}

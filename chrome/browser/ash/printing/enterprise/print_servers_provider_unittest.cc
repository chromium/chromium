// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/print_servers_provider.h"

#include <string>
#include <vector>

#include "chrome/browser/ash/printing/print_server.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace {

// An example of configuration file with print servers.
constexpr char kPrintServersPolicyJson1[] = R"json(
[
  {
    "id": "id1",
    "display_name": "MyPrintServer",
    "url": "ipp://192.168.1.5"
  }, {
    "id": "id2",
    "display_name": "Server API",
    "url":"ipps://print-server.intra.example.com:444/ipp/cl2k4"
  }, {
    "id": "id3",
    "display_name": "YaLP",
    "url": "http://192.168.1.8/bleble/print"
  }
])json";

PrintServer Server1() {
  return {"id1", GURL("http://192.168.1.5:631"), "MyPrintServer"};
}

PrintServer Server3() {
  return {"id3", GURL("http://192.168.1.8/bleble/print"), "YaLP"};
}

// Corresponding vector with PrintServers.
std::vector<PrintServer> PrintServersPolicyData1() {
  return {{Server1(),
           {"id2", GURL("https://print-server.intra.example.com:444/ipp/cl2k4"),
            "Server API"},
           Server3()}};
}

// Observer that stores all its calls.
class TestObserver : public PrintServersProvider::Observer {
 public:
  struct ObserverCall {
    bool complete;
    std::vector<PrintServer> servers;
    ObserverCall(bool complete, std::vector<PrintServer> servers)
        : complete(complete), servers(std::move(servers)) {}
  };

  ~TestObserver() override = default;

  // Callback from PrintServersProvider::Observer.
  void OnServersChanged(bool complete,
                        const std::vector<PrintServer>& servers) override {
    calls_.emplace_back(complete, servers);
  }

  // Returns history of all calls to OnServersChanged(...).
  const std::vector<ObserverCall>& GetCalls() const { return calls_; }

 private:
  // history of all callbacks
  std::vector<ObserverCall> calls_;
};

class PrintServersProviderTest : public testing::Test {
 public:
  PrintServersProviderTest() = default;

 protected:
  void SetUp() override {
    pref_service_.registry()->RegisterListPref(kAllowlistPrefName);
    external_servers_->SetAllowlistPref(&pref_service_, kAllowlistPrefName);
  }

  static constexpr char kAllowlistPrefName[] = "test";

  // Everything must be called on Chrome_UIThread.
  content::BrowserTaskEnvironment task_environment_;
  // Test prefs.
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  // Tested object.
  std::unique_ptr<PrintServersProvider> external_servers_ =
      PrintServersProvider::Create();
};

// static
constexpr char PrintServersProviderTest::kAllowlistPrefName[];

// Verify that the object can be destroyed while parsing is in progress.
TEST_F(PrintServersProviderTest, DestructionIsSafe) {
  {
    std::unique_ptr<PrintServersProvider> servers =
        PrintServersProvider::Create();
    servers->SetData(std::make_unique<std::string>(kPrintServersPolicyJson1));
    // Data is valid.  Computation is proceeding.
  }
  // servers is out of scope.  Destructor has run.  Pump the message queue to
  // see if anything strange happens.
  task_environment_.RunUntilIdle();
}

// Verify that we're initially unset and empty.
// After initialization "complete" flags = false.
TEST_F(PrintServersProviderTest, InitialConditions) {
  TestObserver obs;
  external_servers_->AddObserver(&obs);
  ASSERT_EQ(obs.GetCalls().size(), 1u);
  EXPECT_EQ(obs.GetCalls().back().complete, false);
  EXPECT_TRUE(obs.GetCalls().back().servers.empty());
  external_servers_->RemoveObserver(&obs);
}

// Verify two ClearData() calls.
// ClearData() sets empty list and "complete" flag = true.
TEST_F(PrintServersProviderTest, ClearData2) {
  TestObserver obs;
  external_servers_->AddObserver(&obs);
  external_servers_->ClearData();
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  EXPECT_EQ(obs.GetCalls().back().complete, true);
  EXPECT_TRUE(obs.GetCalls().back().servers.empty());
  external_servers_->ClearData();
  // no changes, because observed object's state is the same
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  external_servers_->RemoveObserver(&obs);
}

// Verifies SetData().
// SetData(...) sets "complete" flag = false, then parse given data in the
// background and sets resultant list with "complete" flag = true.
TEST_F(PrintServersProviderTest, SetData) {
  TestObserver obs;
  external_servers_->AddObserver(&obs);
  external_servers_->SetData(
      std::make_unique<std::string>(kPrintServersPolicyJson1));
  // single call from AddObserver, since SetData(...) is not processed yet
  ASSERT_EQ(obs.GetCalls().size(), 1u);
  // make sure that SetData(...) is processed
  task_environment_.RunUntilIdle();
  // now the call from SetData(...) is there also
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  EXPECT_EQ(obs.GetCalls().back().complete, true);
  EXPECT_EQ(obs.GetCalls().back().servers, PrintServersPolicyData1());
  external_servers_->RemoveObserver(&obs);
}

// Verify two SetData() calls.
TEST_F(PrintServersProviderTest, SetData2) {
  TestObserver obs;
  external_servers_->AddObserver(&obs);
  external_servers_->SetData(
      std::make_unique<std::string>(kPrintServersPolicyJson1));
  // single call from AddObserver, since SetData(...) is not processed yet
  ASSERT_EQ(obs.GetCalls().size(), 1u);
  external_servers_->SetData(std::make_unique<std::string>(R"json(
[
  {
    "id": "id1",
    "display_name": "CUPS",
    "url": "ipp://192.168.1.15"
  }
])json"));
  // no changes, because nothing was processed yet
  ASSERT_EQ(obs.GetCalls().size(), 1u);
  task_environment_.RunUntilIdle();
  // both calls from SetData(...) should be reported
  ASSERT_EQ(obs.GetCalls().size(), 3u);
  EXPECT_EQ(obs.GetCalls()[1].complete, false);
  EXPECT_EQ(obs.GetCalls()[1].servers, PrintServersPolicyData1());
  EXPECT_EQ(obs.GetCalls()[2].complete, true);
  EXPECT_EQ(obs.GetCalls()[2].servers,
            std::vector<PrintServer>(
                {{"id1", GURL("http://192.168.1.15:631"), "CUPS"}}));
  external_servers_->RemoveObserver(&obs);
}

// Verifies SetData() + ClearData() before SetData() completes.
TEST_F(PrintServersProviderTest, SetDataClearData) {
  TestObserver obs;
  external_servers_->AddObserver(&obs);
  external_servers_->SetData(
      std::make_unique<std::string>(kPrintServersPolicyJson1));
  // single call from AddObserver, since SetData(...) is not processed yet
  ASSERT_EQ(obs.GetCalls().size(), 1u);
  external_servers_->ClearData();
  // a call from ClearData() was added, SetData(...) is not processed yet
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  EXPECT_EQ(obs.GetCalls().back().complete, true);
  EXPECT_TRUE(obs.GetCalls().back().servers.empty());
  // process SetData(...)
  task_environment_.RunUntilIdle();
  // no changes, effects of SetData(...) were already replaced by ClearData()
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  external_servers_->RemoveObserver(&obs);
}

// Verifies ClearData() before AddObserver() + SetData() after.
TEST_F(PrintServersProviderTest, ClearDataSetData) {
  TestObserver obs;
  external_servers_->ClearData();
  external_servers_->AddObserver(&obs);
  // single call from AddObserver, but with effects of ClearData()
  ASSERT_EQ(obs.GetCalls().size(), 1u);
  EXPECT_EQ(obs.GetCalls().back().complete, true);
  EXPECT_TRUE(obs.GetCalls().back().servers.empty());
  external_servers_->SetData(
      std::make_unique<std::string>(kPrintServersPolicyJson1));
  // SetData(...) is not completed, but generates a call switching "complete"
  // flag to false
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  EXPECT_EQ(obs.GetCalls().back().complete, false);
  EXPECT_TRUE(obs.GetCalls().back().servers.empty());
  // process SetData(...)
  task_environment_.RunUntilIdle();
  // next call with results from processed SetData(...)
  ASSERT_EQ(obs.GetCalls().size(), 3u);
  EXPECT_EQ(obs.GetCalls().back().complete, true);
  EXPECT_EQ(obs.GetCalls().back().servers, PrintServersPolicyData1());
  external_servers_->RemoveObserver(&obs);
}

// Verify that invalid URLs are filtered out.
TEST_F(PrintServersProviderTest, InvalidURLs) {
  TestObserver obs;
  external_servers_->AddObserver(&obs);
  external_servers_->SetData(std::make_unique<std::string>(R"json(
[
  {
    "id": "1",
    "display_name": "server_1",
    "url": "ipp://aaa.bbb.ccc:666/xx"
  }, {
    "id": "2",
    "display_name": "server_2",
    "url":"ipps:/print.server.intra.example.com:443z/ipp"
  }, {
    "id": "3",
    "display_name": "server_3",
    "url": "file://192.168.1.8/bleble/print"
  }, {
    "id": "4",
    "display_name": "server_4",
    "url": "http://aaa.bbb.ccc:666/xx"
  }, {
    "id": "5",
    "display_name": "server_5",
    "url": "\n \t ipps://aaa.bbb.ccc:666/yy"
  }, {
    "id": "6",
    "display_name": "server_6",
    "url":"ipps:/print.server^.intra.example.com/ipp"
  }, {
    "id": "3",
    "display_name": "server_7",
    "url": "file://194.169.2.18/bleble2/print"
  }, {
    "display_name": "server_8",
    "url": "file://195.161.3.28/bleble/print"
  }
])json"));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  EXPECT_EQ(obs.GetCalls().back().complete, true);
  // Only two records are valid:
  // server_1 - OK
  // server_2 - invalid URL - invalid port number
  // server_3 - unsupported scheme
  // server_4 - duplicate of server_1
  // server_5 - leading whitespaces, but OK
  // server_6 - invalid URL - forbidden character
  // server_7 - duplicate id
  // server_8 - missing id
  EXPECT_EQ(obs.GetCalls().back().servers,
            std::vector<PrintServer>(
                {{"1", GURL("http://aaa.bbb.ccc:666/xx"), "server_1"},
                 {"5", GURL("https://aaa.bbb.ccc:666/yy"), "server_5"}}));
  external_servers_->RemoveObserver(&obs);
}

// Verify that allowlist works as expected.
TEST_F(PrintServersProviderTest, Allowlist) {
  // The sequence from SetData test.
  TestObserver obs;
  external_servers_->AddObserver(&obs);
  external_servers_->SetData(
      std::make_unique<std::string>(kPrintServersPolicyJson1));
  // Apply an empty allowlist on the top.
  pref_service_.SetManagedPref(kAllowlistPrefName,
                               base::Value(base::Value::Type::LIST));
  // Check the resultant list - is is supposed to be empty.
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(obs.GetCalls().empty());
  EXPECT_TRUE(obs.GetCalls().back().complete);
  EXPECT_TRUE(obs.GetCalls().back().servers.empty());
  // Apply allowlist.
  base::Value::List value;
  for (std::string id : {"id3", "idX", "id1"})
    value.Append(std::move(id));
  pref_service_.SetManagedPref(kAllowlistPrefName,
                               base::Value(std::move(value)));
  // Check the resultant list.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(obs.GetCalls().back().complete);
  EXPECT_EQ(obs.GetCalls().back().servers,
            std::vector<PrintServer>({Server1(), Server3()}));
  // The end.
  external_servers_->RemoveObserver(&obs);
}

}  // namespace
}  // namespace ash

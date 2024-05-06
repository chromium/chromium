// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/print_servers_policy_provider.h"

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/print_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace {

PrintServer CreatePrintServer(std::string id,
                              std::string server_url,
                              std::string name) {
  GURL url(server_url);
  PrintServer print_server(id, url, name);
  return print_server;
}

void RecordPrintServers(std::vector<PrintServer>& result,
                        bool is_complete,
                        std::map<GURL, PrintServer> print_servers,
                        ServerPrintersFetchingMode fetching_mode) {
  result.clear();
  for (const auto& [url, print_server] : print_servers) {
    result.push_back(print_server);
  }
}

class FakePrintServersProvider : public PrintServersProvider {
 public:
  FakePrintServersProvider() = default;
  ~FakePrintServersProvider() override = default;

  void AddObserver(Observer* observer) override { observer_ = observer; }
  void RemoveObserver(Observer* observer) override { observer_ = nullptr; }
  void SetData(std::unique_ptr<std::string> data) override {}
  void SetAllowlistPref(PrefService* prefs,
                        const std::string& allowlist_pref) override {}
  void ClearData() override {}

  std::optional<std::vector<PrintServer>> GetPrintServers() override {
    return print_servers_;
  }

  base::WeakPtr<PrintServersProvider> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetPrintServers(std::optional<std::vector<PrintServer>> print_servers) {
    print_servers_ = print_servers;
    if (observer_) {
      observer_->OnServersChanged(print_servers.has_value(),
                                  print_servers.value());
    }
  }

 private:
  std::optional<std::vector<PrintServer>> print_servers_;
  raw_ptr<PrintServersProvider::Observer> observer_ = nullptr;
  base::WeakPtrFactory<FakePrintServersProvider> weak_ptr_factory_{this};
};

TEST(PrintServersPolicyProvider, UserAndDevicePrintServersAreProvided) {
  std::vector<PrintServer> user_print_servers;
  user_print_servers.push_back(
      CreatePrintServer("1", "ipp://localhost:631", "CUPS"));
  user_print_servers.push_back(
      CreatePrintServer("2", "ipp://localhost:632", "CUPS"));

  std::vector<PrintServer> device_print_servers;
  device_print_servers.push_back(
      CreatePrintServer("2", "ipp://localhost:632", "CUPS"));
  device_print_servers.push_back(
      CreatePrintServer("3", "ipp://localhost:633", "CUPS"));

  FakePrintServersProvider user_policy_provider;
  FakePrintServersProvider device_policy_provider;
  auto print_servers_policy_provider =
      PrintServersPolicyProvider::CreateForTesting(
          user_policy_provider.AsWeakPtr(), device_policy_provider.AsWeakPtr());

  std::vector<PrintServer> result;
  print_servers_policy_provider->SetListener(
      base::BindRepeating(&RecordPrintServers, std::ref(result)));
  user_policy_provider.SetPrintServers(user_print_servers);
  device_policy_provider.SetPrintServers(device_print_servers);

  EXPECT_THAT(result,
              testing::UnorderedElementsAre(
                  CreatePrintServer("1", "ipp://localhost:631", "CUPS"),
                  CreatePrintServer("2", "ipp://localhost:632", "CUPS"),
                  CreatePrintServer("3", "ipp://localhost:633", "CUPS")));
}

// This is a regression test for b/287922784.
TEST(PrintServersPolicyProvider, ListenerIsSetAfterPrintServersAreReady) {
  FakePrintServersProvider user_policy_provider;
  FakePrintServersProvider device_policy_provider;

  std::vector<PrintServer> user_print_servers;
  auto user_print_server =
      CreatePrintServer("1", "ipp://localhost:631", "CUPS");
  user_print_servers.push_back(user_print_server);
  user_policy_provider.SetPrintServers(user_print_servers);

  auto print_servers_policy_provider =
      PrintServersPolicyProvider::CreateForTesting(
          user_policy_provider.AsWeakPtr(), device_policy_provider.AsWeakPtr());

  std::vector<PrintServer> result;
  print_servers_policy_provider->SetListener(
      base::BindRepeating(&RecordPrintServers, std::ref(result)));

  EXPECT_THAT(result, testing::UnorderedElementsAre(CreatePrintServer(
                          "1", "ipp://localhost:631", "CUPS")));
}

}  // namespace
}  // namespace ash

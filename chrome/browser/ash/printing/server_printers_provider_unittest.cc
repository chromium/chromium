// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/server_printers_provider.h"

#include <map>
#include <memory>
#include <string>

#include "chrome/browser/ash/printing/enterprise/print_servers_provider_factory.h"
#include "chrome/browser/ash/printing/print_server.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libipp/libipp/builder.h"
#include "third_party/libipp/libipp/frame.h"

namespace ash {

namespace {

using ::chromeos::Printer;
using ::chromeos::Uri;
using ::testing::AllOf;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;

class TestingProfileWithURLLoaderFactory : public TestingProfile {
 public:
  explicit TestingProfileWithURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : url_loader_factory_(url_loader_factory) {}
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return url_loader_factory_;
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

PrintServer PrintServer1() {
  GURL url("http://192.168.1.5/printer");
  PrintServer print_server("id1", url, "LexaPrint");
  return print_server;
}

PrintServer PrintServer2() {
  GURL url("https://print-server.intranet.example.com:443/ipp/cl2k4");
  PrintServer print_server("id2", url, "Color Laser");
  return print_server;
}

Printer Printer1() {
  Printer printer("server-20e91b728d4d04bc68132ced81772ef5");
  printer.set_display_name("LexaPrint - Name");
  std::string server("ipp://192.168.1.5:80");
  printer.set_print_server_uri(server);
  Uri url("ipp://192.168.1.5:80/printers/LexaPrint - Name");
  printer.SetUri(url);
  return printer;
}

Printer Printer2() {
  Printer printer("server-5da95e01216b1fe0ee1de25dc8d0a6e8");
  printer.set_display_name("Color Laser - Name");
  std::string server("ipps://print-server.intranet.example.com:443");
  printer.set_print_server_uri(server);
  Uri url(
      "ipps://print-server.intranet.example.com/printers/Color Laser - Name");
  printer.SetUri(url);
  return printer;
}

}  // namespace

auto GetPrinter = [](const PrinterDetector::DetectedPrinter& input) -> Printer {
  return input.printer;
};

auto PrinterMatcher(Printer printer) {
  return ResultOf(
      GetPrinter,
      AllOf(Property(&Printer::uri, printer.uri()),
            Property(&Printer::print_server_uri, printer.print_server_uri()),
            Property(&Printer::display_name, printer.display_name())));
}

class ServerPrintersProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_profile_ = std::make_unique<TestingProfileWithURLLoaderFactory>(
        test_url_loader_factory_.GetSafeWeakWrapper());
    ASSERT_TRUE(test_server_.Start());
    server_printers_provider_ =
        ServerPrintersProvider::Create(test_profile_.get());
  }

  void TearDown() override { PrintServersProviderFactory::Get()->Shutdown(); }

  std::string CreateResponse(const std::string& name,
                             const std::string& description) {
    ipp::Frame response(ipp::Operation::CUPS_Get_Printers);
    ipp::CollsView::iterator grp;
    response.AddGroup(ipp::GroupTag::printer_attributes, grp);
    grp->AddAttr("printer-name", ipp::ValueTag::nameWithLanguage,
                 ipp::StringWithLanguage(name, "us-EN"));
    grp->AddAttr("printer-info", ipp::ValueTag::textWithLanguage,
                 ipp::StringWithLanguage(description, "us-EN"));
    std::vector<uint8_t> bin_data = ipp::BuildBinaryFrame(response);
    std::string response_body(bin_data.begin(), bin_data.end());
    return response_body;
  }

  void OnServersChanged(bool is_complete,
                        std::vector<PrintServer> print_servers) {
    std::map<GURL, PrintServer> new_print_servers;
    for (auto& print_server : print_servers) {
      new_print_servers.emplace(print_server.GetUrl(), print_server);
    }
    server_printers_provider_->OnServersChanged(is_complete, new_print_servers);
  }

  // Everything must be called on Chrome_UIThread.
  content::BrowserTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  std::unique_ptr<TestingProfileWithURLLoaderFactory> test_profile_;

  net::test_server::EmbeddedTestServer test_server_;

  std::unique_ptr<ServerPrintersProvider> server_printers_provider_;
};

TEST_F(ServerPrintersProviderTest, GetPrinters) {
  test_url_loader_factory_.AddResponse(
      "http://192.168.1.5/printer",
      CreateResponse("LexaPrint - Name", "LexaPrint Description"));
  test_url_loader_factory_.AddResponse(
      "https://print-server.intranet.example.com:443/ipp/cl2k4",
      CreateResponse("Color Laser - Name", "Color Laser Description"));

  EXPECT_TRUE(server_printers_provider_->GetPrinters().empty());

  std::vector<PrintServer> print_servers;
  print_servers.push_back(PrintServer1());
  print_servers.push_back(PrintServer2());
  OnServersChanged(true, print_servers);
  task_environment_.RunUntilIdle();

  EXPECT_THAT(server_printers_provider_->GetPrinters(),
              UnorderedElementsAre(PrinterMatcher(Printer1()),
                                   PrinterMatcher(Printer2())));
}

}  // namespace ash

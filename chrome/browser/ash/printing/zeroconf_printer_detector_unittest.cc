// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/zeroconf_printer_detector.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "chrome/browser/local_discovery/fake_service_discovery_device_lister.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::local_discovery::FakeServiceDiscoveryDeviceLister;
using ::local_discovery::ServiceDescription;
using ::local_discovery::ServiceDiscoveryDeviceLister;

// Determine basic printer attributes deterministically but pseudorandomly based
// on the printer name.  The exact values returned here are not really
// important, the important parts are that there's variety based on the name,
// and it's deterministic.

// Should this printer provide usb_MFG and usb_MDL fields?
bool GetUsbFor(const std::string& name) {
  return std::hash<std::string>()(name) & 1;
}

// Get an IP address for this printer.  The returned address may be IPv4 or IPv6
net::IPAddress GetIPAddressFor(const std::string& name) {
  std::mt19937 rng(std::hash<std::string>()(name));
  if (rng() & 1) {
    // Give an IPv4 address.
    return net::IPAddress(rng(), rng(), rng(), rng());
  } else {
    // Give an IPv6 address.
    return net::IPAddress(rng(), rng(), rng(), rng(), rng(), rng(), rng(),
                          rng(), rng(), rng(), rng(), rng(), rng(), rng(),
                          rng(), rng());
  }
}

int GetPortFor(const std::string& name) {
  return (std::hash<std::string>()(name) % 1000) + 1;
}

// Enums for MakeExpectedPrinter().
enum class ServiceType {
  kIpp,     // IPP
  kIpps,    // IPPS
  kIppE,    // IPP-Everywhere
  kIppsE,   // IPPS-Everywhere
  kSocket,  // Socket
  kLpd,     // LPD
};

// This corresponds to MakeServiceDescription() below. Given the same name (and
// the correct service type), this generates the DetectedPrinter record we
// expect from ZeroconfPrinterDetector when it gets that ServiceDescription.
// This needs to be kept in sync with MakeServiceDescription().
PrinterDetector::DetectedPrinter MakeExpectedPrinter(const std::string& name,
                                                     ServiceType service_type) {
  PrinterDetector::DetectedPrinter detected;
  chromeos::Printer& printer = detected.printer;
  net::IPAddress ip_address = GetIPAddressFor(name);
  int port = GetPortFor(name);
  std::string scheme;
  std::string rp = base::StrCat({name, "_rp"});
  switch (service_type) {
    case ServiceType::kIpp:
      scheme = "ipp";
      break;
    case ServiceType::kIpps:
      scheme = "ipps";
      break;
    case ServiceType::kIppE:
      scheme = "ipp";
      printer.mutable_ppd_reference()->autoconf = true;
      break;
    case ServiceType::kIppsE:
      scheme = "ipps";
      printer.mutable_ppd_reference()->autoconf = true;
      break;
    case ServiceType::kSocket:
      scheme = "socket";
      rp = "";
      break;
    case ServiceType::kLpd:
      scheme = "lpd";
      break;
  }
  printer.SetUri(base::StringPrintf("%s://%s.local:%d/%s", scheme.c_str(),
                                    name.c_str(), port, rp.c_str()));

  printer.set_uuid(base::StrCat({name, "_UUID"}));
  printer.set_display_name(name);
  printer.set_description(base::StrCat({name, "_note"}));
  printer.set_make_and_model(base::StrCat({name, "_ty"}));
  detected.ppd_search_data.make_and_model.push_back(printer.make_and_model());
  detected.ppd_search_data.make_and_model.push_back(
      base::StrCat({name, "_product"}));
  if (GetUsbFor(name)) {
    // We should get an effective make and model guess from the usb fields
    // if they exist.
    detected.ppd_search_data.make_and_model.push_back(
        base::StrCat({name, "_usb_MFG ", name, "_usb_MDL"}));
  }

  return detected;
}

// Creates a deterministic ServiceDescription based on the service name and
// type. See the note on MakeExpectedPrinter() above. This must be kept in sync
// with MakeExpectedPrinter().
ServiceDescription MakeServiceDescription(const std::string& name,
                                          const std::string& service_type) {
  ServiceDescription sd;
  sd.service_name = base::StrCat({name, ".", service_type});
  sd.metadata.push_back(base::StrCat({"ty=", name, "_ty"}));
  sd.metadata.push_back(base::StrCat({"product=(", name, "_product)"}));
  if (GetUsbFor(name)) {
    sd.metadata.push_back(base::StrCat({"usb_MFG=", name, "_usb_MFG"}));
    sd.metadata.push_back(base::StrCat({"usb_MDL=", name, "_usb_MDL"}));
  }
  sd.metadata.push_back(base::StrCat({"rp=", name, "_rp"}));
  sd.metadata.push_back(base::StrCat({"note=", name, "_note"}));
  sd.metadata.push_back(base::StrCat({"UUID=", name, "_UUID"}));
  sd.address.set_host(base::StrCat({name, ".local"}));
  sd.ip_address = GetIPAddressFor(name);
  sd.address.set_port(GetPortFor(name));
  return sd;
}

class ZeroconfPrinterDetectorTest : public testing::Test {
 public:
  ZeroconfPrinterDetectorTest() {
    auto* runner = task_environment_.GetMainThreadTaskRunner().get();
    auto ipp_lister = std::make_unique<FakeServiceDiscoveryDeviceLister>(
        runner, ZeroconfPrinterDetector::kIppServiceName);
    ipp_lister_ = ipp_lister.get();
    auto ipps_lister = std::make_unique<FakeServiceDiscoveryDeviceLister>(
        runner, ZeroconfPrinterDetector::kIppsServiceName);
    ipps_lister_ = ipps_lister.get();
    auto ippe_lister = std::make_unique<FakeServiceDiscoveryDeviceLister>(
        runner, ZeroconfPrinterDetector::kIppEverywhereServiceName);
    ippe_lister_ = ippe_lister.get();
    auto ippse_lister = std::make_unique<FakeServiceDiscoveryDeviceLister>(
        runner, ZeroconfPrinterDetector::kIppsEverywhereServiceName);
    ippse_lister_ = ippse_lister.get();
    auto socket_lister = std::make_unique<FakeServiceDiscoveryDeviceLister>(
        runner, ZeroconfPrinterDetector::kSocketServiceName);
    socket_lister_ = socket_lister.get();
    auto lpd_lister = std::make_unique<FakeServiceDiscoveryDeviceLister>(
        runner, ZeroconfPrinterDetector::kLpdServiceName);
    lpd_lister_ = lpd_lister.get();

    listers_[ZeroconfPrinterDetector::kIppServiceName] = std::move(ipp_lister);
    listers_[ZeroconfPrinterDetector::kIppsServiceName] =
        std::move(ipps_lister);
    listers_[ZeroconfPrinterDetector::kIppEverywhereServiceName] =
        std::move(ippe_lister);
    listers_[ZeroconfPrinterDetector::kIppsEverywhereServiceName] =
        std::move(ippse_lister);
    listers_[ZeroconfPrinterDetector::kSocketServiceName] =
        std::move(socket_lister);
    listers_[ZeroconfPrinterDetector::kLpdServiceName] = std::move(lpd_lister);
  }
  ~ZeroconfPrinterDetectorTest() override = default;

  void CreateDetectorWithIppRejectList(
      base::flat_set<std::string> ipp_reject_list) {
    detector_ = ZeroconfPrinterDetector::CreateForTesting(
        &listers_, std::move(ipp_reject_list));
    // The previously allocated listers_ are swapped into the detector_, and so
    // the unique_ptr values of the listers_ map are no longer valid at this
    // point.  The ipp[se]_lister_ raw pointers are kept as seperate members to
    // keep the lister fakes accessible after ownership is transferred into the
    // detector.
    listers_.clear();
    detector_->RegisterPrintersFoundCallback(base::BindRepeating(
        &ZeroconfPrinterDetectorTest::OnPrintersFound, base::Unretained(this)));
    ipp_lister_->SetDelegate(detector_.get());
    ipps_lister_->SetDelegate(detector_.get());
    ippe_lister_->SetDelegate(detector_.get());
    ippse_lister_->SetDelegate(detector_.get());
    socket_lister_->SetDelegate(detector_.get());
    lpd_lister_->SetDelegate(detector_.get());
  }

  void CreateDetector() { CreateDetectorWithIppRejectList({}); }

  // Expect that the most up-to-date results from the detector match those
  // in printers.
  void ExpectPrintersAre(
      const std::vector<PrinterDetector::DetectedPrinter>& printers) {
    // The last observer callback should tell us the same thing as the querying
    // the detector manually.
    ASSERT_FALSE(printers_found_callbacks_.empty());
    ExpectPrintersEq(printers, printers_found_callbacks_.back());
    ExpectPrintersEq(printers, detector_->GetPrinters());
  }

  // Expect that the detected printers list is empty.
  void ExpectPrintersEmpty() {
    // Assert that the most recent callbacks are empty.
    ASSERT_FALSE(printers_found_callbacks_.empty());
    ASSERT_TRUE(printers_found_callbacks_.back().empty());
    ASSERT_TRUE(detector_->GetPrinters().empty());
  }

  // Expect that the given vectors have the same contents.  The ordering
  // may be different.
  void ExpectPrintersEq(
      const std::vector<PrinterDetector::DetectedPrinter>& expected,
      const std::vector<PrinterDetector::DetectedPrinter>& actual) {
    if (expected.size() != actual.size()) {
      ADD_FAILURE() << "Printers size mismatch, found " << actual.size()
                    << " expected " << expected.size();
      return;
    }
    std::vector<PrinterDetector::DetectedPrinter> sorted_expected = expected;
    std::vector<PrinterDetector::DetectedPrinter> sorted_actual = actual;

    std::sort(sorted_expected.begin(), sorted_expected.end(),
              [](const PrinterDetector::DetectedPrinter& a,
                 const PrinterDetector::DetectedPrinter& b) -> bool {
                return a.printer.uuid() < b.printer.uuid();
              });
    std::sort(sorted_actual.begin(), sorted_actual.end(),
              [](const PrinterDetector::DetectedPrinter& a,
                 const PrinterDetector::DetectedPrinter& b) -> bool {
                return a.printer.uuid() < b.printer.uuid();
              });
    for (size_t i = 0; i < sorted_expected.size(); ++i) {
      ExpectPrinterEq(sorted_expected[i], sorted_actual[i]);
    }
  }

  void ExpectPrinterEq(const PrinterDetector::DetectedPrinter& expected,
                       const PrinterDetector::DetectedPrinter& actual) {
    EXPECT_EQ(expected.printer.uri(), actual.printer.uri());
    // We don't have a good way to directly check for an expected id.
    EXPECT_EQ(expected.printer.uuid(), actual.printer.uuid());
    EXPECT_EQ(expected.printer.display_name(), actual.printer.display_name());
    EXPECT_EQ(expected.printer.description(), actual.printer.description());
    EXPECT_EQ(expected.printer.IsIppEverywhere(),
              actual.printer.IsIppEverywhere());
    EXPECT_EQ(expected.printer.make_and_model(),
              actual.printer.make_and_model());
    EXPECT_EQ(expected.ppd_search_data.usb_vendor_id,
              actual.ppd_search_data.usb_vendor_id);
    EXPECT_EQ(expected.ppd_search_data.usb_product_id,
              actual.ppd_search_data.usb_product_id);
    EXPECT_EQ(expected.ppd_search_data.make_and_model,
              actual.ppd_search_data.make_and_model);
  }

  // PrinterDetector callback.
  void OnPrintersFound(
      const std::vector<PrinterDetector::DetectedPrinter>& printers) {
    printers_found_callbacks_.push_back(printers);
  }

 protected:
  // Runs pending tasks regardless of delay.
  void CompleteTasks() { task_environment_.FastForwardUntilNoTasksRemain(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Device listers fakes.  These are initialized when the test is constructed.
  // These pointers don't involve ownership; ownership of the listers starts
  // with this class in listers_ when the test starts, and is transferred to
  // detector_ when the detector is created.  Throughout, the listers remain
  // available to the test via these pointers.
  raw_ptr<FakeServiceDiscoveryDeviceLister, DanglingUntriaged> ipp_lister_;
  raw_ptr<FakeServiceDiscoveryDeviceLister, DanglingUntriaged> ipps_lister_;
  raw_ptr<FakeServiceDiscoveryDeviceLister, DanglingUntriaged> ippe_lister_;
  raw_ptr<FakeServiceDiscoveryDeviceLister, DanglingUntriaged> ippse_lister_;
  raw_ptr<FakeServiceDiscoveryDeviceLister, DanglingUntriaged> socket_lister_;
  raw_ptr<FakeServiceDiscoveryDeviceLister, DanglingUntriaged> lpd_lister_;

  // Detector under test.
  std::unique_ptr<ZeroconfPrinterDetector> detector_;

  // Saved copies of all the things given to OnPrintersFound.
  std::vector<std::vector<PrinterDetector::DetectedPrinter>>
      printers_found_callbacks_;

 private:
  // Temporary storage for the device listers, between the time the test is
  // constructed and the detector is created.  Tests shouldn't access this
  // directly, use the ipp*_lister_ variables instead.
  std::map<std::string, std::unique_ptr<ServiceDiscoveryDeviceLister>> listers_;
};

// Very basic stuff, one printer of each protocol we support.
TEST_F(ZeroconfPrinterDetectorTest, SingleIppPrinter) {
  ipp_lister_->Announce(MakeServiceDescription(
      "Printer1", ZeroconfPrinterDetector::kIppServiceName));
  CreateDetector();
  CompleteTasks();
  ExpectPrintersAre({MakeExpectedPrinter("Printer1", ServiceType::kIpp)});
}

TEST_F(ZeroconfPrinterDetectorTest, SingleIppsPrinter) {
  ipps_lister_->Announce(MakeServiceDescription(
      "Printer2", ZeroconfPrinterDetector::kIppsServiceName));
  CreateDetector();
  CompleteTasks();
  ExpectPrintersAre({MakeExpectedPrinter("Printer2", ServiceType::kIpps)});
}

TEST_F(ZeroconfPrinterDetectorTest, SingleIppEverywherePrinter) {
  ippe_lister_->Announce(MakeServiceDescription(
      "Printer3", ZeroconfPrinterDetector::kIppEverywhereServiceName));
  CreateDetector();
  CompleteTasks();
  ExpectPrintersAre({MakeExpectedPrinter("Printer3", ServiceType::kIppE)});
}

TEST_F(ZeroconfPrinterDetectorTest, SingleIppsEverywherePrinter) {
  ippse_lister_->Announce(MakeServiceDescription(
      "Printer4", ZeroconfPrinterDetector::kIppsEverywhereServiceName));
  CreateDetector();
  CompleteTasks();
  ExpectPrintersAre({MakeExpectedPrinter("Printer4", ServiceType::kIppsE)});
}

TEST_F(ZeroconfPrinterDetectorTest, SingleSocketPrinter) {
  socket_lister_->Announce(MakeServiceDescription(
      "Printer5", ZeroconfPrinterDetector::kSocketServiceName));
  CreateDetector();
  CompleteTasks();
  ExpectPrintersAre({MakeExpectedPrinter("Printer5", ServiceType::kSocket)});
}

TEST_F(ZeroconfPrinterDetectorTest, SingleLpdPrinter) {
  lpd_lister_->Announce(MakeServiceDescription(
      "Printer6", ZeroconfPrinterDetector::kLpdServiceName));
  CreateDetector();
  CompleteTasks();
  ExpectPrintersAre({MakeExpectedPrinter("Printer6", ServiceType::kLpd)});
}

// Test that an announce after the detector creation shows up as a printer.
TEST_F(ZeroconfPrinterDetectorTest, AnnounceAfterDetectorCreation) {
  CreateDetector();
  CompleteTasks();
  ippse_lister_->Announce(MakeServiceDescription(
      "Printer4", ZeroconfPrinterDetector::kIppsEverywhereServiceName));
  CompleteTasks();
  ExpectPrintersAre({MakeExpectedPrinter("Printer4", ServiceType::kIppsE)});
}

// Test that we use the same printer ID regardless of which service type it
// comes to us from.
TEST_F(ZeroconfPrinterDetectorTest, StableIds) {
  ipp_lister_->Announce(MakeServiceDescription(
      "Printer1", ZeroconfPrinterDetector::kIppServiceName));
  CreateDetector();
  CompleteTasks();
  ASSERT_FALSE(printers_found_callbacks_.empty());
  ASSERT_EQ(1U, printers_found_callbacks_.back().size());
  // Grab the id when it's an IPPS printer We should continue to get the same id
  // regardless of service type.
  std::string id = printers_found_callbacks_.back()[0].printer.id();

  // Remove it as an IPP printer, add it as an IPPS printer.
  ipp_lister_->Remove("Printer1");
  CompleteTasks();
  ASSERT_TRUE(printers_found_callbacks_.back().empty());
  ipps_lister_->Announce(MakeServiceDescription(
      "Printer1", ZeroconfPrinterDetector::kIppsServiceName));
  CompleteTasks();
  ASSERT_EQ(1U, printers_found_callbacks_.back().size());
  // Id should be the same.
  ASSERT_EQ(id, printers_found_callbacks_.back()[0].printer.id());

  // Remove it as an IPPS printer, add it as an IPP-Everywhere printer.
  ipps_lister_->Remove("Printer1");
  CompleteTasks();
  ASSERT_TRUE(printers_found_callbacks_.back().empty());
  ippe_lister_->Announce(MakeServiceDescription(
      "Printer1", ZeroconfPrinterDetector::kIppEverywhereServiceName));
  CompleteTasks();
  ASSERT_EQ(1U, printers_found_callbacks_.back().size());
  // Id should be the same.
  ASSERT_EQ(id, printers_found_callbacks_.back()[0].printer.id());

  // Remove it as an IPP-Everywhere printer, add it as an IPPS-Everywhere
  // printer.
  ippe_lister_->Remove("Printer1");
  CompleteTasks();
  ASSERT_TRUE(printers_found_callbacks_.back().empty());
  ippse_lister_->Announce(MakeServiceDescription(
      "Printer1", ZeroconfPrinterDetector::kIppsEverywhereServiceName));
  CompleteTasks();
  ASSERT_EQ(1U, printers_found_callbacks_.back().size());
  // Id should be the same.
  ASSERT_EQ(id, printers_found_callbacks_.back()[0].printer.id());

  // Remove it as an IPPS-Everywhere printer, add it as a socket printer.
  ippse_lister_->Remove("Printer1");
  CompleteTasks();
  ASSERT_TRUE(printers_found_callbacks_.back().empty());
  socket_lister_->Announce(MakeServiceDescription(
      "Printer1", ZeroconfPrinterDetector::kSocketServiceName));
  CompleteTasks();
  ASSERT_EQ(1U, printers_found_callbacks_.back().size());
  // Id should be the same.
  ASSERT_EQ(id, printers_found_callbacks_.back()[0].printer.id());

  // Remove it as a socket printer, add it as an LPD printer.
  socket_lister_->Remove("Printer1");
  CompleteTasks();
  ASSERT_TRUE(printers_found_callbacks_.back().empty());
  lpd_lister_->Announce(MakeServiceDescription(
      "Printer1", ZeroconfPrinterDetector::kLpdServiceName));
  CompleteTasks();
  ASSERT_EQ(1U, printers_found_callbacks_.back().size());
  // Id should be the same.
  ASSERT_EQ(id, printers_found_callbacks_.back()[0].printer.id());
}

// Test a basic removal.
TEST_F(ZeroconfPrinterDetectorTest, Removal) {
  ipp_lister_->Announce(MakeServiceDescription(
      "Printer5", ZeroconfPrinterDetector::kIppServiceName));
  ipp_lister_->Announce(MakeServiceDescription(
      "Printer6", ZeroconfPrinterDetector::kIppServiceName));
  ipp_lister_->Announce(MakeServiceDescription(
      "Printer7", ZeroconfPrinterDetector::kIppServiceName));
  ipp_lister_->Announce(MakeServiceDescription(
      "Printer8", ZeroconfPrinterDetector::kIppServiceName));
  ipp_lister_->Announce(MakeServiceDescription(
      "Printer9", ZeroconfPrinterDetector::kIppServiceName));
  CreateDetector();
  CompleteTasks();
  ExpectPrintersAre({MakeExpectedPrinter("Printer5", ServiceType::kIpp),
                     MakeExpectedPrinter("Printer6", ServiceType::kIpp),
                     MakeExpectedPrinter("Printer7", ServiceType::kIpp),
                     MakeExpectedPrinter("Printer8", ServiceType::kIpp),
                     MakeExpectedPrinter("Printer9", ServiceType::kIpp)});
  ipp_lister_->Remove("Printer7");
  CompleteTasks();
  ExpectPrintersAre({MakeExpectedPrinter("Printer5", ServiceType::kIpp),
                     MakeExpectedPrinter("Printer6", ServiceType::kIpp),
                     MakeExpectedPrinter("Printer8", ServiceType::kIpp),
                     MakeExpectedPrinter("Printer9", ServiceType::kIpp)});
}

// Test that, when the same printer appears in multiple services, we
// use the highest priority one.  Priorities, from highest to lowest
// are IPPS-E, IPP-E, IPPS, IPP, Socket, LPD.
TEST_F(ZeroconfPrinterDetectorTest, ServiceTypePriorities) {
  // Advertise on all services.
  ipp_lister_->Announce(MakeServiceDescription(
      "Printer5", ZeroconfPrinterDetector::kIppServiceName));
  ipps_lister_->Announce(MakeServiceDescription(
      "Printer5", ZeroconfPrinterDetector::kIppsServiceName));
  ippe_lister_->Announce(MakeServiceDescription(
      "Printer5", ZeroconfPrinterDetector::kIppEverywhereServiceName));
  ippse_lister_->Announce(MakeServiceDescription(
      "Printer5", ZeroconfPrinterDetector::kIppsEverywhereServiceName));
  socket_lister_->Announce(MakeServiceDescription(
      "Printer5", ZeroconfPrinterDetector::kSocketServiceName));
  lpd_lister_->Announce(MakeServiceDescription(
      "Printer5", ZeroconfPrinterDetector::kLpdServiceName));
  CreateDetector();
  CompleteTasks();
  // IPPS-E is highest priority.
  ExpectPrintersAre({MakeExpectedPrinter("Printer5", ServiceType::kIppsE)});
  ippse_lister_->Remove("Printer5");
  CompleteTasks();
  // IPP-E is highest remaining priority.
  ExpectPrintersAre({MakeExpectedPrinter("Printer5", ServiceType::kIppE)});

  ippe_lister_->Remove("Printer5");
  CompleteTasks();
  // IPPS is highest remaining priority.
  ExpectPrintersAre({MakeExpectedPrinter("Printer5", ServiceType::kIpps)});

  ipps_lister_->Remove("Printer5");
  CompleteTasks();
  // IPP is highest remaining priority.
  ExpectPrintersAre({MakeExpectedPrinter("Printer5", ServiceType::kIpp)});

  ipp_lister_->Remove("Printer5");
  CompleteTasks();
  // Socket is highest remaining entry.
  ExpectPrintersAre({MakeExpectedPrinter("Printer5", ServiceType::kSocket)});

  socket_lister_->Remove("Printer5");
  CompleteTasks();
  // LPD is only remaining entry.
  ExpectPrintersAre({MakeExpectedPrinter("Printer5", ServiceType::kLpd)});

  lpd_lister_->Remove("Printer5");
  CompleteTasks();
  // No entries left.
  ExpectPrintersEmpty();
}

// Test a printer that is known not to work with IPP/IPPS and make sure a
// different protocol is chosen.
TEST_F(ZeroconfPrinterDetectorTest, RejectIpp) {
  std::string bad_ipp_printer = "manufacturer awesome printer-name";
  base::flat_set<std::string> reject_list;
  // We have to add the _ty suffix to match how MakeServiceDescription and
  // MakeExpectedPrinter work.
  reject_list.insert(bad_ipp_printer + "_ty");
  // Advertise on IPP and LPD services.
  ipp_lister_->Announce(MakeServiceDescription(
      bad_ipp_printer, ZeroconfPrinterDetector::kIppServiceName));
  ipps_lister_->Announce(MakeServiceDescription(
      bad_ipp_printer, ZeroconfPrinterDetector::kIppsServiceName));
  lpd_lister_->Announce(MakeServiceDescription(
      bad_ipp_printer, ZeroconfPrinterDetector::kLpdServiceName));
  CreateDetectorWithIppRejectList(reject_list);
  CompleteTasks();

  // Should be rejected for IPPS-E and IPP-E, so it should only exist in LPD.
  ExpectPrintersAre({MakeExpectedPrinter(bad_ipp_printer, ServiceType::kLpd)});

  lpd_lister_->Remove(bad_ipp_printer);
  CompleteTasks();
  // No entries left.
  ExpectPrintersEmpty();
}

// Test that cache flushes appropriately remove entries.
TEST_F(ZeroconfPrinterDetectorTest, CacheFlushes) {
  ipp_lister_->Announce(MakeServiceDescription(
      "Printer6", ZeroconfPrinterDetector::kIppServiceName));
  ipp_lister_->Announce(MakeServiceDescription(
      "Printer7", ZeroconfPrinterDetector::kIppServiceName));
  ipps_lister_->Announce(MakeServiceDescription(
      "Printer7", ZeroconfPrinterDetector::kIppsServiceName));
  ipps_lister_->Announce(MakeServiceDescription(
      "Printer8", ZeroconfPrinterDetector::kIppsServiceName));
  ippe_lister_->Announce(MakeServiceDescription(
      "Printer8", ZeroconfPrinterDetector::kIppEverywhereServiceName));
  ippe_lister_->Announce(MakeServiceDescription(
      "Printer9", ZeroconfPrinterDetector::kIppEverywhereServiceName));
  ippse_lister_->Announce(MakeServiceDescription(
      "Printer9", ZeroconfPrinterDetector::kIppsEverywhereServiceName));
  ippse_lister_->Announce(MakeServiceDescription(
      "Printer10", ZeroconfPrinterDetector::kIppsEverywhereServiceName));
  socket_lister_->Announce(MakeServiceDescription(
      "Printer10", ZeroconfPrinterDetector::kSocketServiceName));
  socket_lister_->Announce(MakeServiceDescription(
      "Printer11", ZeroconfPrinterDetector::kSocketServiceName));
  lpd_lister_->Announce(MakeServiceDescription(
      "Printer11", ZeroconfPrinterDetector::kLpdServiceName));
  lpd_lister_->Announce(MakeServiceDescription(
      "Printer12", ZeroconfPrinterDetector::kLpdServiceName));

  CreateDetector();
  CompleteTasks();
  ExpectPrintersAre({MakeExpectedPrinter("Printer6", ServiceType::kIpp),
                     MakeExpectedPrinter("Printer7", ServiceType::kIpps),
                     MakeExpectedPrinter("Printer8", ServiceType::kIppE),
                     MakeExpectedPrinter("Printer9", ServiceType::kIppsE),
                     MakeExpectedPrinter("Printer10", ServiceType::kIppsE),
                     MakeExpectedPrinter("Printer11", ServiceType::kSocket),
                     MakeExpectedPrinter("Printer12", ServiceType::kLpd)});

  ipps_lister_->Clear();

  CompleteTasks();
  // With the IPPS lister cleared, all printers should be cleared.
  ExpectPrintersEmpty();

  // We should have restarted discovery after dealing with the cache flush.
  EXPECT_TRUE(ipps_lister_->discovery_started());

  // Just for kicks, announce something new at this point.
  ipps_lister_->Announce(MakeServiceDescription(
      "Printer13", ZeroconfPrinterDetector::kIppsServiceName));
  CompleteTasks();
  ExpectPrintersAre({MakeExpectedPrinter("Printer13", ServiceType::kIpps)});

  // Clear out the IPPS lister, which will clear all printers too.
  ipps_lister_->Clear();
  CompleteTasks();

  // With the IPPS lister cleared, Printer13 should disappear.
  ExpectPrintersEmpty();
  EXPECT_TRUE(ippe_lister_->discovery_started());
}

// Test some general traffic with a mix of everything we expect to handle.
TEST_F(ZeroconfPrinterDetectorTest, GeneralMixedTraffic) {
  ipp_lister_->Announce(MakeServiceDescription(
      "Printer12", ZeroconfPrinterDetector::kIppServiceName));
  ipps_lister_->Announce(MakeServiceDescription(
      "Printer12", ZeroconfPrinterDetector::kIppsServiceName));
  ipps_lister_->Announce(MakeServiceDescription(
      "Printer13", ZeroconfPrinterDetector::kIppsServiceName));
  ippse_lister_->Announce(MakeServiceDescription(
      "Printer14", ZeroconfPrinterDetector::kIppsEverywhereServiceName));
  ipps_lister_->Announce(MakeServiceDescription(
      "Printer15", ZeroconfPrinterDetector::kIppsServiceName));
  socket_lister_->Announce(MakeServiceDescription(
      "Printer16", ZeroconfPrinterDetector::kSocketServiceName));
  lpd_lister_->Announce(MakeServiceDescription(
      "Printer17", ZeroconfPrinterDetector::kLpdServiceName));

  CreateDetector();
  CompleteTasks();
  ExpectPrintersAre({MakeExpectedPrinter("Printer12", ServiceType::kIpps),
                     MakeExpectedPrinter("Printer13", ServiceType::kIpps),
                     MakeExpectedPrinter("Printer14", ServiceType::kIppsE),
                     MakeExpectedPrinter("Printer15", ServiceType::kIpps),
                     MakeExpectedPrinter("Printer16", ServiceType::kSocket),
                     MakeExpectedPrinter("Printer17", ServiceType::kLpd)});

  ippe_lister_->Announce(MakeServiceDescription(
      "Printer13", ZeroconfPrinterDetector::kIppEverywhereServiceName));
  ipp_lister_->Announce(MakeServiceDescription(
      "Printer18", ZeroconfPrinterDetector::kIppServiceName));
  CompleteTasks();
  ExpectPrintersAre({MakeExpectedPrinter("Printer12", ServiceType::kIpps),
                     MakeExpectedPrinter("Printer13", ServiceType::kIppE),
                     MakeExpectedPrinter("Printer14", ServiceType::kIppsE),
                     MakeExpectedPrinter("Printer15", ServiceType::kIpps),
                     MakeExpectedPrinter("Printer16", ServiceType::kSocket),
                     MakeExpectedPrinter("Printer17", ServiceType::kLpd),
                     MakeExpectedPrinter("Printer18", ServiceType::kIpp)});

  ipp_lister_->Remove("NonexistantPrinter");
  ipps_lister_->Remove("Printer12");
  ipps_lister_->Clear();
  CompleteTasks();
  ExpectPrintersEmpty();
}

// Verify tasks are cleaned up properly when class is destroyed.
TEST_F(ZeroconfPrinterDetectorTest, DestroyedWithTasksPending) {
  CreateDetector();
  // Cause a callback to be queued.
  ipp_lister_->Announce(MakeServiceDescription(
      "TestPrinter", ZeroconfPrinterDetector::kIppServiceName));
  // Run listers but don't run the delayed tasks.
  task_environment_.RunUntilIdle();

  // Delete the detector.
  detector_.reset();

  // Clear task queues where we would crash if we did something wrong.
  task_environment_.FastForwardUntilNoTasksRemain();
  SUCCEED();
}

}  // namespace
}  // namespace ash

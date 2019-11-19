// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/zeroconf_printer_detector.h"

#include <algorithm>
#include <functional>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

using local_discovery::ServiceDescription;
using local_discovery::ServiceDiscoveryDeviceLister;

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

// Bitfield flags for MakeExpectedPrinter()
int kFlagSSL = 0x1;   // Use ipps, not ipp.
int kFlagIPPE = 0x2;  // Printer can be autoconfigured with IPP-Everywhere

// This corresponds to FakeServiceDeviceLister::MakeServiceDescription.  Given
// the same name (and the correct ssl/ippe flags based on the service type) this
// generates the DetectedPrinter record we expect from ZeroconfPrinterDectector
// when it gets that ServiceDescription.  This needs to be kept in sync with
// FakeServiceDeviceLister::MakeServiceDescription.
PrinterDetector::DetectedPrinter MakeExpectedPrinter(const std::string& name,
                                                     int flags) {
  PrinterDetector::DetectedPrinter detected;
  Printer& printer = detected.printer;
  net::IPAddress ip_address = GetIPAddressFor(name);
  int port = GetPortFor(name);
  bool ssl = flags & kFlagSSL;
  printer.set_uri(base::StringPrintf("ipp%s://%s.local:%d/%s_rp",
                                     ssl ? "s" : "", name.c_str(), port,
                                     name.c_str()));

  printer.set_uuid(base::StrCat({name, "_UUID"}));
  printer.set_display_name(base::StrCat({name, "_ty"}));
  printer.set_description(base::StrCat({name, "_note"}));
  printer.set_make_and_model(base::StrCat({name, "_product"}));
  detected.ppd_search_data.make_and_model.push_back(printer.display_name());
  detected.ppd_search_data.make_and_model.push_back(printer.make_and_model());
  if (GetUsbFor(name)) {
    // We should get an effective make and model guess from the usb fields
    // if they exist.
    detected.ppd_search_data.make_and_model.push_back(
        base::StrCat({name, "_usb_MFG ", name, "_usb_MDL"}));
  }

  if (flags & kFlagIPPE) {
    printer.mutable_ppd_reference()->autoconf = true;
  }
  return detected;
}

// This is a thin wrapper around Delegate that defers callbacks until
// the actual delegate is initialized, then calls all deferred callbacks.  Once
// the actual delegate is initialized, this just becomes a simple passthrough.
class DeferringDelegate : public ServiceDiscoveryDeviceLister::Delegate {
 public:
  void OnDeviceChanged(const std::string& service_type,
                       bool added,
                       const ServiceDescription& service_description) override {
    if (actual_) {
      actual_->OnDeviceChanged(service_type, added, service_description);
    } else {
      deferred_callbacks_.push_back(base::BindOnce(
          &DeferringDelegate::OnDeviceChanged, base::Unretained(this),
          service_type, added, service_description));
    }
  }
  // Not guaranteed to be called after OnDeviceChanged.
  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override {
    if (actual_) {
      actual_->OnDeviceRemoved(service_type, service_name);
    } else {
      deferred_callbacks_.push_back(
          base::BindOnce(&DeferringDelegate::OnDeviceRemoved,
                         base::Unretained(this), service_type, service_name));
    }
  }

  void OnDeviceCacheFlushed(const std::string& service_type) override {
    if (actual_) {
      actual_->OnDeviceCacheFlushed(service_type);
    } else {
      deferred_callbacks_.push_back(
          base::BindOnce(&DeferringDelegate::OnDeviceCacheFlushed,
                         base::Unretained(this), service_type));
    }
  }

  void SetActual(ServiceDiscoveryDeviceLister::Delegate* actual) {
    CHECK(!actual_);
    actual_ = actual;
    for (auto& cb : deferred_callbacks_) {
      std::move(cb).Run();
    }
    deferred_callbacks_.clear();
  }

 private:
  std::vector<base::OnceCallback<void()>> deferred_callbacks_;
  ServiceDiscoveryDeviceLister::Delegate* actual_ = nullptr;
};

// A fake ServiceDiscoveryDeviceLister.  This provides an implementation
// of ServiceDiscoveryDeviceLister that tests can use to trigger addition
// and removal of devices.
//
// There's some hackery here to handle constructor order constraints.  There's a
// circular dependency in that ZeroconfPrinterDetector (which is a device lister
// delegate) needs its device lister set to be supplied at construction time,
// and each device lister needs to know about its delegate for callbacks.  Thus
// we use DeferringDelegate to queue callbacks triggered before we have the
// delegate reference in this class, and invoke those queued callbacks when the
// Delegate is set.
class FakeServiceDiscoveryDeviceLister : public ServiceDiscoveryDeviceLister {
 public:
  FakeServiceDiscoveryDeviceLister(base::TaskRunner* task_runner,
                                   const std::string& service_type)
      : task_runner_(task_runner), service_type_(service_type) {}

  ~FakeServiceDiscoveryDeviceLister() override = default;

  // The only thing we care about with Start() is that it's called before
  // DiscoverNewDevices.
  void Start() override {
    if (start_called_) {
      ADD_FAILURE() << "Start called multiple times";
    }
    start_called_ = true;
  }

  // When DiscoverNewDevices is called, all updates we've queued up until this
  // point are invoked.
  void DiscoverNewDevices() override {
    if (!start_called_) {
      ADD_FAILURE() << "DiscoverNewDevices called before Start";
    }
    discovery_started_ = true;
    for (const auto& update : queued_updates_) {
      SendUpdate(update);
    }
    queued_updates_.clear();
  }

  const std::string& service_type() const override { return service_type_; }

  void SetDelegate(ServiceDiscoveryDeviceLister::Delegate* delegate) {
    deferring_delegate_.SetActual(delegate);
  }

  // Announce a new service or update it if we've seen it before and already
  // announced it.  If discovery hasn't started yet, queue the description
  // to be sent when discovery is started.
  void Announce(const std::string& name) {
    ServiceDescription description = MakeServiceDescription(name);
    if (!discovery_started_) {
      queued_updates_.push_back(description);
    } else {
      SendUpdate(description);
    }
  }

  void Remove(const std::string& name) {
    std::string service_name = base::StrCat({name, ".", service_type_});
    announced_services_.erase(service_name);
    CHECK(task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceDiscoveryDeviceLister::Delegate::OnDeviceRemoved,
                       base::Unretained(&deferring_delegate_), service_type_,
                       service_name)));
  }

  // Simulate an event that clears downstream caches and the lister.
  void Clear() {
    announced_services_.clear();
    discovery_started_ = false;
    CHECK(task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ServiceDiscoveryDeviceLister::Delegate::OnDeviceCacheFlushed,
            base::Unretained(&deferring_delegate_), service_type_)));
  }

  // Create a deterministic ServiceDescription based on the name and this
  // lister's service_type.  See the note on MakeExpectedPrinter, above.  This
  // is a member function instead of a free function because the service_type_
  // impacts some of the fields.  This must be kept in sync with
  // MakeExpectedPrinter.
  ServiceDescription MakeServiceDescription(const std::string& name) {
    ServiceDescription sd;
    sd.service_name = base::StrCat({name, ".", service_type_});
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

  bool discovery_started() { return discovery_started_; }

 private:
  void SendUpdate(const ServiceDescription& description) {
    bool is_new;
    if (!base::Contains(announced_services_, description.service_name)) {
      is_new = true;
      announced_services_.insert(description.service_name);
    } else {
      is_new = false;
    }
    CHECK(task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceDiscoveryDeviceLister::Delegate::OnDeviceChanged,
                       base::Unretained(&deferring_delegate_), service_type_,
                       is_new, description)));
  }
  base::TaskRunner* task_runner_;

  // Services which have previously posted an update, and therefore are no
  // longer 'new' for the purposes of the OnDeviceChanged callback.
  std::set<std::string> announced_services_;

  // Updates added to the class before discovery started.
  std::vector<ServiceDescription> queued_updates_;

  // Has Start() been called?
  bool start_called_ = false;

  // Has DiscoverNewDevices been called?
  bool discovery_started_ = false;

  std::string service_type_;
  DeferringDelegate deferring_delegate_;
};

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

    listers_[ZeroconfPrinterDetector::kIppServiceName] = std::move(ipp_lister);
    listers_[ZeroconfPrinterDetector::kIppsServiceName] =
        std::move(ipps_lister);
    listers_[ZeroconfPrinterDetector::kIppEverywhereServiceName] =
        std::move(ippe_lister);
    listers_[ZeroconfPrinterDetector::kIppsEverywhereServiceName] =
        std::move(ippse_lister);
  }
  ~ZeroconfPrinterDetectorTest() override = default;

  void CreateDetector() {
    detector_ = ZeroconfPrinterDetector::CreateForTesting(&listers_);
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
  }

  // Expect that the most up-to-date results from the detector match those
  // in printers.
  void ExpectPrintersAre(
      const std::vector<PrinterDetector::DetectedPrinter>& printers) {
    // The last observer callback should tell us the same thing as the querying
    // the detector manually.
    ASSERT_TRUE(!printers_found_callbacks_.empty());
    ExpectPrintersEq(printers, printers_found_callbacks_.back());
    ExpectPrintersEq(printers, detector_->GetPrinters());
  }

  // Expect that the detected printers list is empty.
  void ExpectPrintersEmpty() {
    // Assert that the most recent callbacks are empty.
    ASSERT_TRUE(!printers_found_callbacks_.empty());
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
  base::test::TaskEnvironment task_environment_;

  // Device listers fakes.  These are initialized when the test is constructed.
  // These pointers don't involve ownership; ownership of the listers starts
  // with this class in listers_ when the test starts, and is transferred to
  // detector_ when the detector is created.  Throughout, the listers remain
  // available to the test via these pointers.
  FakeServiceDiscoveryDeviceLister* ipp_lister_;
  FakeServiceDiscoveryDeviceLister* ipps_lister_;
  FakeServiceDiscoveryDeviceLister* ippe_lister_;
  FakeServiceDiscoveryDeviceLister* ippse_lister_;

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
  ipp_lister_->Announce("Printer1");
  CreateDetector();
  task_environment_.RunUntilIdle();
  ExpectPrintersAre({MakeExpectedPrinter("Printer1", 0)});
}

TEST_F(ZeroconfPrinterDetectorTest, SingleIppsPrinter) {
  ipps_lister_->Announce("Printer2");
  CreateDetector();
  task_environment_.RunUntilIdle();
  ExpectPrintersAre({MakeExpectedPrinter("Printer2", kFlagSSL)});
}

TEST_F(ZeroconfPrinterDetectorTest, SingleIppEverywherePrinter) {
  ippe_lister_->Announce("Printer3");
  CreateDetector();
  task_environment_.RunUntilIdle();
  ExpectPrintersAre({MakeExpectedPrinter("Printer3", kFlagIPPE)});
}

TEST_F(ZeroconfPrinterDetectorTest, SingleIppsEverywherePrinter) {
  ippse_lister_->Announce("Printer4");
  CreateDetector();
  task_environment_.RunUntilIdle();
  ExpectPrintersAre({MakeExpectedPrinter("Printer4", kFlagSSL | kFlagIPPE)});
}

// Test that an announce after the detector creation shows up as a printer.
TEST_F(ZeroconfPrinterDetectorTest, AnnounceAfterDetectorCreation) {
  CreateDetector();
  task_environment_.RunUntilIdle();
  ippse_lister_->Announce("Printer4");
  task_environment_.RunUntilIdle();
  ExpectPrintersAre({MakeExpectedPrinter("Printer4", kFlagSSL | kFlagIPPE)});
}

// Test that we use the same printer ID regardless of which service type it
// comes to us from.
TEST_F(ZeroconfPrinterDetectorTest, StableIds) {
  ipp_lister_->Announce("Printer1");
  CreateDetector();
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(printers_found_callbacks_.empty());
  ASSERT_EQ(1U, printers_found_callbacks_.back().size());
  // Grab the id when it's an IPPS printer We should continue to get the same id
  // regardless of service type.
  std::string id = printers_found_callbacks_.back()[0].printer.id();

  // Remove it as an IPP printer, add it as an IPPS printer.
  ipp_lister_->Remove("Printer1");
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(printers_found_callbacks_.back().empty());
  ipps_lister_->Announce("Printer1");
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(printers_found_callbacks_.back().empty());
  // Id should be the same.
  ASSERT_EQ(id, printers_found_callbacks_.back()[0].printer.id());

  // Remove it as an IPPS printer, add it as an IPP-Everywhere printer.
  ipps_lister_->Remove("Printer1");
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(printers_found_callbacks_.back().empty());
  ippe_lister_->Announce("Printer1");
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(printers_found_callbacks_.back().empty());
  // Id should be the same.
  ASSERT_EQ(id, printers_found_callbacks_.back()[0].printer.id());

  // Remove it as an IPP-Everywhere printer, add it as an IPPS-Everywhere
  // printer.
  ippe_lister_->Remove("Printer1");
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(printers_found_callbacks_.back().empty());
  ippse_lister_->Announce("Printer1");
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(printers_found_callbacks_.back().empty());
  // Id should be the same.
  ASSERT_EQ(id, printers_found_callbacks_.back()[0].printer.id());
}

// Test a basic removal.
TEST_F(ZeroconfPrinterDetectorTest, Removal) {
  ipp_lister_->Announce("Printer5");
  ipp_lister_->Announce("Printer6");
  ipp_lister_->Announce("Printer7");
  ipp_lister_->Announce("Printer8");
  ipp_lister_->Announce("Printer9");
  CreateDetector();
  task_environment_.RunUntilIdle();
  ExpectPrintersAre(
      {MakeExpectedPrinter("Printer5", 0), MakeExpectedPrinter("Printer6", 0),
       MakeExpectedPrinter("Printer7", 0), MakeExpectedPrinter("Printer8", 0),
       MakeExpectedPrinter("Printer9", 0)});
  ipp_lister_->Remove("Printer7");
  task_environment_.RunUntilIdle();
  ExpectPrintersAre(
      {MakeExpectedPrinter("Printer5", 0), MakeExpectedPrinter("Printer6", 0),
       MakeExpectedPrinter("Printer8", 0), MakeExpectedPrinter("Printer9", 0)});
}

// Test that, when the same printer appears in multiple services, we
// use the highest priority one.  Priorities, from highest to lowest
// are IPPS-E, IPP-E, IPPS, IPP.
TEST_F(ZeroconfPrinterDetectorTest, ServiceTypePriorities) {
  // Advertise on all 4 services.
  ipp_lister_->Announce("Printer5");
  ipps_lister_->Announce("Printer5");
  ippe_lister_->Announce("Printer5");
  ippse_lister_->Announce("Printer5");
  CreateDetector();
  task_environment_.RunUntilIdle();
  // IPPS-E is highest priority.
  ExpectPrintersAre({MakeExpectedPrinter("Printer5", kFlagSSL | kFlagIPPE)});
  ippse_lister_->Remove("Printer5");
  task_environment_.RunUntilIdle();
  // IPP-E is highest remaining priority.
  ExpectPrintersAre({MakeExpectedPrinter("Printer5", kFlagIPPE)});

  ippe_lister_->Remove("Printer5");
  task_environment_.RunUntilIdle();
  // IPPS is highest remaining priority.
  ExpectPrintersAre({MakeExpectedPrinter("Printer5", kFlagSSL)});

  ipps_lister_->Remove("Printer5");
  task_environment_.RunUntilIdle();
  // IPP is only remaining entry.
  ExpectPrintersAre({MakeExpectedPrinter("Printer5", 0)});

  ipp_lister_->Remove("Printer5");
  task_environment_.RunUntilIdle();
  // No entries left.
  ExpectPrintersEmpty();
}

// Test that cache flushes appropriately remove entries.
TEST_F(ZeroconfPrinterDetectorTest, CacheFlushes) {
  ipp_lister_->Announce("Printer6");
  ipp_lister_->Announce("Printer7");
  ipps_lister_->Announce("Printer7");
  ipps_lister_->Announce("Printer8");
  ippe_lister_->Announce("Printer8");
  ippe_lister_->Announce("Printer9");
  ippse_lister_->Announce("Printer9");
  ippse_lister_->Announce("Printer10");

  CreateDetector();
  task_environment_.RunUntilIdle();
  ExpectPrintersAre({MakeExpectedPrinter("Printer6", 0),
                     MakeExpectedPrinter("Printer7", kFlagSSL),
                     MakeExpectedPrinter("Printer8", kFlagIPPE),
                     MakeExpectedPrinter("Printer9", kFlagSSL | kFlagIPPE),
                     MakeExpectedPrinter("Printer10", kFlagSSL | kFlagIPPE)});

  ipps_lister_->Clear();

  task_environment_.RunUntilIdle();
  // With the IPPS lister cleared, all printers should be cleared.
  ExpectPrintersEmpty();

  // We should have restarted discovery after dealing with the cache flush.
  EXPECT_TRUE(ipps_lister_->discovery_started());

  // Just for kicks, announce something new at this point.
  ipps_lister_->Announce("Printer11");
  task_environment_.RunUntilIdle();
  ExpectPrintersAre({MakeExpectedPrinter("Printer11", kFlagSSL)});

  // Clear out the IPPS lister, which will clear all printers too.
  ipps_lister_->Clear();
  task_environment_.RunUntilIdle();

  // With the IPPS lister cleared, Printer11 should disappear.
  ExpectPrintersEmpty();
  EXPECT_TRUE(ippe_lister_->discovery_started());
}

// Test some general traffic with a mix of everything we expect to handle.
TEST_F(ZeroconfPrinterDetectorTest, GeneralMixedTraffic) {
  ipp_lister_->Announce("Printer12");
  ipps_lister_->Announce("Printer12");
  ipps_lister_->Announce("Printer13");
  ippse_lister_->Announce("Printer14");
  ipps_lister_->Announce("Printer15");

  CreateDetector();
  task_environment_.RunUntilIdle();
  ExpectPrintersAre({MakeExpectedPrinter("Printer12", kFlagSSL),
                     MakeExpectedPrinter("Printer13", kFlagSSL),
                     MakeExpectedPrinter("Printer14", kFlagSSL | kFlagIPPE),
                     MakeExpectedPrinter("Printer15", kFlagSSL)});

  ippe_lister_->Announce("Printer13");
  ipp_lister_->Announce("Printer16");
  task_environment_.RunUntilIdle();
  ExpectPrintersAre({MakeExpectedPrinter("Printer12", kFlagSSL),
                     MakeExpectedPrinter("Printer13", kFlagIPPE),
                     MakeExpectedPrinter("Printer14", kFlagSSL | kFlagIPPE),
                     MakeExpectedPrinter("Printer15", kFlagSSL),
                     MakeExpectedPrinter("Printer16", 0)});

  ipp_lister_->Remove("NonexistantPrinter");
  ipps_lister_->Remove("Printer12");
  ipps_lister_->Clear();
  task_environment_.RunUntilIdle();
  ExpectPrintersEmpty();
}

}  // namespace
}  // namespace chromeos

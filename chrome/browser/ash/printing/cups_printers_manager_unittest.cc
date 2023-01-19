// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_printers_manager.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/enterprise_printers_provider.h"
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "chrome/browser/ash/printing/printer_event_tracker.h"
#include "chrome/browser/ash/printing/printers_map.h"
#include "chrome/browser/ash/printing/server_printers_provider.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chrome/browser/ash/printing/test_printer_configurer.h"
#include "chrome/browser/ash/printing/usb_printer_detector.h"
#include "chrome/browser/ash/printing/usb_printer_notification_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/printing/ppd_provider.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace {

using ::chromeos::kPrinterId;
using ::chromeos::PpdProvider;
using ::chromeos::Printer;
using ::chromeos::PrinterClass;
using ::chromeos::PrinterSearchData;

// Fake backend for EnterprisePrintersProvider.  This allows us to poke
// arbitrary changes in the enterprise printer lists.
class FakeEnterprisePrintersProvider : public EnterprisePrintersProvider {
 public:
  FakeEnterprisePrintersProvider() = default;
  ~FakeEnterprisePrintersProvider() override = default;

  // Attach |observer| for notification of events.  |observer| is expected to
  // live on the same thread (UI) as this object.  OnPrinter* methods are
  // invoked inline so calling RegisterPrinter in response to OnPrinterAdded is
  // forbidden.
  void AddObserver(EnterprisePrintersProvider::Observer* observer) override {
    observers_.AddObserver(observer);
  }

  // Remove |observer| so that it no longer receives notifications.  After the
  // completion of this method, the |observer| can be safely destroyed.
  void RemoveObserver(EnterprisePrintersProvider::Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  // Fake manipulation functions.

  // Add the given printers to the list of enterprise printers and
  // notify observers.
  void AddEnterprisePrinters(const std::vector<Printer>& printers) {
    enterprise_printers_.insert(enterprise_printers_.end(), printers.begin(),
                                printers.end());
    for (Observer& observer : observers_) {
      observer.OnPrintersChanged(true, enterprise_printers_);
    }
  }

 private:
  base::ObserverList<EnterprisePrintersProvider::Observer>::Unchecked
      observers_;
  std::vector<Printer> enterprise_printers_;
};

// Fake backend for SyncedPrintersManager.  This allows us to poke arbitrary
// changes in the saved printer lists.
class FakeSyncedPrintersManager : public SyncedPrintersManager {
 public:
  FakeSyncedPrintersManager() = default;
  ~FakeSyncedPrintersManager() override = default;

  // Returns the printers that are saved in preferences.
  std::vector<Printer> GetSavedPrinters() const override {
    return saved_printers_;
  }

  // Attach |observer| for notification of events.  |observer| is expected to
  // live on the same thread (UI) as this object.  OnPrinter* methods are
  // invoked inline so calling RegisterPrinter in response to OnPrinterAdded is
  // forbidden.
  void AddObserver(SyncedPrintersManager::Observer* observer) override {
    observers_.AddObserver(observer);
  }

  // Remove |observer| so that it no longer receives notifications.  After the
  // completion of this method, the |observer| can be safely destroyed.
  void RemoveObserver(SyncedPrintersManager::Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void UpdateSavedPrinter(const Printer& printer) override {
    if (!IsPrinterAlreadySaved(printer)) {
      SavePrinter(printer);
      return;
    }

    for (size_t i = 0; i < saved_printers_.size(); ++i) {
      if (saved_printers_[i].id() == printer.id()) {
        saved_printers_[i] = printer;
        break;
      }
    }

    NotifyOnSavedPrintersObservers();
  }

  bool RemoveSavedPrinter(const std::string& printer_id) override {
    for (auto it = saved_printers_.begin(); it != saved_printers_.end(); ++it) {
      if (it->id() == printer_id) {
        saved_printers_.erase(it);
        NotifyOnSavedPrintersObservers();
        return true;
      }
    }
    return false;
  }

  // Everything else in the interface we either don't use in
  // CupsPrintersManager, or just use in a simple pass-through manner that's not
  // worth additional layers of testing on top of the testing in
  // SyncedPrintersManager.
  PrintersSyncBridge* GetSyncBridge() override { return nullptr; }
  // Returns the printer with id |printer_id|, or nullptr if no such printer
  // exists.
  std::unique_ptr<Printer> GetPrinter(
      const std::string& printer_id) const override {
    return nullptr;
  }

  // Fake manipulation functions.

  // Add the given printers to the list of saved printers and
  // notify observers.
  void AddSavedPrinters(const std::vector<Printer>& printers) {
    saved_printers_.insert(saved_printers_.end(), printers.begin(),
                           printers.end());
    NotifyOnSavedPrintersObservers();
  }

  // Remove the printers with the given ids from the set of saved printers,
  // notify observers.
  void RemoveSavedPrinters(const std::unordered_set<std::string>& ids) {
    RemovePrinters(ids, &saved_printers_);
    NotifyOnSavedPrintersObservers();
  }

 private:
  void RemovePrinters(const std::unordered_set<std::string>& ids,
                      std::vector<Printer>* target) {
    auto new_end = std::remove_if(target->begin(), target->end(),
                                  [&ids](const Printer& printer) -> bool {
                                    return base::Contains(ids, printer.id());
                                  });

    target->resize(new_end - target->begin());
  }

  bool IsPrinterAlreadySaved(const Printer& printer) const {
    for (const Printer& saved_printer : saved_printers_) {
      if (printer.id() == saved_printer.id()) {
        return true;
      }
    }
    return false;
  }

  void SavePrinter(const Printer& printer) {
    DCHECK(!IsPrinterAlreadySaved(printer));

    saved_printers_.push_back(printer);

    NotifyOnSavedPrintersObservers();
  }

  void NotifyOnSavedPrintersObservers() const {
    for (Observer& observer : observers_) {
      observer.OnSavedPrintersChanged();
    }
  }

  base::ObserverList<SyncedPrintersManager::Observer>::Unchecked observers_;
  std::vector<Printer> saved_printers_;
};

class FakePrinterDetector : public PrinterDetector {
 public:
  FakePrinterDetector() {}
  ~FakePrinterDetector() override = default;

  void RegisterPrintersFoundCallback(OnPrintersFoundCallback cb) override {
    on_printers_found_callback_ = std::move(cb);
  }

  std::vector<DetectedPrinter> GetPrinters() override { return detections_; }

  void AddDetections(
      const std::vector<PrinterDetector::DetectedPrinter>& new_detections) {
    detections_.insert(detections_.end(), new_detections.begin(),
                       new_detections.end());
    on_printers_found_callback_.Run(detections_);
  }

  // Remove printers that have ids in ids.
  void RemoveDetections(const std::unordered_set<std::string>& ids) {
    auto new_end =
        std::remove_if(detections_.begin(), detections_.end(),
                       [&ids](const DetectedPrinter& detection) -> bool {
                         return base::Contains(ids, detection.printer.id());
                       });

    detections_.resize(new_end - detections_.begin());
    on_printers_found_callback_.Run(detections_);
  }

 private:
  std::vector<DetectedPrinter> detections_;
  OnPrintersFoundCallback on_printers_found_callback_;
};

// Fake PpdProvider backend.  This fake generates PpdReferences based on
// the passed make_and_model strings using these rules:
//
// If make_and_model is empty, then we say NOT_FOUND
// Otherwise, generate a ppd reference with make_and_model[0] as
// the effective make and model in the PpdReference.
class FakePpdProvider : public PpdProvider {
 public:
  FakePpdProvider() {}

  void ResolvePpdReference(const PrinterSearchData& search_data,
                           ResolvePpdReferenceCallback cb) override {
    if (search_data.make_and_model.empty()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb), PpdProvider::NOT_FOUND,
                         Printer::PpdReference(), usb_manufacturer_));
    } else {
      Printer::PpdReference ret;
      ret.effective_make_and_model = search_data.make_and_model[0];
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), PpdProvider::SUCCESS, ret,
                                    "" /* usb_manufacturer */));
    }
  }

  void SetUsbManufacturer(const std::string& manufacturer) {
    usb_manufacturer_ = manufacturer;
  }

  // These methods are not used by CupsPrintersManager.
  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {}
  void ResolveManufacturers(ResolveManufacturersCallback cb) override {}
  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {}
  void ResolvePpdLicense(base::StringPiece effective_make_and_model,
                         ResolvePpdLicenseCallback cb) override {}
  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {}

 private:
  ~FakePpdProvider() override {}
  std::string usb_manufacturer_;
};

// Expect that the printers in printers have the given ids, without
// considering order.
void ExpectPrinterIdsAre(const std::vector<Printer>& printers,
                         const std::vector<std::string>& ids) {
  std::vector<std::string> found_ids;
  for (const Printer& printer : printers) {
    found_ids.push_back(printer.id());
  }
  std::sort(found_ids.begin(), found_ids.end());
  std::vector<std::string> sorted_ids(ids);
  std::sort(sorted_ids.begin(), sorted_ids.end());
  EXPECT_EQ(sorted_ids, found_ids);
}

class FakeUsbPrinterNotificationController
    : public UsbPrinterNotificationController {
 public:
  FakeUsbPrinterNotificationController() = default;
  ~FakeUsbPrinterNotificationController() override = default;

  void ShowEphemeralNotification(const Printer& printer) override {
    NOTIMPLEMENTED();
  }
  void ShowConfigurationNotification(const Printer& printer) override {
    configuration_notifications_.insert(printer.id());
  }
  void ShowSavedNotification(const Printer& printer) override {
    saved_notifications_.insert(printer.id());
  }
  void RemoveNotification(const std::string& printer_id) override {
    saved_notifications_.erase(printer_id);
    configuration_notifications_.erase(printer_id);
  }
  bool IsNotificationDisplayed(const std::string& printer_id) const override {
    return configuration_notifications_.contains(printer_id) ||
           saved_notifications_.contains(printer_id);
  }

  bool IsSavedNotification(const std::string& printer_id) const {
    return saved_notifications_.contains(printer_id);
  }

  bool IsConfigurationNotification(const std::string& printer_id) const {
    return configuration_notifications_.contains(printer_id);
  }

 private:
  base::flat_set<std::string> saved_notifications_;
  base::flat_set<std::string> configuration_notifications_;
};

class FakePrintServersManager : public PrintServersManager {
 public:
  FakePrintServersManager() = default;
  ~FakePrintServersManager() override = default;

  void AddObserver(Observer* observer) override { observer_ = observer; }
  void RemoveObserver(Observer* observer) override { observer_ = nullptr; }
  void ChoosePrintServer(
      const std::vector<std::string>& selected_print_server_ids) override {}
  PrintServersConfig GetPrintServersConfig() const override {
    return PrintServersConfig();
  }

  void ServerPrintersChanged(
      const std::vector<PrinterDetector::DetectedPrinter>& printers) {
    observer_->OnServerPrintersChanged(printers);
  }

 private:
  Observer* observer_;
};

class CupsPrintersManagerTest : public testing::Test,
                                public CupsPrintersManager::Observer {
 public:
  CupsPrintersManagerTest() : ppd_provider_(new FakePpdProvider) {
    // Zeroconf and usb detector ownerships are taken by the manager, so we
    // have to keep raw pointers to them.
    auto zeroconf_detector = std::make_unique<FakePrinterDetector>();
    zeroconf_detector_ = zeroconf_detector.get();
    auto usb_detector = std::make_unique<FakePrinterDetector>();
    usb_detector_ = usb_detector.get();
    auto printer_configurer = std::make_unique<TestPrinterConfigurer>();
    printer_configurer_ = printer_configurer.get();
    auto usb_notif_controller =
        std::make_unique<FakeUsbPrinterNotificationController>();
    usb_notif_controller_ = usb_notif_controller.get();
    auto enterprise_printers_provider =
        std::make_unique<FakeEnterprisePrintersProvider>();
    enterprise_printers_provider_ = enterprise_printers_provider.get();
    auto print_servers_manager = std::make_unique<FakePrintServersManager>();
    print_servers_manager_ = print_servers_manager.get();

    // Register the pref |UserPrintersAllowed|
    CupsPrintersManager::RegisterProfilePrefs(pref_service_.registry());

    manager_ = CupsPrintersManager::CreateForTesting(
        &synced_printers_manager_, std::move(usb_detector),
        std::move(zeroconf_detector), ppd_provider_,
        std::move(printer_configurer), std::move(usb_notif_controller),
        std::move(print_servers_manager),
        std::move(enterprise_printers_provider), &event_tracker_,
        &pref_service_);
    manager_->AddObserver(this);
  }

  ~CupsPrintersManagerTest() override {
    // Fast forwarding so that delayed tasks like |SendScannerCountToUMA| will
    // run and not leak memory in unused callbacks.
    task_environment_.FastForwardUntilNoTasksRemain();
  }

  // CupsPrintersManager::Observer implementation
  void OnPrintersChanged(PrinterClass printer_class,
                         const std::vector<Printer>& printers) override {
    observed_printers_[printer_class] = printers;
  }

  // Check that, for the given printer class, the printers we have from the
  // observation callback and the printers we have when we query the manager
  // are both the same and have the passed ids.
  void ExpectPrintersInClassAre(PrinterClass printer_class,
                                const std::vector<std::string>& ids) {
    ExpectPrinterIdsAre(manager_->GetPrinters(printer_class), ids);
    ExpectPrinterIdsAre(observed_printers_[printer_class], ids);
  }

  void UpdatePolicyValue(const char* name, bool value) {
    auto value_ptr = std::make_unique<base::Value>(value);
    // TestingPrefSyncableService assumes ownership of |value_ptr|.
    pref_service_.SetManagedPref(name, std::move(value_ptr));
  }

  static PrintServer CreatePrintServer(std::string id,
                                       std::string server_url,
                                       std::string name) {
    GURL url(server_url);
    PrintServer print_server(id, url, name);
    return print_server;
  }

 protected:
  // Everything from PrintServersProvider must be called on Chrome_UIThread
  // Note: MainThreadType::IO is strictly about requesting a specific
  // MessagePumpType for the main thread. It has nothing to do with
  // BrowserThread::UI or BrowserThread::IO which are named threads in the
  // //content/browser code.
  // See
  // //docs/threading_and_tasks_testing.md#mainthreadtype-trait
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Captured printer lists from observer callbacks.
  base::flat_map<PrinterClass, std::vector<Printer>> observed_printers_;

  // Backend fakes driving the CupsPrintersManager.
  FakeSyncedPrintersManager synced_printers_manager_;
  FakeEnterprisePrintersProvider* enterprise_printers_provider_;  // Not owned.
  FakePrinterDetector* usb_detector_;                             // Not owned.
  FakePrinterDetector* zeroconf_detector_;                        // Not owned.
  TestPrinterConfigurer* printer_configurer_;                     // Not owned.
  FakeUsbPrinterNotificationController* usb_notif_controller_;    // Not owned.
  FakePrintServersManager* print_servers_manager_;                // Not owned.
  scoped_refptr<FakePpdProvider> ppd_provider_;

  // This is unused, it's just here for memory ownership.
  PrinterEventTracker event_tracker_;

  // PrefService used to register the |UserPrintersAllowed| pref and
  // change its value for testing.
  sync_preferences::TestingPrefServiceSyncable pref_service_;

  // The manager being tested.  This must be declared after the fakes, as its
  // initialization must come after that of the fakes.
  std::unique_ptr<CupsPrintersManager> manager_;

  // Manages active networks.
  network_config::CrosNetworkConfigTestHelper cros_network_config_helper_;
};

// Pseudo-constructor for inline creation of a DetectedPrinter that should (in
// this test) be handled as a Discovered printer (because it has no make and
// model information, and that's now the FakePpdProvider is set up to
// determine whether or not something has a Ppd available).
PrinterDetector::DetectedPrinter MakeDiscoveredPrinter(const std::string& id,
                                                       const std::string& uri) {
  PrinterDetector::DetectedPrinter ret;
  ret.printer.set_id(id);
  ret.printer.SetUri(uri);
  return ret;
}

// Calls MakeDiscoveredPrinter with empty uri.
PrinterDetector::DetectedPrinter MakeDiscoveredPrinter(const std::string& id) {
  return MakeDiscoveredPrinter(id, "" /* uri */);
}

// Calls MakeDiscoveredPrinter with the USB protocol as the uri.
PrinterDetector::DetectedPrinter MakeUsbDiscoveredPrinter(
    const std::string& id) {
  return MakeDiscoveredPrinter(id, "usb://host/path");
}

// Pseudo-constructor for inline creation of a DetectedPrinter that should (in
// this test) be handled as an Automatic printer (because it has a make and
// model string).
PrinterDetector::DetectedPrinter MakeAutomaticPrinter(const std::string& id) {
  PrinterDetector::DetectedPrinter ret;
  ret.printer.set_id(id);
  ret.ppd_search_data.make_and_model.push_back("make and model string");
  return ret;
}

// Test that Enterprise printers from SyncedPrinterManager are
// surfaced appropriately.
TEST_F(CupsPrintersManagerTest, GetEnterprisePrinters) {
  enterprise_printers_provider_->AddEnterprisePrinters(
      {Printer("Foo"), Printer("Bar")});
  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kEnterprise, {"Foo", "Bar"});
}

// Test that Saved printers from SyncedPrinterManager are
// surfaced appropriately.
TEST_F(CupsPrintersManagerTest, GetSavedPrinters) {
  synced_printers_manager_.AddSavedPrinters({Printer("Foo"), Printer("Bar")});
  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kSaved, {"Foo", "Bar"});
}

// Test that USB printers from the usb detector are converted to 'Printer's
// and surfaced appropriately.  One printer should be "automatic" because it
// has a findable Ppd, the other should be "discovered".
TEST_F(CupsPrintersManagerTest, GetUsbPrinters) {
  usb_detector_->AddDetections({MakeDiscoveredPrinter("DiscoveredPrinter"),
                                MakeAutomaticPrinter("AutomaticPrinter")});
  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"DiscoveredPrinter"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"AutomaticPrinter"});
}

// Same as GetUsbPrinters, only for Zeroconf printers.
TEST_F(CupsPrintersManagerTest, GetZeroconfPrinters) {
  zeroconf_detector_->AddDetections({MakeDiscoveredPrinter("DiscoveredPrinter"),
                                     MakeAutomaticPrinter("AutomaticPrinter")});
  synced_printers_manager_.AddSavedPrinters({Printer("Foo"), Printer("Bar")});

  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"DiscoveredPrinter"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"AutomaticPrinter"});
}

// Test that printers that appear in either a Saved or Enterprise set do
// *not* appear in Discovered or Automatic, even if they are detected as such.
TEST_F(CupsPrintersManagerTest, SyncedPrintersTrumpDetections) {
  zeroconf_detector_->AddDetections(
      {MakeDiscoveredPrinter("DiscoveredPrinter0"),
       MakeDiscoveredPrinter("DiscoveredPrinter1"),
       MakeAutomaticPrinter("AutomaticPrinter0"),
       MakeAutomaticPrinter("AutomaticPrinter1")});
  task_environment_.RunUntilIdle();
  // Before we muck with anything else, check that automatic and discovered
  // classes are what we intended to set up.
  ExpectPrintersInClassAre(PrinterClass::kDiscovered,
                           {"DiscoveredPrinter0", "DiscoveredPrinter1"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic,
                           {"AutomaticPrinter0", "AutomaticPrinter1"});

  // Save both the Discovered and Automatic printers.  This should put them
  // into the Saved class and thus *remove* them from their previous
  // classes.
  manager_->PrinterInstalled(Printer("DiscoveredPrinter0"),
                             /*is_automatic=*/true);
  manager_->SavePrinter(Printer("DiscoveredPrinter0"));
  manager_->PrinterInstalled(Printer("AutomaticPrinter0"),
                             /*is_automatic=*/true);
  manager_->SavePrinter(Printer("AutomaticPrinter0"));
  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"DiscoveredPrinter1"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"AutomaticPrinter1"});
  ExpectPrintersInClassAre(PrinterClass::kSaved,
                           {"DiscoveredPrinter0", "AutomaticPrinter0"});
}

// Test updates of saved printers.  Updates of existing saved printers
// should propagate.  Updates of printers in other classes should result in
// those printers becoming saved.  Updates of unknown printers should
// result in a new saved printer.
TEST_F(CupsPrintersManagerTest, SavePrinter) {
  // Start with a printer in each class named after the class it's in, except
  // Enterprise which is not relevant to this test.
  Printer existing_saved("Saved");
  synced_printers_manager_.AddSavedPrinters({existing_saved});
  usb_detector_->AddDetections({MakeDiscoveredPrinter("Discovered")});
  zeroconf_detector_->AddDetections({MakeAutomaticPrinter("Automatic")});
  task_environment_.RunUntilIdle();

  // Sanity check that we do, indeed, have one printer in each class.
  ExpectPrintersInClassAre(PrinterClass::kSaved, {"Saved"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"Automatic"});
  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"Discovered"});

  // Update the existing saved printer.  Check that the new display name
  // propagated.
  existing_saved.set_display_name("New Display Name");
  manager_->SavePrinter(existing_saved);
  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kSaved, {"Saved"});
  EXPECT_EQ(manager_->GetPrinters(PrinterClass::kSaved)[0].display_name(),
            "New Display Name");

  // Do the same thing for the Automatic and Discovered printers.
  // Create a configuration for the zeroconf printer, which should shift it
  // into the saved category.
  manager_->SavePrinter(Printer("Automatic"));
  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {});
  ExpectPrintersInClassAre(PrinterClass::kSaved, {"Automatic", "Saved"});

  manager_->SavePrinter(Printer("Discovered"));
  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {});
  ExpectPrintersInClassAre(PrinterClass::kSaved,
                           {"Automatic", "Saved", "Discovered"});

  // Save a printer we haven't seen before, which should just add it to
  // kSaved.
  manager_->SavePrinter(Printer("NewFangled"));
  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kSaved,
                           {"Automatic", "Saved", "Discovered", "NewFangled"});

  // Remove the automatic printer, make sure it ends up back in the automatic
  // class after removal.
  manager_->RemoveSavedPrinter("Automatic");
  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kSaved,
                           {"Saved", "Discovered", "NewFangled"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"Automatic"});
}

// Test that GetPrinter() finds printers in any class, and returns null if
// a printer is not found.
TEST_F(CupsPrintersManagerTest, GetPrinter) {
  synced_printers_manager_.AddSavedPrinters({Printer("Saved")});
  enterprise_printers_provider_->AddEnterprisePrinters({Printer("Enterprise")});
  usb_detector_->AddDetections({MakeDiscoveredPrinter("Discovered")});
  zeroconf_detector_->AddDetections({MakeAutomaticPrinter("Automatic")});
  task_environment_.RunUntilIdle();

  for (const std::string& id :
       {"Saved", "Enterprise", "Discovered", "Automatic"}) {
    absl::optional<Printer> printer = manager_->GetPrinter(id);
    ASSERT_TRUE(printer);
    EXPECT_EQ(printer->id(), id);
  }

  absl::optional<Printer> printer = manager_->GetPrinter("Nope");
  EXPECT_FALSE(printer);
}

// Test that if |UserPrintersAllowed| pref is set to false, then
// GetPrinters() will only return printers from
// |PrinterClass::kEnterprise|.
TEST_F(CupsPrintersManagerTest, GetPrintersUserNativePrintersDisabled) {
  synced_printers_manager_.AddSavedPrinters({Printer("Saved")});
  enterprise_printers_provider_->AddEnterprisePrinters({Printer("Enterprise")});
  task_environment_.RunUntilIdle();

  // Disable the use of non-enterprise printers.
  UpdatePolicyValue(prefs::kUserPrintersAllowed, false);

  // Verify that non-enterprise printers are not returned by GetPrinters()
  std::vector<Printer> saved_printers =
      manager_->GetPrinters(PrinterClass::kSaved);
  ExpectPrinterIdsAre(saved_printers, {});

  // Verify that enterprise printers are returned by GetPrinters()
  std::vector<Printer> enterprise_printers =
      manager_->GetPrinters(PrinterClass::kEnterprise);
  ExpectPrinterIdsAre(enterprise_printers, {"Enterprise"});
}

// Test that if |UserPrintersAllowed| pref is set to false, then
// SavePrinter() will simply do nothing.
TEST_F(CupsPrintersManagerTest, SavePrinterUserNativePrintersDisabled) {
  // Start by installing a saved printer to be used to test than any
  // changes made to the printer will not be propagated.
  Printer existing_saved("Saved");
  synced_printers_manager_.AddSavedPrinters({existing_saved});
  usb_detector_->AddDetections({MakeDiscoveredPrinter("Discovered")});
  zeroconf_detector_->AddDetections({MakeAutomaticPrinter("Automatic")});
  task_environment_.RunUntilIdle();

  // Sanity check that we do, indeed, have one printer in each class.
  ExpectPrintersInClassAre(PrinterClass::kSaved, {"Saved"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"Automatic"});
  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"Discovered"});

  // Disable the use of non-enterprise printers.
  UpdatePolicyValue(prefs::kUserPrintersAllowed, false);

  // Update the existing saved printer. Verify that the changes did not
  // progogate.
  existing_saved.set_display_name("New Display Name");
  manager_->SavePrinter(existing_saved);
  task_environment_.RunUntilIdle();

  // Reenable user printers in order to do checking.
  UpdatePolicyValue(prefs::kUserPrintersAllowed, true);
  ExpectPrintersInClassAre(PrinterClass::kSaved, {"Saved"});
  EXPECT_EQ(manager_->GetPrinters(PrinterClass::kSaved)[0].display_name(), "");
  UpdatePolicyValue(prefs::kUserPrintersAllowed, false);

  // Attempt to update the Automatic and Discovered printers. In both cases
  // check that the printers do not move into the saved category.
  manager_->SavePrinter(Printer("Automatic"));
  task_environment_.RunUntilIdle();
  UpdatePolicyValue(prefs::kUserPrintersAllowed, true);
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"Automatic"});
  ExpectPrintersInClassAre(PrinterClass::kSaved, {"Saved"});
  UpdatePolicyValue(prefs::kUserPrintersAllowed, false);

  manager_->SavePrinter(Printer("Discovered"));
  task_environment_.RunUntilIdle();
  UpdatePolicyValue(prefs::kUserPrintersAllowed, true);
  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"Discovered"});
  ExpectPrintersInClassAre(PrinterClass::kSaved, {"Saved"});
  UpdatePolicyValue(prefs::kUserPrintersAllowed, false);

  // Attempt to update a printer that we haven't seen before, check that
  // nothing changed.
  manager_->SavePrinter(Printer("NewFangled"));
  task_environment_.RunUntilIdle();
  UpdatePolicyValue(prefs::kUserPrintersAllowed, true);
  ExpectPrintersInClassAre(PrinterClass::kSaved, {"Saved"});
}

// Test that if |UserPrintersAllowed| pref is set to false GetPrinter only
// returns a printer when the given printer id corresponds to an enterprise
// printer. Otherwise, it returns nothing.
TEST_F(CupsPrintersManagerTest, GetPrinterUserNativePrintersDisabled) {
  synced_printers_manager_.AddSavedPrinters({Printer("Saved")});
  enterprise_printers_provider_->AddEnterprisePrinters({Printer("Enterprise")});
  task_environment_.RunUntilIdle();

  // Sanity check that the printers were added.
  ExpectPrintersInClassAre(PrinterClass::kSaved, {"Saved"});
  ExpectPrintersInClassAre(PrinterClass::kEnterprise, {"Enterprise"});

  // Disable the use of non-enterprise printers.
  UpdatePolicyValue(prefs::kUserPrintersAllowed, false);

  absl::optional<Printer> saved_printer = manager_->GetPrinter("Saved");
  EXPECT_FALSE(saved_printer);

  absl::optional<Printer> enterprise_printer =
      manager_->GetPrinter("Enterprise");
  ASSERT_TRUE(enterprise_printer);
  EXPECT_EQ(enterprise_printer->id(), "Enterprise");
}

TEST_F(CupsPrintersManagerTest, SetUsbManufacturer) {
  const std::string& expected_manufacturer = "HP";
  ppd_provider_->SetUsbManufacturer(expected_manufacturer);
  usb_detector_->AddDetections({MakeUsbDiscoveredPrinter("DiscoveredPrinter")});
  task_environment_.RunUntilIdle();

  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"DiscoveredPrinter"});

  EXPECT_EQ(
      expected_manufacturer,
      manager_->GetPrinter("DiscoveredPrinter")->usb_printer_manufacturer());
}

TEST_F(CupsPrintersManagerTest, EmptyUsbManufacturer) {
  usb_detector_->AddDetections({MakeUsbDiscoveredPrinter("DiscoveredPrinter")});
  task_environment_.RunUntilIdle();

  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"DiscoveredPrinter"});

  EXPECT_TRUE(manager_->GetPrinter("DiscoveredPrinter")
                  ->usb_printer_manufacturer()
                  .empty());
}

TEST_F(CupsPrintersManagerTest, PrinterNotInstalled) {
  Printer printer(kPrinterId);
  EXPECT_FALSE(manager_->IsPrinterInstalled(printer));
}

TEST_F(CupsPrintersManagerTest, PrinterIsInstalled) {
  Printer printer(kPrinterId);
  manager_->PrinterInstalled(printer, /*is_automatic=*/false);
  EXPECT_TRUE(manager_->IsPrinterInstalled(printer));
}

// Test that we detect that the configuration is stale when any of the
// relevant fields change.
TEST_F(CupsPrintersManagerTest, UpdatedPrinterConfiguration) {
  Printer printer(kPrinterId);
  manager_->PrinterInstalled(printer, /*is_automatic=*/false);

  Printer updated(printer);
  updated.SetUri("ipp://different.value");
  EXPECT_FALSE(manager_->IsPrinterInstalled(updated));

  updated = printer;
  updated.mutable_ppd_reference()->autoconf = true;
  EXPECT_FALSE(manager_->IsPrinterInstalled(updated));

  updated = printer;
  updated.mutable_ppd_reference()->user_supplied_ppd_url = "different value";
  EXPECT_FALSE(manager_->IsPrinterInstalled(updated));

  updated = printer;
  updated.mutable_ppd_reference()->effective_make_and_model = "different value";
  EXPECT_FALSE(manager_->IsPrinterInstalled(updated));

  updated = printer;
  updated.mutable_ppd_reference()->autoconf = true;
  EXPECT_FALSE(manager_->IsPrinterInstalled(updated));

  // Sanity check, configuration for the original printers should still be
  // current.
  EXPECT_TRUE(manager_->IsPrinterInstalled(printer));
}

// Test that we can save non-discovered printers.
TEST_F(CupsPrintersManagerTest, SavePrinterSucceedsOnManualPrinter) {
  Printer printer(kPrinterId);
  printer.SetUri("ipp://manual.uri");
  manager_->SavePrinter(printer);

  auto saved_printers = manager_->GetPrinters(PrinterClass::kSaved);
  ASSERT_EQ(1u, saved_printers.size());
  EXPECT_EQ(printer.uri(), saved_printers[0].uri());
}

// Test that installing a printer does not put it in the saved class.
TEST_F(CupsPrintersManagerTest, PrinterInstalledDoesNotSavePrinter) {
  Printer printer(kPrinterId);
  manager_->PrinterInstalled(printer, /*is_automatic=*/false);

  auto saved_printers = manager_->GetPrinters(PrinterClass::kSaved);
  EXPECT_EQ(0u, saved_printers.size());
}

// Test that calling SavePrinter() when printer configuration change updates
// the saved printer but does not install the updated printer.
TEST_F(CupsPrintersManagerTest, SavePrinterUpdatesPreviouslyInstalledPrinter) {
  Printer printer(kPrinterId);
  manager_->PrinterInstalled(printer, /*is_automatic=*/false);
  manager_->SavePrinter(printer);
  EXPECT_TRUE(manager_->IsPrinterInstalled(printer));

  Printer updated(printer);
  updated.SetUri("ipps://different/value");
  EXPECT_FALSE(manager_->IsPrinterInstalled(updated));

  manager_->SavePrinter(updated);
  auto saved_printers = manager_->GetPrinters(PrinterClass::kSaved);
  ASSERT_EQ(1u, saved_printers.size());
  EXPECT_EQ(updated.uri(), saved_printers[0].uri());

  // Even though the updated printer was saved, it still needs to be marked as
  // installed again.
  EXPECT_FALSE(manager_->IsPrinterInstalled(updated));
}

// Automatic USB Printer is configured automatically.
TEST_F(CupsPrintersManagerTest, AutomaticUsbPrinterIsInstalledAutomatically) {
  auto automatic_printer = MakeAutomaticPrinter(kPrinterId);
  automatic_printer.printer.SetUri("usb://host/path");

  usb_detector_->AddDetections({automatic_printer});

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(printer_configurer_->IsConfigured(kPrinterId));
}

// Automatic USB Printer is *not* configured if |UserPrintersAllowed|
// pref is set to false.
TEST_F(CupsPrintersManagerTest, AutomaticUsbPrinterNotInstalledAutomatically) {
  // Disable the use of non-enterprise printers.
  UpdatePolicyValue(prefs::kUserPrintersAllowed, false);

  auto automatic_printer = MakeAutomaticPrinter(kPrinterId);
  automatic_printer.printer.SetUri("usb://host/path");

  zeroconf_detector_->AddDetections({automatic_printer});

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(manager_->IsPrinterInstalled(automatic_printer.printer));
}

// Nearby printers that are not automatic & USB are not automatically
// installed.
TEST_F(CupsPrintersManagerTest, OtherNearbyPrintersNotInstalledAutomatically) {
  auto discovered_printer = MakeDiscoveredPrinter("Discovered");
  discovered_printer.printer.SetUri("usb://host/path");
  auto automatic_printer = MakeAutomaticPrinter("Automatic");

  usb_detector_->AddDetections({discovered_printer});
  zeroconf_detector_->AddDetections({automatic_printer});

  task_environment_.RunUntilIdle();

  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"Discovered"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"Automatic"});
  EXPECT_FALSE(printer_configurer_->IsConfigured("Discovered"));
  EXPECT_FALSE(printer_configurer_->IsConfigured("Automatic"));
}

TEST_F(CupsPrintersManagerTest, DetectedUsbPrinterConfigurationNotification) {
  auto discovered_printer = MakeDiscoveredPrinter("Discovered");
  discovered_printer.printer.SetUri("usb://host/path");

  usb_detector_->AddDetections({discovered_printer});
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(usb_notif_controller_->IsConfigurationNotification("Discovered"));

  usb_detector_->RemoveDetections({"Discovered"});

  EXPECT_FALSE(
      usb_notif_controller_->IsConfigurationNotification("Discovered"));
}

TEST_F(CupsPrintersManagerTest,
       DetectedZeroconfDiscoveredPrinterNoNotification) {
  auto discovered_printer = MakeDiscoveredPrinter("Discovered");
  discovered_printer.printer.SetUri("ipp://host");

  zeroconf_detector_->AddDetections({discovered_printer});
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(
      usb_notif_controller_->IsConfigurationNotification("Discovered"));
}

// Test that RecordNearbyNetworkPrinterCounts logs the total number of
// detected network printers.
TEST_F(CupsPrintersManagerTest, RecordTotalNetworkPrinterCounts) {
  base::HistogramTester histogram_tester;
  manager_->SavePrinter(Printer("DiscoveredNetworkPrinter0"));
  usb_detector_->AddDetections({MakeDiscoveredPrinter("DiscoveredUSBPrinter"),
                                MakeAutomaticPrinter("AutomaticUSBPrinter")});
  manager_->RecordNearbyNetworkPrinterCounts();
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectBucketCount("Printing.CUPS.TotalNetworkPrintersCount",
                                     0, 1);
  zeroconf_detector_->AddDetections(
      {MakeDiscoveredPrinter("DiscoveredNetworkPrinter0"),
       MakeDiscoveredPrinter("DiscoveredNetworkPrinter1"),
       MakeAutomaticPrinter("AutomaticNetworkPrinter0"),
       MakeAutomaticPrinter("AutomaticNetworkPrinter1")});
  manager_->RecordNearbyNetworkPrinterCounts();
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectBucketCount("Printing.CUPS.TotalNetworkPrintersCount",
                                     4, 1);
}

// Test that RecordNearbyNetworkPrinterCounts logs the number of
// all nearby (not already saved) detected network printers.
TEST_F(CupsPrintersManagerTest, RecordNearbyNetworkPrinterCounts) {
  base::HistogramTester histogram_tester;
  usb_detector_->AddDetections({MakeDiscoveredPrinter("DiscoveredUSBPrinter"),
                                MakeAutomaticPrinter("AutomaticUSBPrinter")});
  manager_->RecordNearbyNetworkPrinterCounts();
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectBucketCount("Printing.CUPS.NearbyNetworkPrintersCount",
                                     0, 1);
  manager_->SavePrinter(Printer("DiscoveredNetworkPrinter0"));
  zeroconf_detector_->AddDetections(
      {MakeDiscoveredPrinter("DiscoveredNetworkPrinter0"),
       MakeDiscoveredPrinter("DiscoveredNetworkPrinter1"),
       MakeAutomaticPrinter("AutomaticNetworkPrinter0"),
       MakeAutomaticPrinter("AutomaticNetworkPrinter1")});
  manager_->RecordNearbyNetworkPrinterCounts();
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectBucketCount("Printing.CUPS.NearbyNetworkPrintersCount",
                                     3, 1);

  // Save one more network printer.
  manager_->SavePrinter(Printer("AutomaticNetworkPrinter1"));
  manager_->RecordNearbyNetworkPrinterCounts();
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectBucketCount("Printing.CUPS.NearbyNetworkPrintersCount",
                                     2, 1);
}

TEST_F(CupsPrintersManagerTest, OnServerPrintersChanged) {
  auto server_printer = MakeAutomaticPrinter("ServerPrinter");
  server_printer.printer.mutable_ppd_reference()->autoconf = true;

  print_servers_manager_->ServerPrintersChanged({server_printer});

  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"ServerPrinter"});
}

// Tests that when the active network is switched to a different network the
// list of nearby printers is cleared.
TEST_F(CupsPrintersManagerTest, ActiveNetworkSwitched) {
  cros_network_config_helper_.network_state_helper().ConfigureService(
      R"({"GUID": "Wifi1", "Type": "wifi", "State": "online"})");

  zeroconf_detector_->AddDetections({MakeDiscoveredPrinter("DiscoveredPrinter"),
                                     MakeAutomaticPrinter("AutomaticPrinter")});

  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"DiscoveredPrinter"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"AutomaticPrinter"});

  cros_network_config_helper_.network_state_helper().ClearServices();
  cros_network_config_helper_.network_state_helper().ConfigureService(
      R"({"GUID": "Wifi2", "Type": "wifi", "State": "online"})");

  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {});
}

// Tests that when the active network is disconnected the list of nearby
// printers is cleared.
TEST_F(CupsPrintersManagerTest, ActiveNetworkDisconnected) {
  cros_network_config_helper_.network_state_helper().ConfigureService(
      R"({"GUID": "Wifi1", "Type": "wifi", "State": "online"})");

  zeroconf_detector_->AddDetections({MakeDiscoveredPrinter("DiscoveredPrinter"),
                                     MakeAutomaticPrinter("AutomaticPrinter")});

  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"DiscoveredPrinter"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"AutomaticPrinter"});

  cros_network_config_helper_.network_state_helper().ClearServices();

  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {});
}

// Tests that when the a new wifi network is detected, but the active network
// remains the same, the list of nearby printers stays the same.
TEST_F(CupsPrintersManagerTest, NewNetworkDetected) {
  cros_network_config_helper_.network_state_helper().ConfigureService(
      R"({"GUID": "Wifi1", "Type": "wifi", "State": "online"})");
  zeroconf_detector_->AddDetections({MakeDiscoveredPrinter("DiscoveredPrinter"),
                                     MakeAutomaticPrinter("AutomaticPrinter")});

  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"DiscoveredPrinter"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"AutomaticPrinter"});

  cros_network_config_helper_.network_state_helper().ConfigureService(
      R"({"GUID": "Wifi2", "Type": "wifi", "State": "online"})");

  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"DiscoveredPrinter"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"AutomaticPrinter"});
}

// Tests that when the signal strength of the active network changes, the list
// of nearby printers stays the same.
TEST_F(CupsPrintersManagerTest, ActiveNetworkStrengthChanged) {
  const std::string service_path =
      cros_network_config_helper_.network_state_helper().ConfigureService(
          R"({"GUID": "Wifi1", "Type": "wifi", "State": "online"})");
  zeroconf_detector_->AddDetections({MakeDiscoveredPrinter("DiscoveredPrinter"),
                                     MakeAutomaticPrinter("AutomaticPrinter")});

  task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"DiscoveredPrinter"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"AutomaticPrinter"});

  cros_network_config_helper_.network_state_helper().SetServiceProperty(
      service_path, shill::kSignalStrengthProperty, base::Value(50));

  ExpectPrintersInClassAre(PrinterClass::kDiscovered, {"DiscoveredPrinter"});
  ExpectPrintersInClassAre(PrinterClass::kAutomatic, {"AutomaticPrinter"});
}

}  // namespace
}  // namespace ash

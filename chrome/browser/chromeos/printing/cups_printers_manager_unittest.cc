// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/sequenced_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager.h"
#include "chrome/browser/chromeos/printing/usb_printer_detector.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

// Fake backend for SyncedPrintersManager.  This allows us to poke arbitrary
// changes in the configured and enterprise printer lists.
class FakeSyncedPrintersManager : public SyncedPrintersManager {
 public:
  FakeSyncedPrintersManager() = default;
  ~FakeSyncedPrintersManager() override = default;

  // Returns the printers that are saved in preferences.
  std::vector<Printer> GetConfiguredPrinters() const override {
    return configured_printers_;
  }

  // Returns printers from enterprise policy.
  std::vector<Printer> GetEnterprisePrinters() const override {
    return enterprise_printers_;
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

  void UpdateConfiguredPrinter(const Printer& printer) override {
    size_t i;
    for (i = 0; i < configured_printers_.size(); ++i) {
      if (configured_printers_[i].id() == printer.id()) {
        configured_printers_[i] = printer;
        break;
      }
    }
    if (i == configured_printers_.size()) {
      // Didn't find it, add it.
      configured_printers_.push_back(printer);
    }
    for (Observer& observer : observers_) {
      observer.OnConfiguredPrintersChanged(configured_printers_);
    }
  }

  bool RemoveConfiguredPrinter(const std::string& printer_id) override {
    for (auto it = configured_printers_.begin();
         it != configured_printers_.end(); ++it) {
      if (it->id() == printer_id) {
        configured_printers_.erase(it);
        for (Observer& observer : observers_) {
          observer.OnConfiguredPrintersChanged(configured_printers_);
        }
        return true;
      }
    }
    return false;
  }

  // Everything else in the interface we either don't use in
  // CupsPrintersManager, or just use in a simple pass-through manner that's not
  // worth additional layers of testing on top of the testing in
  // SyncedPrintersManager.
  void PrinterInstalled(const Printer& printer) override {}
  bool IsConfigurationCurrent(const Printer& printer) const override {
    return true;
  }
  PrintersSyncBridge* GetSyncBridge() override { return nullptr; }
  // Returns the printer with id |printer_id|, or nullptr if no such printer
  // exists.  Searches both Configured and Enterprise printers.
  std::unique_ptr<Printer> GetPrinter(
      const std::string& printer_id) const override {
    return nullptr;
  }

  // Fake manipulation functions.

  // Add the given printers to the list of configured printers and
  // notify observers.
  void AddConfiguredPrinters(const std::vector<Printer>& printers) {
    configured_printers_.insert(configured_printers_.end(), printers.begin(),
                                printers.end());
    for (Observer& observer : observers_) {
      observer.OnConfiguredPrintersChanged(configured_printers_);
    }
  }

  // Remove the printers with the given ids from the set of configured printers,
  // notify observers.
  void RemoveConfiguredPrinters(const std::unordered_set<std::string>& ids) {
    RemovePrinters(ids, &configured_printers_);
    for (Observer& observer : observers_) {
      observer.OnConfiguredPrintersChanged(configured_printers_);
    }
  }

  // Add the given printers to the list of enterprise printers and
  // notify observers.
  void AddEnterprisePrinters(const std::vector<Printer>& printers) {
    enterprise_printers_.insert(enterprise_printers_.end(), printers.begin(),
                                printers.end());
    for (Observer& observer : observers_) {
      observer.OnEnterprisePrintersChanged(enterprise_printers_);
    }
  }

  // Remove the printers with the given ids from the set of enterprise printers,
  // notify observers.
  void RemoveEnterprisePrinters(const std::unordered_set<std::string>& ids) {
    RemovePrinters(ids, &enterprise_printers_);
    for (Observer& observer : observers_) {
      observer.OnEnterprisePrintersChanged(enterprise_printers_);
    }
  }

 private:
  void RemovePrinters(const std::unordered_set<std::string>& ids,
                      std::vector<Printer>* target) {
    auto new_end = std::remove_if(target->begin(), target->end(),
                                  [&ids](const Printer& printer) -> bool {
                                    return base::ContainsKey(ids, printer.id());
                                  });

    target->resize(new_end - target->begin());
  }

  base::ObserverList<SyncedPrintersManager::Observer>::Unchecked observers_;
  std::vector<Printer> configured_printers_;
  std::vector<Printer> enterprise_printers_;
};

class FakePrinterDetector : public PrinterDetector {
 public:
  FakePrinterDetector() {}
  ~FakePrinterDetector() override = default;

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  std::vector<DetectedPrinter> GetPrinters() override { return detections_; }

  void AddDetections(
      const std::vector<PrinterDetector::DetectedPrinter>& new_detections) {
    detections_.insert(detections_.end(), new_detections.begin(),
                       new_detections.end());
    for (Observer& observer : observers_) {
      observer.OnPrintersFound(detections_);
    }
  }

  // Remove printers that have ids in ids.
  void RemoveDetections(const std::unordered_set<std::string>& ids) {
    auto new_end =
        std::remove_if(detections_.begin(), detections_.end(),
                       [&ids](const DetectedPrinter& detection) -> bool {
                         return base::ContainsKey(ids, detection.printer.id());
                       });

    detections_.resize(new_end - detections_.begin());
  }

 private:
  std::vector<DetectedPrinter> detections_;
  base::ObserverList<PrinterDetector::Observer>::Unchecked observers_;
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
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), PpdProvider::NOT_FOUND,
                                    Printer::PpdReference()));
    } else {
      Printer::PpdReference ret;
      ret.effective_make_and_model = search_data.make_and_model[0];
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), PpdProvider::SUCCESS, ret));
    }
  }

  // These three functions are not used by CupsPrintersManager.
  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {}
  void ResolveManufacturers(ResolveManufacturersCallback cb) override {}
  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {}
  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {}

 private:
  ~FakePpdProvider() override {}
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

class CupsPrintersManagerTest : public testing::Test,
                                public CupsPrintersManager::Observer {
 public:
  CupsPrintersManagerTest()
      : observed_printers_(CupsPrintersManager::kNumPrinterClasses),
        ppd_provider_(new FakePpdProvider) {
    // Zeroconf and usb detector ownerships are taken by the manager, so we have
    // to keep raw pointers to them.
    auto zeroconf_detector = std::make_unique<FakePrinterDetector>();
    zeroconf_detector_ = zeroconf_detector.get();
    auto usb_detector = std::make_unique<FakePrinterDetector>();
    usb_detector_ = usb_detector.get();

    // Register the pref |UserNativePrintersAllowed|
    CupsPrintersManager::RegisterProfilePrefs(pref_service_.registry());

    manager_ = CupsPrintersManager::CreateForTesting(
        &synced_printers_manager_, std::move(usb_detector),
        std::move(zeroconf_detector), ppd_provider_, &event_tracker_,
        &pref_service_);
    manager_->AddObserver(this);
  }

  ~CupsPrintersManagerTest() override {}

  // CupsPrintersManager::Observer implementation
  void OnPrintersChanged(CupsPrintersManager::PrinterClass printer_class,
                         const std::vector<Printer>& printers) override {
    observed_printers_.at(printer_class) = printers;
  }

  // Check that, for the given printer class, the printers we have from the
  // observation callback and the printers we have when we query the manager are
  // both the same and have the passed ids.
  void ExpectPrintersInClassAre(CupsPrintersManager::PrinterClass printer_class,
                                const std::vector<std::string>& ids) {
    ExpectPrinterIdsAre(manager_->GetPrinters(printer_class), ids);
    ExpectPrinterIdsAre(observed_printers_.at(printer_class), ids);
  }

  void UpdatePolicyValue(const char* name, bool value) {
    auto value_ptr = std::make_unique<base::Value>(value);
    // TestingPrefSyncableService assumes ownership of |value_ptr|.
    pref_service_.SetManagedPref(name, std::move(value_ptr));
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // Captured printer lists from observer callbacks.
  std::vector<std::vector<Printer>> observed_printers_;

  // Backend fakes driving the CupsPrintersManager.
  FakeSyncedPrintersManager synced_printers_manager_;
  FakePrinterDetector* usb_detector_;
  FakePrinterDetector* zeroconf_detector_;
  scoped_refptr<FakePpdProvider> ppd_provider_;

  // This is unused, it's just here for memory ownership.
  PrinterEventTracker event_tracker_;

  // PrefService used to register the |UserNativePrintersAllowed| pref and
  // change its value for testing.
  sync_preferences::TestingPrefServiceSyncable pref_service_;

  // The manager being tested.  This must be declared after the fakes, as its
  // initialization must come after that of the fakes.
  std::unique_ptr<CupsPrintersManager> manager_;
};

// Pseudo-constructor for inline creation of a DetectedPrinter that should (in
// this test) be handled as a Discovered printer (because it has no make and
// model information, and that's now the FakePpdProvider is set up to determine
// whether or not something has a Ppd available).
PrinterDetector::DetectedPrinter MakeDiscoveredPrinter(const std::string& id) {
  PrinterDetector::DetectedPrinter ret;
  ret.printer.set_id(id);
  return ret;
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
  synced_printers_manager_.AddEnterprisePrinters(
      {Printer("Foo"), Printer("Bar")});
  scoped_task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(CupsPrintersManager::kEnterprise, {"Foo", "Bar"});
}

// Test that Configured printers from SyncedPrinterManager are
// surfaced appropriately.
TEST_F(CupsPrintersManagerTest, GetConfiguredPrinters) {
  synced_printers_manager_.AddConfiguredPrinters(
      {Printer("Foo"), Printer("Bar")});
  scoped_task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(CupsPrintersManager::kConfigured, {"Foo", "Bar"});
}

// Test that USB printers from the usb detector are converted to 'Printer's and
// surfaced appropriately.  One printer should be "automatic" because it has
// a findable Ppd, the other should be "discovered".
TEST_F(CupsPrintersManagerTest, GetUsbPrinters) {
  usb_detector_->AddDetections({MakeDiscoveredPrinter("DiscoveredPrinter"),
                                MakeAutomaticPrinter("AutomaticPrinter")});
  scoped_task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(CupsPrintersManager::kDiscovered,
                           {"DiscoveredPrinter"});
  ExpectPrintersInClassAre(CupsPrintersManager::kAutomatic,
                           {"AutomaticPrinter"});
}

// Same as GetUsbPrinters, only for Zeroconf printers.
TEST_F(CupsPrintersManagerTest, GetZeroconfPrinters) {
  zeroconf_detector_->AddDetections({MakeDiscoveredPrinter("DiscoveredPrinter"),
                                     MakeAutomaticPrinter("AutomaticPrinter")});
  synced_printers_manager_.AddConfiguredPrinters(
      {Printer("Foo"), Printer("Bar")});

  scoped_task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(CupsPrintersManager::kDiscovered,
                           {"DiscoveredPrinter"});
  ExpectPrintersInClassAre(CupsPrintersManager::kAutomatic,
                           {"AutomaticPrinter"});
}

// Test that printers that appear in either a Configured or Enterprise set do
// *not* appear in Discovered or Automatic, even if they are detected as such.
TEST_F(CupsPrintersManagerTest, SyncedPrintersTrumpDetections) {
  zeroconf_detector_->AddDetections(
      {MakeDiscoveredPrinter("DiscoveredPrinter0"),
       MakeDiscoveredPrinter("DiscoveredPrinter1"),
       MakeAutomaticPrinter("AutomaticPrinter0"),
       MakeAutomaticPrinter("AutomaticPrinter1")});
  scoped_task_environment_.RunUntilIdle();
  // Before we muck with anything else, check that automatic and discovered
  // classes are what we intended to set up.
  ExpectPrintersInClassAre(CupsPrintersManager::kDiscovered,
                           {"DiscoveredPrinter0", "DiscoveredPrinter1"});
  ExpectPrintersInClassAre(CupsPrintersManager::kAutomatic,
                           {"AutomaticPrinter0", "AutomaticPrinter1"});

  // Configure both the Discovered and Automatic printers.  This should put them
  // into the Configured class and thus *remove* them from their previous
  // classes.
  manager_->UpdateConfiguredPrinter(Printer("DiscoveredPrinter0"));
  manager_->UpdateConfiguredPrinter(Printer("AutomaticPrinter0"));
  scoped_task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(CupsPrintersManager::kDiscovered,
                           {"DiscoveredPrinter1"});
  ExpectPrintersInClassAre(CupsPrintersManager::kAutomatic,
                           {"AutomaticPrinter1"});
  ExpectPrintersInClassAre(CupsPrintersManager::kConfigured,
                           {"DiscoveredPrinter0", "AutomaticPrinter0"});
}

// Test updates of configured printers.  Updates of existing configured printers
// should propagate.  Updates of printers in other classes should result in
// those printers becoming configured.  Updates of unknown printers should
// result in a new configured printer.
TEST_F(CupsPrintersManagerTest, UpdateConfiguredPrinter) {
  // Start with a printer in each class named after the class it's in, except
  // Enterprise which is not relevant to this test.
  Printer existing_configured("Configured");
  synced_printers_manager_.AddConfiguredPrinters({existing_configured});
  usb_detector_->AddDetections({MakeDiscoveredPrinter("Discovered")});
  zeroconf_detector_->AddDetections({MakeAutomaticPrinter("Automatic")});
  scoped_task_environment_.RunUntilIdle();

  // Sanity check that we do, indeed, have one printer in each class.
  ExpectPrintersInClassAre(CupsPrintersManager::kConfigured, {"Configured"});
  ExpectPrintersInClassAre(CupsPrintersManager::kAutomatic, {"Automatic"});
  ExpectPrintersInClassAre(CupsPrintersManager::kDiscovered, {"Discovered"});

  // Update the existing configured printer.  Check that the new display name
  // propagated.
  existing_configured.set_display_name("New Display Name");
  manager_->UpdateConfiguredPrinter(existing_configured);
  scoped_task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(CupsPrintersManager::kConfigured, {"Configured"});
  EXPECT_EQ(
      manager_->GetPrinters(CupsPrintersManager::kConfigured)[0].display_name(),
      "New Display Name");

  // Do the same thing for the Automatic and Discovered printers.
  // Create a configuration for the zeroconf printer, which should shift it
  // into the configured category.
  manager_->UpdateConfiguredPrinter(Printer("Automatic"));
  scoped_task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(CupsPrintersManager::kAutomatic, {});
  ExpectPrintersInClassAre(CupsPrintersManager::kConfigured,
                           {"Automatic", "Configured"});

  manager_->UpdateConfiguredPrinter(Printer("Discovered"));
  scoped_task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(CupsPrintersManager::kDiscovered, {});
  ExpectPrintersInClassAre(CupsPrintersManager::kConfigured,
                           {"Automatic", "Configured", "Discovered"});

  // Update a printer we haven't seen before, which should just add it.
  manager_->UpdateConfiguredPrinter(Printer("NewFangled"));
  scoped_task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(
      CupsPrintersManager::kConfigured,
      {"Automatic", "Configured", "Discovered", "NewFangled"});

  // Remove the automatic printer, make sure it ends up back in the automatic
  // class after removal.
  manager_->RemoveConfiguredPrinter("Automatic");
  scoped_task_environment_.RunUntilIdle();
  ExpectPrintersInClassAre(CupsPrintersManager::kConfigured,
                           {"Configured", "Discovered", "NewFangled"});
  ExpectPrintersInClassAre(CupsPrintersManager::kAutomatic, {"Automatic"});
}

// Test that GetPrinter() finds printers in any class, and returns null if
// a printer is not found.
TEST_F(CupsPrintersManagerTest, GetPrinter) {
  synced_printers_manager_.AddConfiguredPrinters({Printer("Configured")});
  synced_printers_manager_.AddEnterprisePrinters({Printer("Enterprise")});
  usb_detector_->AddDetections({MakeDiscoveredPrinter("Discovered")});
  zeroconf_detector_->AddDetections({MakeAutomaticPrinter("Automatic")});
  scoped_task_environment_.RunUntilIdle();

  for (const std::string& id :
       {"Configured", "Enterprise", "Discovered", "Automatic"}) {
    std::unique_ptr<Printer> printer = manager_->GetPrinter(id);
    ASSERT_NE(printer, nullptr);
    EXPECT_EQ(printer->id(), id);
  }

  std::unique_ptr<Printer> printer = manager_->GetPrinter("Nope");
  EXPECT_EQ(printer, nullptr);
}

// Test that if |UserNativePrintersAllowed| pref is set to false, then
// GetPrinters() will only return printers from
// |CupsPrintersManager::kEnterprise|.
TEST_F(CupsPrintersManagerTest, GetPrintersUserNativePrintersDisabled) {
  synced_printers_manager_.AddConfiguredPrinters({Printer("Configured")});
  synced_printers_manager_.AddEnterprisePrinters({Printer("Enterprise")});
  scoped_task_environment_.RunUntilIdle();

  // Disable the use of non-enterprise printers.
  UpdatePolicyValue(prefs::kUserNativePrintersAllowed, false);

  // Verify that non-enterprise printers are not returned by GetPrinters()
  std::vector<Printer> configured_printers =
      manager_->GetPrinters(CupsPrintersManager::kConfigured);
  ExpectPrinterIdsAre(configured_printers, {});

  // Verify that enterprise printers are returned by GetPrinters()
  std::vector<Printer> enterprise_printers =
      manager_->GetPrinters(CupsPrintersManager::kEnterprise);
  ExpectPrinterIdsAre(enterprise_printers, {"Enterprise"});
}

// Test that if |UserNativePrintersAllowed| pref is set to false, then
// UpdateConfiguredPrinter() will simply do nothing.
TEST_F(CupsPrintersManagerTest,
       UpdateConfiguredPrinterUserNativePrintersDisabled) {
  // Start by installing a configured printer to be used to test than any
  // changes made to the printer will not be propogated.
  Printer existing_configured("Configured");
  synced_printers_manager_.AddConfiguredPrinters({existing_configured});
  usb_detector_->AddDetections({MakeDiscoveredPrinter("Discovered")});
  zeroconf_detector_->AddDetections({MakeAutomaticPrinter("Automatic")});
  scoped_task_environment_.RunUntilIdle();

  // Sanity check that we do, indeed, have one printer in each class.
  ExpectPrintersInClassAre(CupsPrintersManager::kConfigured, {"Configured"});
  ExpectPrintersInClassAre(CupsPrintersManager::kAutomatic, {"Automatic"});
  ExpectPrintersInClassAre(CupsPrintersManager::kDiscovered, {"Discovered"});

  // Disable the use of non-enterprise printers.
  UpdatePolicyValue(prefs::kUserNativePrintersAllowed, false);

  // Update the existing configured printer. Verify that the changes did not
  // progogate.
  existing_configured.set_display_name("New Display Name");
  manager_->UpdateConfiguredPrinter(existing_configured);
  scoped_task_environment_.RunUntilIdle();

  // Reenable user printers in order to do checking.
  UpdatePolicyValue(prefs::kUserNativePrintersAllowed, true);
  ExpectPrintersInClassAre(CupsPrintersManager::kConfigured, {"Configured"});
  EXPECT_EQ(
      manager_->GetPrinters(CupsPrintersManager::kConfigured)[0].display_name(),
      "");
  UpdatePolicyValue(prefs::kUserNativePrintersAllowed, false);

  // Attempt to update the Automatic and Discovered printers. In both cases
  // check that the printers do not move into the configured category.
  manager_->UpdateConfiguredPrinter(Printer("Automatic"));
  scoped_task_environment_.RunUntilIdle();
  UpdatePolicyValue(prefs::kUserNativePrintersAllowed, true);
  ExpectPrintersInClassAre(CupsPrintersManager::kAutomatic, {"Automatic"});
  ExpectPrintersInClassAre(CupsPrintersManager::kConfigured, {"Configured"});
  UpdatePolicyValue(prefs::kUserNativePrintersAllowed, false);

  manager_->UpdateConfiguredPrinter(Printer("Discovered"));
  scoped_task_environment_.RunUntilIdle();
  UpdatePolicyValue(prefs::kUserNativePrintersAllowed, true);
  ExpectPrintersInClassAre(CupsPrintersManager::kDiscovered, {"Discovered"});
  ExpectPrintersInClassAre(CupsPrintersManager::kConfigured, {"Configured"});
  UpdatePolicyValue(prefs::kUserNativePrintersAllowed, false);

  // Attempt to update a printer that we haven't seen before, check that nothing
  // changed.
  manager_->UpdateConfiguredPrinter(Printer("NewFangled"));
  scoped_task_environment_.RunUntilIdle();
  UpdatePolicyValue(prefs::kUserNativePrintersAllowed, true);
  ExpectPrintersInClassAre(CupsPrintersManager::kConfigured, {"Configured"});
}

// Test that if |UserNativePrintersAllowed| pref is set to false GetPrinter only
// returns a printer when the given printer id corresponds to an enterprise
// printer. Otherwise, it returns nothing.
TEST_F(CupsPrintersManagerTest, GetPrinterUserNativePrintersDisabled) {
  synced_printers_manager_.AddConfiguredPrinters({Printer("Configured")});
  synced_printers_manager_.AddEnterprisePrinters({Printer("Enterprise")});
  scoped_task_environment_.RunUntilIdle();

  // Sanity check that the printers were added.
  ExpectPrintersInClassAre(CupsPrintersManager::kConfigured, {"Configured"});
  ExpectPrintersInClassAre(CupsPrintersManager::kEnterprise, {"Enterprise"});

  // Diable the use of non-enterprise printers.
  UpdatePolicyValue(prefs::kUserNativePrintersAllowed, false);

  std::unique_ptr<Printer> configured_ptr = manager_->GetPrinter("Configured");
  EXPECT_EQ(configured_ptr, nullptr);

  std::unique_ptr<Printer> enterprise_ptr = manager_->GetPrinter("Enterprise");
  ASSERT_NE(enterprise_ptr, nullptr);
  EXPECT_EQ(enterprise_ptr->id(), "Enterprise");
}

}  // namespace
}  // namespace chromeos

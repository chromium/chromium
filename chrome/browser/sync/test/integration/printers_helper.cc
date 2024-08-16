// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/printers_helper.h"

#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "content/public/test/test_utils.h"

using sync_datatype_helper::test;

namespace printers_helper {

namespace {

using PrinterList = std::vector<chromeos::Printer>;

// Returns true if Printer#id, Printer#description, and Printer#uri all match.
bool PrintersAreMostlyEqual(const chromeos::Printer& left,
                            const chromeos::Printer& right) {
  return left.id() == right.id() && left.description() == right.description() &&
         left.uri() == right.uri();
}

// Returns true if both lists have the same elements irrespective of order.
bool ListsContainTheSamePrinters(const PrinterList& list_a,
                                 const PrinterList& list_b) {
  std::unordered_multimap<std::string, const chromeos::Printer*> map_b;
  for (const chromeos::Printer& b : list_b) {
    map_b.insert({b.id(), &b});
  }

  for (const chromeos::Printer& a : list_a) {
    auto [begin, end] = map_b.equal_range(a.id());

    auto it = std::find_if(
        begin, end,
        [&a](const std::pair<std::string, const chromeos::Printer*>& entry) {
          return PrintersAreMostlyEqual(a, *(entry.second));
        });

    if (it == end) {
      // Element in a does not match an element in b. Lists do not contain the
      // same elements.
      return false;
    }

    map_b.erase(it);
  }

  return map_b.empty();
}

std::string PrinterId(int index) {
  return base::StringPrintf("printer%d", index);
}

ash::SyncedPrintersManager* GetPrinterStore(content::BrowserContext* context) {
  return ash::SyncedPrintersManagerFactory::GetForBrowserContext(context);
}

}  // namespace

void AddPrinter(ash::SyncedPrintersManager* manager,
                const chromeos::Printer& printer) {
  manager->UpdateSavedPrinter(printer);
}

void RemovePrinter(ash::SyncedPrintersManager* manager, int index) {
  chromeos::Printer testPrinter(CreateTestPrinter(index));
  manager->RemoveSavedPrinter(testPrinter.id());
}

bool EditPrinterDescription(ash::SyncedPrintersManager* manager,
                            int index,
                            const std::string& description) {
  PrinterList printers = manager->GetSavedPrinters();
  std::string printer_id = PrinterId(index);
  auto found = base::ranges::find(printers, printer_id, &chromeos::Printer::id);

  if (found == printers.end()) {
    return false;
  }

  found->set_description(description);
  manager->UpdateSavedPrinter(*found);

  return true;
}

chromeos::Printer CreateTestPrinter(int index) {
  chromeos::Printer printer(PrinterId(index));
  printer.set_description("Description");
  printer.SetUri(base::StringPrintf("ipp://192.168.1.%d", index));

  return printer;
}

std::unique_ptr<sync_pb::PrinterSpecifics> CreateTestPrinterSpecifics(
    int index) {
  auto specifics = std::make_unique<sync_pb::PrinterSpecifics>();
  specifics->set_id(PrinterId(index));
  specifics->set_description("Description");
  specifics->set_uri(base::StringPrintf("ipp://192.168.1.%d", index));

  return specifics;
}

void WaitForPrinterStoreToLoad(content::BrowserContext* context) {
  GetPrinterStore(context);
  // Run tasks to allow a DataTypeStore to be associated with the
  // SyncedPrinterManager.
  //
  // TODO(sync): Remove this forced initialization once there is a mechanism
  // to queue writes/reads before the DataTypeStore is associated with the
  // SyncedPrinterManager. https://crbug.com/709094.
  content::RunAllTasksUntilIdle();
}

ash::SyncedPrintersManager* GetVerifierPrinterStore() {
  ash::SyncedPrintersManager* manager =
      GetPrinterStore(sync_datatype_helper::test()->verifier());

  return manager;
}

ash::SyncedPrintersManager* GetPrinterStore(int index) {
  ash::SyncedPrintersManager* manager =
      GetPrinterStore(sync_datatype_helper::test()->GetProfile(index));

  return manager;
}

int GetVerifierPrinterCount() {
  return GetVerifierPrinterStore()->GetSavedPrinters().size();
}

int GetPrinterCount(int index) {
  return GetPrinterStore(index)->GetSavedPrinters().size();
}

bool AllProfilesContainSamePrinters(std::ostream* os) {
  std::vector<chromeos::Printer> reference_printers =
      GetPrinterStore(0)->GetSavedPrinters();
  for (int i = 1; i < test()->num_clients(); ++i) {
    std::vector<chromeos::Printer> printers =
        GetPrinterStore(i)->GetSavedPrinters();
    if (!ListsContainTheSamePrinters(reference_printers, printers)) {
      if (os) {
        *os << "Printers in client [" << i << "] don't match client 0";
      }
      return false;
    }
  }

  return true;
}

bool ProfileContainsSamePrintersAsVerifier(int index) {
  return ListsContainTheSamePrinters(
      GetVerifierPrinterStore()->GetSavedPrinters(),
      GetPrinterStore(index)->GetSavedPrinters());
}

PrintersMatchChecker::PrintersMatchChecker()
    : AwaitMatchStatusChangeChecker(base::BindRepeating(
          &printers_helper::AllProfilesContainSamePrinters)) {}

PrintersMatchChecker::~PrintersMatchChecker() = default;

}  // namespace printers_helper

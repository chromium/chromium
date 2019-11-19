// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/printers_helper.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
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
  for (const auto& b : list_b) {
    map_b.insert({b.id(), &b});
  }

  for (const auto& a : list_a) {
    auto range = map_b.equal_range(a.id());

    auto it = std::find_if(
        range.first, range.second,
        [&a](const std::pair<std::string, const chromeos::Printer*>& entry)
            -> bool { return PrintersAreMostlyEqual(a, *(entry.second)); });

    if (it == range.second) {
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

chromeos::SyncedPrintersManager* GetPrinterStore(
    content::BrowserContext* context) {
  return chromeos::SyncedPrintersManagerFactory::GetForBrowserContext(context);
}

}  // namespace

void AddPrinter(chromeos::SyncedPrintersManager* manager,
                const chromeos::Printer& printer) {
  manager->UpdateSavedPrinter(printer);
}

void RemovePrinter(chromeos::SyncedPrintersManager* manager, int index) {
  chromeos::Printer testPrinter(CreateTestPrinter(index));
  manager->RemoveSavedPrinter(testPrinter.id());
}

bool EditPrinterDescription(chromeos::SyncedPrintersManager* manager,
                            int index,
                            const std::string& description) {
  PrinterList printers = manager->GetSavedPrinters();
  std::string printer_id = PrinterId(index);
  auto found =
      std::find_if(printers.begin(), printers.end(),
                   [&printer_id](const chromeos::Printer& printer) -> bool {
                     return printer.id() == printer_id;
                   });

  if (found == printers.end())
    return false;

  found->set_description(description);
  manager->UpdateSavedPrinter(*found);

  return true;
}

chromeos::Printer CreateTestPrinter(int index) {
  chromeos::Printer printer(PrinterId(index));
  printer.set_description("Description");
  printer.set_uri(base::StringPrintf("ipp://192.168.1.%d", index));

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
  // Run tasks to allow a ModelTypeStore to be associated with the
  // SyncedPrinterManager.
  //
  // TODO(sync): Remove this forced initialization once there is a mechanism
  // to queue writes/reads before the ModelTypeStore is associated with the
  // SyncedPrinterManager. https://crbug.com/709094.
  content::RunAllTasksUntilIdle();
}

chromeos::SyncedPrintersManager* GetVerifierPrinterStore() {
  chromeos::SyncedPrintersManager* manager =
      GetPrinterStore(sync_datatype_helper::test()->verifier());

  return manager;
}

chromeos::SyncedPrintersManager* GetPrinterStore(int index) {
  chromeos::SyncedPrintersManager* manager =
      GetPrinterStore(sync_datatype_helper::test()->GetProfile(index));

  return manager;
}

int GetVerifierPrinterCount() {
  return GetVerifierPrinterStore()->GetSavedPrinters().size();
}

int GetPrinterCount(int index) {
  return GetPrinterStore(index)->GetSavedPrinters().size();
}

bool AllProfilesContainSamePrinters() {
  auto reference_printers = GetPrinterStore(0)->GetSavedPrinters();
  for (int i = 1; i < test()->num_clients(); ++i) {
    auto printers = GetPrinterStore(i)->GetSavedPrinters();
    if (!ListsContainTheSamePrinters(reference_printers, printers)) {
      VLOG(1) << "Printers in client [" << i << "] don't match client 0";
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
    : AwaitMatchStatusChangeChecker(
          base::Bind(&printers_helper::AllProfilesContainSamePrinters),
          "All printers match") {}

PrintersMatchChecker::~PrintersMatchChecker() {}

}  // namespace printers_helper

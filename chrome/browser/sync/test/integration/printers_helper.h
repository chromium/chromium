// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PRINTERS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PRINTERS_HELPER_H_

#include <memory>
#include <string>

#include "chrome/browser/chromeos/printing/synced_printers_manager.h"
#include "chrome/browser/sync/test/integration/await_match_status_change_checker.h"
#include "chromeos/printing/printer_configuration.h"

namespace content {
class BrowserContext;
}

namespace sync_pb {
class PrinterSpecifics;
}

namespace printers_helper {

// Create a test printer.
chromeos::Printer CreateTestPrinter(int index);

// Create a test printer, as PrinterSpecifics.
std::unique_ptr<sync_pb::PrinterSpecifics> CreateTestPrinterSpecifics(
    int index);

// Add printer to the supplied store.
void AddPrinter(chromeos::SyncedPrintersManager* manager,
                const chromeos::Printer& printer);

// Remove printer |index| from the |manager|.
void RemovePrinter(chromeos::SyncedPrintersManager* manager, int index);

// Change the description of the printer at |index| with |description|.  Returns
// false if the printer is not tracked by the manager.
bool EditPrinterDescription(chromeos::SyncedPrintersManager* manager,
                            int index,
                            const std::string& description);

// Waits for the printer store associated with |context| to load.
void WaitForPrinterStoreToLoad(content::BrowserContext* context);

// Returns the verifier store.
chromeos::SyncedPrintersManager* GetVerifierPrinterStore();

// Returns printer store at |index|.
chromeos::SyncedPrintersManager* GetPrinterStore(int index);

// Returns the number of printers in the verifier store.
int GetVerifierPrinterCount();

// Returns the number of printers in printer store |index|.
int GetPrinterCount(int index);

// Returns true if all profiles contain the same printers as profile 0.
bool AllProfilesContainSamePrinters();

// Returns true if the verifier store and printer store |index| contain the same
// data.
bool ProfileContainsSamePrintersAsVerifier(int index);

// A waiter that can block until we can satisfy
// AllProfilesContainSamePrinters().
// Example:
//     // Make changes that you expect to sync.
//
//     ASSERT_TRUE(PrintersMatchChecker().Wait());
//
//     // Sync is complete, verify the chagnes.
class PrintersMatchChecker : public AwaitMatchStatusChangeChecker {
 public:
  PrintersMatchChecker();
  ~PrintersMatchChecker() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintersMatchChecker);
};

}  // namespace printers_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PRINTERS_HELPER_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PRINTERS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PRINTERS_HELPER_H_

#include <iosfwd>
#include <memory>
#include <string>

#include "chrome/browser/ash/printing/synced_printers_manager.h"
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
void AddPrinter(ash::SyncedPrintersManager* manager,
                const chromeos::Printer& printer);

// Remove printer |index| from the |manager|.
void RemovePrinter(ash::SyncedPrintersManager* manager, int index);

// Change the description of the printer at |index| with |description|.  Returns
// false if the printer is not tracked by the manager.
bool EditPrinterDescription(ash::SyncedPrintersManager* manager,
                            int index,
                            const std::string& description);

// Waits for the printer store associated with |context| to load.
void WaitForPrinterStoreToLoad(content::BrowserContext* context);

// Returns the verifier store.
ash::SyncedPrintersManager* GetVerifierPrinterStore();

// Returns printer store at |index|.
ash::SyncedPrintersManager* GetPrinterStore(int index);

// Returns the number of printers in the verifier store.
int GetVerifierPrinterCount();

// Returns the number of printers in printer store |index|.
int GetPrinterCount(int index);

// Returns true if all profiles contain the same printers as profile 0.
bool AllProfilesContainSamePrinters(std::ostream* os = nullptr);

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

  PrintersMatchChecker(const PrintersMatchChecker&) = delete;
  PrintersMatchChecker& operator=(const PrintersMatchChecker&) = delete;

  ~PrintersMatchChecker() override;
};

}  // namespace printers_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PRINTERS_HELPER_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PRINTERS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PRINTERS_HELPER_H_

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/sync/protocol/printer_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace syncer {
class SyncServiceImpl;
}  // namespace syncer

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

// Returns printer store at |index|.
ash::SyncedPrintersManager* GetPrinterStore(int index);

// Returns the number of printers in printer store |index|.
int GetPrinterCount(int index);

// Returns true if all `profiles` contain the same printers as `profiles[0]`.
bool AllProfilesContainSamePrinters(
    const std::vector<raw_ptr<Profile>>& profiles,
    std::ostream* os = nullptr);

// A waiter that can block until `AllProfilesContainSamePrinters()` is
// satisfied.
// Example:
//     // Make changes that you expect to sync.
//
//     ASSERT_TRUE(
//         PrintersMatchChecker(GetSyncServices(), GetAllProfiles()).Wait());
//
//     // Sync is complete, verify the changes.
class PrintersMatchChecker : public MultiClientStatusChangeChecker {
 public:
  PrintersMatchChecker(std::vector<raw_ptr<syncer::SyncServiceImpl>> services,
                       std::vector<raw_ptr<Profile>> profiles);

  PrintersMatchChecker(const PrintersMatchChecker&) = delete;
  PrintersMatchChecker& operator=(const PrintersMatchChecker&) = delete;

  ~PrintersMatchChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const std::vector<raw_ptr<Profile>> profiles_;
};

// A waiter that blocks until the fake server has printer specifics matching
// `matcher`.
class ServerPrinterMatchChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher = testing::Matcher<std::vector<sync_pb::PrinterSpecifics>>;

  explicit ServerPrinterMatchChecker(const Matcher& matcher);
  ~ServerPrinterMatchChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // FakeServer::Observer implementation.
  void OnCommit(std::string_view committer_cache_guid,
                syncer::DataTypeSet committed_data_types) override;

 private:
  const Matcher matcher_;
};

// Matcher to check if sync_pb::PrinterSpecifics matches a chromeos::Printer.
MATCHER_P(MatchesPrinter, expected_printer, "") {
  return arg.id() == expected_printer.id() &&
         arg.description() == expected_printer.description() &&
         arg.uri() == expected_printer.uri().GetNormalized();
}

}  // namespace printers_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PRINTERS_HELPER_H_

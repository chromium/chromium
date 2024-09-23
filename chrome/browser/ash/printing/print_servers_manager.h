// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_PRINTING_PRINT_SERVERS_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINT_SERVERS_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ash/printing/enterprise/print_servers_policy_provider.h"
#include "chrome/browser/ash/printing/print_server.h"
#include "chrome/browser/ash/printing/printer_detector.h"
#include "chrome/browser/ash/printing/printer_installation_manager.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/uri.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {

class PrinterDetector;
class ServerPrintersProvider;

struct PrintServersConfig {
  PrintServersConfig();
  ~PrintServersConfig();
  PrintServersConfig(const PrintServersConfig&);
  PrintServersConfig& operator=(const PrintServersConfig&);

  ServerPrintersFetchingMode fetching_mode;
  std::vector<PrintServer> print_servers;
};

// Manager of IPP print servers in ChromeOS.
class PrintServersManager {
 public:
  class Observer {
   public:
    virtual void OnPrintServersChanged(const PrintServersConfig& config) {}
    virtual void OnServerPrintersChanged(
        const std::vector<PrinterDetector::DetectedPrinter>& printers) {}

    virtual ~Observer() = default;
  };

  // Factory function.
  static std::unique_ptr<PrintServersManager> Create(Profile* profile);

  // Factory function that allows injected dependencies, for testing.
  static std::unique_ptr<PrintServersManager> CreateForTesting(
      std::unique_ptr<ServerPrintersProvider> server_printers_provider,
      std::unique_ptr<PrintServersPolicyProvider> print_servers_provider);

  // Register the profile printing preferences with the |registry|.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Register the printing preferences with the |registry|.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  virtual ~PrintServersManager() = default;

  // Add or remove observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Selects print servers from all the available print servers. Returns true
  // on successfully selecting the requested print server.
  virtual void ChoosePrintServer(
      const std::vector<std::string>& selected_print_server_ids) = 0;

  // Gets all the print servers available through device or user policy and the
  // current fetching mode strategy for print servers.
  virtual PrintServersConfig GetPrintServersConfig() const = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINT_SERVERS_MANAGER_H_

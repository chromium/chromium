// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/print_servers_manager.h"

#include <map>
#include <optional>
#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/printing/cups_printer_status_creator.h"
#include "chrome/browser/ash/printing/enterprise/print_servers_policy_provider.h"
#include "chrome/browser/ash/printing/enterprise/print_servers_provider.h"
#include "chrome/browser/ash/printing/ppd_provider_factory.h"
#include "chrome/browser/ash/printing/ppd_resolution_tracker.h"
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "chrome/browser/ash/printing/printer_event_tracker.h"
#include "chrome/browser/ash/printing/printer_event_tracker_factory.h"
#include "chrome/browser/ash/printing/printer_info.h"
#include "chrome/browser/ash/printing/printers_map.h"
#include "chrome/browser/ash/printing/server_printers_provider.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/printing/usb_printer_notification_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printing_constants.h"
#include "chromeos/printing/uri.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/printer_query_result.h"

namespace ash {

PrintServersConfig::PrintServersConfig() = default;
PrintServersConfig::~PrintServersConfig() = default;
PrintServersConfig::PrintServersConfig(const PrintServersConfig&) = default;
PrintServersConfig& PrintServersConfig::operator=(const PrintServersConfig&) =
    default;

namespace {

class PrintServersManagerImpl : public PrintServersManager {
 public:
  PrintServersManagerImpl(
      std::unique_ptr<PrintServersPolicyProvider> print_servers_provider,
      std::unique_ptr<ServerPrintersProvider> server_printers_provider)
      : print_servers_provider_(std::move(print_servers_provider)),
        server_printers_provider_(std::move(server_printers_provider)) {
    print_servers_provider_->SetListener(
        base::BindRepeating(&PrintServersManagerImpl::OnPrintServersUpdated,
                            weak_ptr_factory_.GetWeakPtr()));
    server_printers_provider_->RegisterPrintersFoundCallback(
        base::BindRepeating(&PrintServersManagerImpl::OnPrintersUpdated,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  ~PrintServersManagerImpl() override = default;

  // Public API function.
  void AddObserver(PrintServersManager::Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  // Public API function.
  void RemoveObserver(PrintServersManager::Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  // Callback for ServerPrintersProvider.
  void OnPrintersUpdated(bool complete) {
    const std::vector<PrinterDetector::DetectedPrinter> printers =
        server_printers_provider_->GetPrinters();
    if (complete) {
      PRINTER_LOG(EVENT) << "The list of server printers has been completed. "
                         << "Number of server printers: " << printers.size();
    }
    for (auto& observer : observer_list_) {
      observer.OnServerPrintersChanged(printers);
    }
  }

  void OnPrintServersUpdated(bool is_complete,
                             std::map<GURL, PrintServer> print_servers,
                             ServerPrintersFetchingMode fetching_mode) {
    fetching_mode_ = fetching_mode;
    if (!is_complete) {
      return;
    }
    // Create an entry in the device log.
    if (is_complete) {
      PRINTER_LOG(EVENT) << "The list of print servers has been completed. "
                         << "Number of print servers: " << print_servers.size();
      if (!print_servers.empty()) {
        base::UmaHistogramCounts1000("Printing.PrintServers.ServersToQuery",
                                     print_servers.size());
      }
    }

    print_servers_ = std::map<std::string, PrintServer>();
    std::vector<PrintServer> print_servers_list;
    for (const auto& server_pair : print_servers) {
      const PrintServer& server = server_pair.second;
      print_servers_.value().emplace(server.GetId(), server);
      print_servers_list.push_back(server);
    }

    if (fetching_mode_ == ServerPrintersFetchingMode::kSingleServerOnly) {
      ChoosePrintServer(selected_print_server_ids_);
    } else {
      selected_print_server_ids_.clear();
      server_printers_provider_->OnServersChanged(true, print_servers);
    }

    config_ = PrintServersConfig();
    config_.print_servers = print_servers_list;
    config_.fetching_mode = fetching_mode;
    for (auto& observer : observer_list_) {
      observer.OnPrintServersChanged(config_);
    }
  }

  // Public API function.
  void ChoosePrintServer(
      const std::vector<std::string>& selected_print_server_ids) override {
    if (fetching_mode_ != ServerPrintersFetchingMode::kSingleServerOnly ||
        !print_servers_.has_value()) {
      return;
    }

    std::map<GURL, PrintServer> selected_print_servers;
    std::vector<std::string> selected_ids;
    for (auto selected_print_server_id : selected_print_server_ids) {
      auto iter = print_servers_.value().find(selected_print_server_id);
      if (iter != print_servers_.value().end()) {
        const PrintServer& server = iter->second;
        selected_ids.push_back(selected_print_server_id);
        selected_print_servers.emplace(server.GetUrl(), server);
      }
    }
    selected_print_server_ids_ = selected_ids;
    server_printers_provider_->OnServersChanged(true, selected_print_servers);
  }

  // Public API function.
  PrintServersConfig GetPrintServersConfig() const override { return config_; }

 private:
  std::unique_ptr<PrintServersPolicyProvider> print_servers_provider_;

  // The IDs of the currently selected print servers.
  std::vector<std::string> selected_print_server_ids_;

  ServerPrintersFetchingMode fetching_mode_;

  std::optional<std::map<std::string, PrintServer>> print_servers_;

  PrintServersConfig config_;

  std::unique_ptr<ServerPrintersProvider> server_printers_provider_;

  base::ObserverList<PrintServersManager::Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<PrintServersManagerImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<PrintServersManager> PrintServersManager::Create(
    Profile* profile) {
  return std::make_unique<PrintServersManagerImpl>(
      PrintServersPolicyProvider::Create(profile),
      ServerPrintersProvider::Create(profile));
}

// static
std::unique_ptr<PrintServersManager> PrintServersManager::CreateForTesting(
    std::unique_ptr<ServerPrintersProvider> server_printers_provider,
    std::unique_ptr<PrintServersPolicyProvider> print_servers_provider) {
  return std::make_unique<PrintServersManagerImpl>(
      std::move(print_servers_provider), std::move(server_printers_provider));
}

// static
void PrintServersManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  PrintServersProvider::RegisterProfilePrefs(registry);
}

// static
void PrintServersManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  PrintServersProvider::RegisterLocalStatePrefs(registry);
}

}  // namespace ash

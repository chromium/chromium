// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/print_servers_policy_provider.h"

#include "base/functional/bind.h"
#include "chrome/browser/ash/printing/enterprise/print_servers_provider.h"
#include "chrome/browser/ash/printing/enterprise/print_servers_provider_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

namespace ash {

namespace {

constexpr int kMaxRecords = 16;

}  // namespace

PrintServersPolicyProvider::PrintServersPolicyProvider(
    base::WeakPtr<PrintServersProvider> user_policy_provider,
    base::WeakPtr<PrintServersProvider> device_policy_provider)
    : user_policy_provider_(user_policy_provider),
      device_policy_provider_(device_policy_provider) {
  user_policy_provider_->AddObserver(this);
  device_policy_provider_->AddObserver(this);
}

PrintServersPolicyProvider::~PrintServersPolicyProvider() {
  if (device_policy_provider_)
    device_policy_provider_->RemoveObserver(this);
  if (user_policy_provider_)
    user_policy_provider_->RemoveObserver(this);
}

// static
std::unique_ptr<PrintServersPolicyProvider> PrintServersPolicyProvider::Create(
    Profile* profile) {
  base::WeakPtr<PrintServersProvider> user_policy_provider =
      PrintServersProviderFactory::Get()->GetForProfile(profile);
  user_policy_provider->SetAllowlistPref(profile->GetPrefs(),
                                         prefs::kExternalPrintServersAllowlist);
  base::WeakPtr<PrintServersProvider> device_policy_provider =
      PrintServersProviderFactory::Get()->GetForDevice();
  device_policy_provider->SetAllowlistPref(
      g_browser_process->local_state(),
      prefs::kDeviceExternalPrintServersAllowlist);
  return std::make_unique<PrintServersPolicyProvider>(user_policy_provider,
                                                      device_policy_provider);
}

// static
std::unique_ptr<PrintServersPolicyProvider>
PrintServersPolicyProvider::CreateForTesting(
    base::WeakPtr<PrintServersProvider> user_policy_provider,
    base::WeakPtr<PrintServersProvider> device_policy_provider) {
  return std::make_unique<PrintServersPolicyProvider>(user_policy_provider,
                                                      device_policy_provider);
}

void PrintServersPolicyProvider::SetListener(OnPrintServersChanged callback) {
  callback_ = std::move(callback);
  RecalculateServersAndNotifyListener();
}

void PrintServersPolicyProvider::OnServersChanged(
    bool unused_complete,
    const std::vector<PrintServer>& unused_servers) {
  RecalculateServersAndNotifyListener();
}

void PrintServersPolicyProvider::RecalculateServersAndNotifyListener() {
  if (!callback_) {
    return;
  }
  std::map<GURL, PrintServer> all_servers;
  auto device_servers = device_policy_provider_->GetPrintServers();
  if (device_servers.has_value()) {
    for (const auto& server : device_servers.value()) {
      all_servers.emplace(server.GetUrl(), server);
    }
  }
  auto user_servers = user_policy_provider_->GetPrintServers();
  if (user_servers.has_value()) {
    for (const auto& server : user_servers.value()) {
      all_servers.emplace(server.GetUrl(), server);
    }
  }
  bool is_complete = user_servers.has_value() || device_servers.has_value();
  ServerPrintersFetchingMode fetching_mode = GetFetchingMode(all_servers);
  callback_.Run(is_complete, all_servers, fetching_mode);
}

ServerPrintersFetchingMode PrintServersPolicyProvider::GetFetchingMode(
    const std::map<GURL, PrintServer>& all_servers) {
  return all_servers.size() <= kMaxRecords
             ? ServerPrintersFetchingMode::kStandard
             : ServerPrintersFetchingMode::kSingleServerOnly;
}

}  // namespace ash

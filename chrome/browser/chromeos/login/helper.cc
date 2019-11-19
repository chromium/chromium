// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/helper.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/login/signin_partition_manager.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

namespace {

constexpr char kInvalidJsonError[] = "Invalid JSON Dictionary";

}  // namespace

gfx::Rect CalculateScreenBounds(const gfx::Size& size) {
  gfx::Rect bounds = display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  if (!size.IsEmpty()) {
    int horizontal_diff = bounds.width() - size.width();
    int vertical_diff = bounds.height() - size.height();
    bounds.Inset(horizontal_diff / 2, vertical_diff / 2);
  }
  return bounds;
}

int GetCurrentUserImageSize() {
  // The biggest size that the profile picture is displayed at is currently
  // 220px, used for the big preview on OOBE and Change Picture options page.
  static const int kBaseUserImageSize = 220;
  float scale_factor = display::Display::GetForcedDeviceScaleFactor();
  if (scale_factor > 1.0f)
    return static_cast<int>(scale_factor * kBaseUserImageSize);
  return kBaseUserImageSize * gfx::ImageSkia::GetMaxSupportedScale();
}

namespace login {

NetworkStateHelper::NetworkStateHelper() {}
NetworkStateHelper::~NetworkStateHelper() {}

base::string16 NetworkStateHelper::GetCurrentNetworkName() const {
  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  const NetworkState* network =
      nsh->ConnectedNetworkByType(NetworkTypePattern::NonVirtual());
  if (network) {
    if (network->Matches(NetworkTypePattern::Ethernet()))
      return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    return base::UTF8ToUTF16(network->name());
  }

  network = nsh->ConnectingNetworkByType(NetworkTypePattern::NonVirtual());
  if (network) {
    if (network->Matches(NetworkTypePattern::Ethernet()))
      return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    return base::UTF8ToUTF16(network->name());
  }
  return base::string16();
}

void NetworkStateHelper::GetConnectedWifiNetwork(std::string* out_onc_spec) {
  const NetworkState* network_state =
      NetworkHandler::Get()->network_state_handler()->ConnectedNetworkByType(
          NetworkTypePattern::WiFi());

  if (!network_state)
    return;

  std::unique_ptr<base::DictionaryValue> current_onc =
      network_util::TranslateNetworkStateToONC(network_state);
  std::string security;
  current_onc->GetString(
      onc::network_config::WifiProperty(onc::wifi::kSecurity), &security);
  if (security != onc::wifi::kSecurityNone)
    return;

  const std::string hex_ssid = network_state->GetHexSsid();

  std::unique_ptr<base::DictionaryValue> copied_onc(
      new base::DictionaryValue());
  copied_onc->Set(onc::toplevel_config::kType,
                  std::make_unique<base::Value>(onc::network_type::kWiFi));
  copied_onc->Set(onc::network_config::WifiProperty(onc::wifi::kHexSSID),
                  std::make_unique<base::Value>(hex_ssid));
  copied_onc->Set(onc::network_config::WifiProperty(onc::wifi::kSecurity),
                  std::make_unique<base::Value>(security));
  base::JSONWriter::Write(*copied_onc.get(), out_onc_spec);
}

void NetworkStateHelper::CreateAndConnectNetworkFromOnc(
    const std::string& onc_spec,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) const {
  std::string error;
  std::unique_ptr<base::Value> root =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          onc_spec, base::JSON_ALLOW_TRAILING_COMMAS, nullptr, &error);

  base::DictionaryValue* toplevel_onc = nullptr;
  if (!root || !root->GetAsDictionary(&toplevel_onc)) {
    LOG(ERROR) << kInvalidJsonError << ": " << error;
    std::unique_ptr<base::DictionaryValue> error_data =
        std::make_unique<base::DictionaryValue>();
    error_data->SetString(network_handler::kErrorName, kInvalidJsonError);
    error_data->SetString(network_handler::kErrorDetail, error);
    error_callback.Run(kInvalidJsonError, std::move(error_data));
    return;
  }

  NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->CreateConfiguration(
          "", *toplevel_onc,
          base::Bind(&NetworkStateHelper::OnCreateConfiguration,
                     base::Unretained(this), success_callback, error_callback),
          error_callback);
}

bool NetworkStateHelper::IsConnected() const {
  chromeos::NetworkStateHandler* nsh =
      chromeos::NetworkHandler::Get()->network_state_handler();
  return nsh->ConnectedNetworkByType(chromeos::NetworkTypePattern::Default()) !=
         nullptr;
}

bool NetworkStateHelper::IsConnecting() const {
  chromeos::NetworkStateHandler* nsh =
      chromeos::NetworkHandler::Get()->network_state_handler();
  return nsh->ConnectingNetworkByType(
             chromeos::NetworkTypePattern::Default()) != nullptr;
}

void NetworkStateHelper::OnCreateConfiguration(
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& service_path,
    const std::string& guid) const {
  // Connect to the network.
  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      service_path, success_callback, error_callback,
      false /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
}

content::StoragePartition* GetSigninPartition() {
  Profile* signin_profile = ProfileHelper::GetSigninProfile();
  SigninPartitionManager* signin_partition_manager =
      SigninPartitionManager::Factory::GetForBrowserContext(signin_profile);
  if (!signin_partition_manager->IsInSigninSession())
    return nullptr;
  return signin_partition_manager->GetCurrentStoragePartition();
}

network::mojom::NetworkContext* GetSigninNetworkContext() {
  content::StoragePartition* signin_partition = GetSigninPartition();

  if (!signin_partition)
    return nullptr;

  return signin_partition->GetNetworkContext();
}

scoped_refptr<network::SharedURLLoaderFactory> GetSigninURLLoaderFactory() {
  content::StoragePartition* signin_partition = GetSigninPartition();

  // Special case for unit tests. There's no LoginDisplayHost thus no
  // webview instance. See http://crbug.com/477402
  if (!signin_partition && !LoginDisplayHost::default_host())
    return ProfileHelper::GetSigninProfile()->GetURLLoaderFactory();

  if (!signin_partition)
    return nullptr;

  return signin_partition->GetURLLoaderFactoryForBrowserProcess();
}

void SaveSyncPasswordDataToProfile(const UserContext& user_context,
                                   Profile* profile) {
  DCHECK(user_context.GetSyncPasswordData().has_value());
  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS);
  if (password_store) {
    password_store->SaveSyncPasswordHash(
        user_context.GetSyncPasswordData().value(),
        password_manager::metrics_util::GaiaPasswordHashChange::
            SAVED_ON_CHROME_SIGNIN);
  }
}

}  // namespace login

}  // namespace chromeos

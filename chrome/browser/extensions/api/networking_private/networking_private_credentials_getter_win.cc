// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_credentials_getter.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_crypto.h"
#include "chrome/services/wifi_util_win/public/mojom/constants.mojom.h"
#include "chrome/services/wifi_util_win/public/mojom/wifi_credentials_getter.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

using extensions::NetworkingPrivateCredentialsGetter;

class CredentialsGetterHostClient {
 public:
  static CredentialsGetterHostClient* Create(const std::string& public_key) {
    return new CredentialsGetterHostClient(public_key);
  }

  void GetWiFiCredentialsOnIOThread(
      const std::string& network_guid,
      const NetworkingPrivateCredentialsGetter::CredentialsCallback& callback,
      std::unique_ptr<service_manager::Connector> connector);

 private:
  explicit CredentialsGetterHostClient(const std::string& public_key)
      : public_key_(public_key.begin(), public_key.end()) {}

  ~CredentialsGetterHostClient() = default;

  // Credentials result handler.
  void GetWiFiCredentialsDone(bool success, const std::string& key_data);

  // Report the result to |callback_|.
  void ReportResult(bool success, const std::string& key_data);

  // Public key used to encrypt the result.
  std::vector<uint8_t> public_key_;

  // Callback for reporting the encrypted result.
  NetworkingPrivateCredentialsGetter::CredentialsCallback callback_;

  // Utility process used to get the credentials.
  chrome::mojom::WiFiCredentialsGetterPtr wifi_credentials_getter_;

  // WiFi network to get the credentials from.
  std::string wifi_network_;

  DISALLOW_COPY_AND_ASSIGN(CredentialsGetterHostClient);
};

void CredentialsGetterHostClient::GetWiFiCredentialsOnIOThread(
    const std::string& network_guid,
    const NetworkingPrivateCredentialsGetter::CredentialsCallback& callback,
    std::unique_ptr<service_manager::Connector> connector) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!wifi_credentials_getter_);
  DCHECK(!callback.is_null());

  wifi_network_ = network_guid;
  callback_ = callback;

  connector->BindInterface(chrome::mojom::kWifiUtilWinServiceName,
                           &wifi_credentials_getter_);

  wifi_credentials_getter_.set_connection_error_handler(
      base::Bind(&CredentialsGetterHostClient::GetWiFiCredentialsDone,
                 base::Unretained(this), false, std::string()));
  wifi_credentials_getter_->GetWiFiCredentials(
      wifi_network_,
      base::Bind(&CredentialsGetterHostClient::GetWiFiCredentialsDone,
                 base::Unretained(this)));
}

void CredentialsGetterHostClient::GetWiFiCredentialsDone(
    bool success,
    const std::string& key_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  wifi_credentials_getter_.reset();
  ReportResult(success, key_data);
  delete this;
}

void CredentialsGetterHostClient::ReportResult(bool success,
                                               const std::string& key_data) {
  if (!success) {
    callback_.Run(std::string(), "Get Credentials Failed");
    return;
  }

  if (wifi_network_ == chrome::mojom::WiFiCredentialsGetter::kWiFiTestNetwork) {
    DCHECK_EQ(wifi_network_, key_data);
    callback_.Run(key_data, std::string());
    return;
  }

  std::vector<uint8_t> ciphertext;
  if (!networking_private_crypto::EncryptByteString(public_key_, key_data,
                                                    &ciphertext)) {
    callback_.Run(std::string(), "Encrypt Credentials Failed");
    return;
  }

  std::string base64_encoded_key_data;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(ciphertext.data()),
                        ciphertext.size()),
      &base64_encoded_key_data);
  callback_.Run(base64_encoded_key_data, std::string());
}

}  // namespace

namespace extensions {

class NetworkingPrivateCredentialsGetterWin
    : public NetworkingPrivateCredentialsGetter {
 public:
  NetworkingPrivateCredentialsGetterWin() = default;

  void Start(const std::string& network_guid,
             const std::string& public_key,
             const CredentialsCallback& callback) override {
    // Bounce to the UI thread to retrieve a service_manager::Connector.
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::Bind(&NetworkingPrivateCredentialsGetterWin::StartOnUIThread,
                   network_guid, public_key, callback));
  }

 private:
  ~NetworkingPrivateCredentialsGetterWin() override = default;

  static void StartOnUIThread(const std::string& network_guid,
                              const std::string& public_key,
                              const CredentialsCallback& callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    std::unique_ptr<service_manager::Connector> connector =
        content::ServiceManagerConnection::GetForProcess()
            ->GetConnector()
            ->Clone();
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            &NetworkingPrivateCredentialsGetterWin::GetCredentialsOnIOThread,
            network_guid, public_key, callback, std::move(connector)));
  }

  static void GetCredentialsOnIOThread(
      const std::string& network_guid,
      const std::string& public_key,
      const CredentialsCallback& callback,
      std::unique_ptr<service_manager::Connector> connector) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    // CredentialsGetterHostClient is self deleting.
    CredentialsGetterHostClient* client =
        CredentialsGetterHostClient::Create(public_key);
    client->GetWiFiCredentialsOnIOThread(network_guid, callback,
                                         std::move(connector));
  }

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateCredentialsGetterWin);
};

NetworkingPrivateCredentialsGetter*
NetworkingPrivateCredentialsGetter::Create() {
  return new NetworkingPrivateCredentialsGetterWin();
}

}  // namespace extensions

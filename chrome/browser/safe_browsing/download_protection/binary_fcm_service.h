// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_BINARY_FCM_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_BINARY_FCM_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/safe_browsing/proto/webprotect.pb.h"

class Profile;

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace safe_browsing {

// This class handles the interactions with FCM for asynchronously receiving
// notifications when scans are complete. It has two major responsibilities:
// management of the InstanceID tokens, and routing of incoming FCM messages
// to the appropriate callbacks.
// This class is neither copyable nor movable.
class BinaryFCMService : public gcm::GCMAppHandler {
 public:
  static std::unique_ptr<BinaryFCMService> Create(Profile* profile);

  BinaryFCMService(gcm::GCMDriver* gcm_driver,
                   instance_id::InstanceIDDriver* instance_id_driver);
  ~BinaryFCMService() override;
  BinaryFCMService(BinaryFCMService&) = delete;
  BinaryFCMService& operator=(BinaryFCMService&) = delete;
  BinaryFCMService(BinaryFCMService&&) = delete;
  BinaryFCMService& operator=(BinaryFCMService&&) = delete;

  using GetInstanceIDCallback =
      base::OnceCallback<void(const std::string& token)>;
  using OnMessageCallback =
      base::RepeatingCallback<void(DeepScanningClientResponse)>;

  virtual void GetInstanceID(GetInstanceIDCallback callback);
  void SetCallbackForToken(const std::string& token,
                           OnMessageCallback callback);
  void ClearCallbackForToken(const std::string& token);

  // GCMAppHandler implementation
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;
  bool CanHandle(const std::string& app_id) const override;

  static const char kInvalidId[];

 protected:
  // Constructor used by mock implementation
  BinaryFCMService();

 private:
  void OnGetInstanceID(GetInstanceIDCallback callback,
                       const std::string& instance_id,
                       instance_id::InstanceID::Result result);

  // References to the profile's GCMDriver and InstanceIDDriver. Both are
  // unowned.
  gcm::GCMDriver* gcm_driver_;
  instance_id::InstanceIDDriver* instance_id_driver_;

  std::unordered_map<std::string, OnMessageCallback> message_token_map_;

  base::WeakPtrFactory<BinaryFCMService> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_BINARY_FCM_SERVICE_H_

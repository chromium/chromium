// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_FCM_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_FCM_SERVICE_H_

#include <deque>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/instance_id/instance_id.h"

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
  using OnMessageCallback = base::RepeatingCallback<void(
      enterprise_connectors::ContentAnalysisResponse)>;
  using UnregisterInstanceIDCallback = base::OnceCallback<void(bool)>;

  // Get an InstanceID for use.
  virtual void GetInstanceID(GetInstanceIDCallback callback);

  // Called to indicate the caller is done with the InstanceID. This may
  // invalidate the InstanceID, once all callers using the same InstanceID call
  // this method.
  virtual void UnregisterInstanceID(const std::string& token,
                                    UnregisterInstanceIDCallback callback);

  void SetCallbackForToken(const std::string& token,
                           OnMessageCallback callback);
  void ClearCallbackForToken(const std::string& token);

  // Performs cleanup needed at shutdown.
  void Shutdown();

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

  // Indicates if the underlying implementation is in a state allowing messages
  // to be received and propagated to `message_token_map_` callbacks.
  virtual bool Connected();

  static const char kInvalidId[];

  void SetQueuedOperationDelayForTesting(base::TimeDelta delay);

 protected:
  // Constructor used by mock implementation
  BinaryFCMService();

 private:
  void OnGetInstanceID(GetInstanceIDCallback callback,
                       const std::string& instance_id,
                       instance_id::InstanceID::Result result);

  void QueueGetInstanceIDCallback(GetInstanceIDCallback callback);

  // Run the next queued operation, and post a task for another operation if
  // necessary.
  void MaybeRunNextQueuedOperation();

  // Helper function that performs the actual unregistration.
  void UnregisterInstanceIDImpl(const std::string& instance_id,
                                UnregisterInstanceIDCallback callback);

  void OnInstanceIDUnregistered(const std::string& token,
                                UnregisterInstanceIDCallback callback,
                                instance_id::InstanceID::Result result);

  // References to the profile's GCMDriver and InstanceIDDriver. Both are
  // unowned.
  raw_ptr<gcm::GCMDriver, DanglingUntriaged> gcm_driver_;
  raw_ptr<instance_id::InstanceIDDriver, DanglingUntriaged> instance_id_driver_;

  // Queue of pending GetToken calls.
  std::deque<base::OnceClosure> pending_token_calls_;

  // Count of unregistrations currently happening asynchronously.
  size_t pending_unregistrations_count_ = 0;

  // Delay between attempts to dequeue pending operations. Not constant so we
  // can override it in tests.
  base::TimeDelta delay_between_pending_attempts_ = base::Seconds(1);

  base::flat_map<std::string, OnMessageCallback> message_token_map_;

  // Map from an InstanceID to the number of callers to GetInstanceID using that
  // InstanceID.
  base::flat_map<std::string, unsigned int> instance_id_caller_counts_;

  base::WeakPtrFactory<BinaryFCMService> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_FCM_SERVICE_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_REMOTE_COMMANDS_INVALIDATOR_H_
#define CHROME_BROWSER_POLICY_CLOUD_REMOTE_COMMANDS_INVALIDATOR_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace invalidation {
class InvalidationService;
}  // namespace invalidation

namespace syncer {
class Invalidation;
}  // namespace syncer

namespace policy {

// This class provides basic intefaces for an invalidator for remote commands
// services. It's not interacting with CloudPolicyClient/CloudPolicyCore
// directly, instead, it handles the interacting with invalidation service
// only and leaves interfaces to integrate with subclasses.
class RemoteCommandsInvalidator : public syncer::InvalidationHandler {
 public:
  explicit RemoteCommandsInvalidator(std::string owner_name);
  ~RemoteCommandsInvalidator() override;

  // Initialize this invalidator to pair with |invalidation_service|. Must be
  // called before Start().
  void Initialize(invalidation::InvalidationService* invalidation_service);

  // Shutdown this invalidator. Will stop the invalidator first, and after
  // shutting down, the invalidator can't be started anymore unless it's
  // initialized again.
  void Shutdown();

  // Starts to process invalidations.
  void Start();

  // Stops to process invalidation. May only be called after Start() has been
  // called.
  void Stop();

  // Helpful accessors.
  invalidation::InvalidationService* invalidation_service() {
    return invalidation_service_;
  }

  // Whether the invalidator currently has the ability to receive invalidations.
  bool invalidations_enabled() { return invalidations_enabled_; }

  // syncer::InvalidationHandler:
  void OnInvalidatorStateChange(syncer::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const syncer::TopicInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;
  bool IsPublicTopic(const syncer::Topic& topic) const override;

 protected:
  virtual void OnInitialize() = 0;
  virtual void OnShutdown() = 0;
  virtual void OnStart() = 0;
  virtual void OnStop() = 0;

  // Subclasses must override this method to implement the actual remote
  // commands fetch.
  virtual void DoRemoteCommandsFetch(
      const syncer::Invalidation& invalidation) = 0;

  // Subclasses must call this function to set the topic for remote command
  // invalidations.
  void ReloadPolicyData(const enterprise_management::PolicyData* policy);

 private:
  // Registers this handler with |invalidation_service_| if needed and
  // subscribes to the given |topic| with the invalidation service.
  void Register(const syncer::Topic& topic);

  // Unregisters this handler and unsubscribes from the current topic with
  // the invalidation service.
  // TODO(crbug.com/1056114): Topic subscription remains active after browser
  // restart, so explicit unsubscription here causes redundant (un)subscription
  // traffic (and potentially leaking subscriptions).
  void Unregister();

  // Updates invalidations_enabled_.
  void UpdateInvalidationsEnabled();

  // The state of the object.
  enum State {
    SHUT_DOWN,
    STOPPED,
    STARTED,
  };
  State state_ = SHUT_DOWN;

  // The unique name to be returned with by GetOwnerName().
  const std::string owner_name_;

  // The invalidation service.
  invalidation::InvalidationService* invalidation_service_ = nullptr;

  // Whether the invalidator currently has the ability to receive invalidations.
  // This is true if the invalidation service is enabled and the invalidator
  // has registered for a remote commands object.
  bool invalidations_enabled_ = false;

  // Whether the invalidation service is currently enabled.
  bool invalidation_service_enabled_ = false;

  // Whether this object has registered for remote commands invalidations.
  bool is_registered_ = false;

  // The Topic representing the remote commands in the invalidation service.
  syncer::Topic topic_;

  // A thread checker to make sure that callbacks are invoked on the correct
  // thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(RemoteCommandsInvalidator);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_REMOTE_COMMANDS_INVALIDATOR_H_

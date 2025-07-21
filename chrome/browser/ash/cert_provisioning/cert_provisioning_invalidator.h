// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_INVALIDATOR_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_INVALIDATOR_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/invalidation/invalidation_listener.h"

class Profile;

namespace ash::cert_provisioning {

enum class InvalidationEvent {
  // The client has successfully subscribed to the invalidation topic.
  // This is relevant because if an invalidation was published for that
  // invalidation topic before the client has successfully subscribed, the
  // client will not receive that invalidation.
  // This could be called multiple times because the registration could need to
  // be re-established by the FCM client.
  kSuccessfullySubscribed,
  // An invalidation has been received.
  kInvalidationReceived,
};

using OnInvalidationEventCallback =
    base::RepeatingCallback<void(InvalidationEvent invalidation_event)>;

//=============== CertProvisioningInvalidationHandler ==========================

namespace internal {

// Responsible for listening to events of certificate invalidations.
class CertProvisioningInvalidationHandler
    : public invalidation::InvalidationListener::Observer {
 public:
  // Creates and registers the handler to `invalidation_listener`
  // with `listener_type`.
  // `on_invalidation_event_callback` will be called when incoming invalidation
  // is received.
  CertProvisioningInvalidationHandler(
      invalidation::InvalidationListener* invalidation_listener,
      const std::string& listener_type,
      OnInvalidationEventCallback on_invalidation_event_callback);
  CertProvisioningInvalidationHandler(
      const CertProvisioningInvalidationHandler&) = delete;
  CertProvisioningInvalidationHandler& operator=(
      const CertProvisioningInvalidationHandler&) = delete;

  ~CertProvisioningInvalidationHandler() override;

  // invalidation::InvalidationListener::Observer
  void OnExpectationChanged(
      invalidation::InvalidationsExpected expected) override;
  void OnInvalidationReceived(
      const invalidation::DirectInvalidation& invalidation) override;
  std::string GetType() const override;

 private:
  // Returns true if ready to receive invalidations.
  bool IsRegistered() const;

  // Returns true if ready to receive invalidations and invalidations are
  // enabled.
  bool AreInvalidationsEnabled() const;

  // Sequence checker to ensure that calls from invalidation service are
  // consecutive.
  SEQUENCE_CHECKER(sequence_checker_);

  // An invalidation service providing the handler with incoming invalidations.
  const raw_ptr<invalidation::InvalidationListener> invalidation_listener_;

  // A listener type for routing FCM invalidations.
  const std::string listener_type_;

  invalidation::InvalidationsExpected are_invalidations_expected_ =
      invalidation::InvalidationsExpected::kMaybe;

  // A callback to be called on incoming invalidation event.
  const OnInvalidationEventCallback on_invalidation_event_callback_;

  // Automatically unregisters `this` as an observer on destruction. Should be
  // destroyed first so the other fields are still valid and can be used during
  // the unregistration.
  base::ScopedObservation<invalidation::InvalidationListener,
                          invalidation::InvalidationListener::Observer>
      invalidation_listener_observation_{this};
};

}  // namespace internal

//=============== CertProvisioningInvalidatorFactory ===========================

class CertProvisioningInvalidator;

// Interface for a factory that creates CertProvisioningInvalidators.
class CertProvisioningInvalidatorFactory {
 public:
  CertProvisioningInvalidatorFactory() = default;
  CertProvisioningInvalidatorFactory(
      const CertProvisioningInvalidatorFactory&) = delete;
  CertProvisioningInvalidatorFactory& operator=(
      const CertProvisioningInvalidatorFactory&) = delete;
  virtual ~CertProvisioningInvalidatorFactory() = default;

  virtual std::unique_ptr<CertProvisioningInvalidator> Create() = 0;
};

//=============== CertProvisioningInvalidator ==================================

// An invalidator that calls a Callback when an invalidation for a specific
// topic has been received. Register can be called multiple times for the same
// topic (e.g. after a chrome restart).
class CertProvisioningInvalidator {
 public:
  CertProvisioningInvalidator();
  CertProvisioningInvalidator(const CertProvisioningInvalidator&) = delete;
  CertProvisioningInvalidator& operator=(const CertProvisioningInvalidator&) =
      delete;
  virtual ~CertProvisioningInvalidator();

  virtual void Register(
      const std::string& listener_type,
      OnInvalidationEventCallback on_invalidation_event_callback) = 0;
  virtual void Unregister();

 protected:
  std::unique_ptr<internal::CertProvisioningInvalidationHandler>
      invalidation_handler_;
};

//=============== CertProvisioningUserInvalidatorFactory =======================

// This factory creates CertProvisioningInvalidators that use the passed user
// Profile's `InvalidationListener`.
class CertProvisioningUserInvalidatorFactory
    : public CertProvisioningInvalidatorFactory {
 public:
  explicit CertProvisioningUserInvalidatorFactory(Profile* profile);
  std::unique_ptr<CertProvisioningInvalidator> Create() override;

 private:
  raw_ptr<Profile> profile_ = nullptr;
};

//=============== CertProvisioningUserInvalidator ==============================

class CertProvisioningUserInvalidator : public CertProvisioningInvalidator {
 public:
  explicit CertProvisioningUserInvalidator(Profile* profile);

  void Register(
      const std::string& listener_type,
      OnInvalidationEventCallback on_invalidation_event_callback) override;

 private:
  raw_ptr<Profile> profile_ = nullptr;
};

//=============== CertProvisioningDeviceInvalidatorFactory =====================

// This factory creates CertProvisioningInvalidators that use the device-wide
// `InvalidationListener`.
class CertProvisioningDeviceInvalidatorFactory
    : public CertProvisioningInvalidatorFactory {
 public:
  CertProvisioningDeviceInvalidatorFactory();
  ~CertProvisioningDeviceInvalidatorFactory() override;

  explicit CertProvisioningDeviceInvalidatorFactory(
      invalidation::InvalidationListener* invalidation__listener);
  std::unique_ptr<CertProvisioningInvalidator> Create() override;

 private:
  raw_ptr<invalidation::InvalidationListener> invalidation_listener_;
};

//=============== CertProvisioningDeviceInvalidator ============================

class CertProvisioningDeviceInvalidator : public CertProvisioningInvalidator {
 public:
  explicit CertProvisioningDeviceInvalidator(
      invalidation::InvalidationListener* invalidation_listener);
  ~CertProvisioningDeviceInvalidator() override;

  void Register(
      const std::string& listener_type,
      OnInvalidationEventCallback on_invalidation_event_callback) override;

 private:
  std::string listener_type_;
  OnInvalidationEventCallback on_invalidation_event_callback_;
  raw_ptr<invalidation::InvalidationListener> invalidation_listener_;
};

}  // namespace ash::cert_provisioning

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_INVALIDATOR_H_

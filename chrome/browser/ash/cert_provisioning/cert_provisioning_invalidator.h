// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_INVALIDATOR_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_INVALIDATOR_H_

#include <memory>
#include <variant>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/policy/invalidation/affiliated_invalidation_service_provider.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidation_util.h"

class Profile;

namespace ash::cert_provisioning {

enum class CertScope;

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
// Note: If uses `InvalidationService`, an instance of invalidator will not
// automatically unregister given topic when destroyed so that subscription can
// be preserved if browser restarts. A user must explicitly call `Unregister` if
// subscription is not needed anymore.
class CertProvisioningInvalidationHandler
    : public invalidation::InvalidationHandler,
      public invalidation::InvalidationListener::Observer {
 public:
  // Creates and registers the handler to `invalidation_service_or_listener`
  // with `topic` (applicable for `InvalidationService`).
  // `on_invalidation_event_callback` will be called when incoming invalidation
  // is received. `scope` specifies a scope of invalidated certificate: user or
  // device.
  static std::unique_ptr<CertProvisioningInvalidationHandler> BuildAndRegister(
      CertScope scope,
      std::variant<invalidation::InvalidationService*,
                   invalidation::InvalidationListener*>
          invalidation_service_or_listener,
      const invalidation::Topic& topic,
      const std::string& listener_type,
      OnInvalidationEventCallback on_invalidation_event_callback);
  CertProvisioningInvalidationHandler(
      CertScope scope,
      std::variant<invalidation::InvalidationService*,
                   invalidation::InvalidationListener*>
          invalidation_service_or_listener,
      const invalidation::Topic& topic,
      const std::string& listener_type,
      OnInvalidationEventCallback on_invalidation_event_callback);
  CertProvisioningInvalidationHandler(
      const CertProvisioningInvalidationHandler&) = delete;
  CertProvisioningInvalidationHandler& operator=(
      const CertProvisioningInvalidationHandler&) = delete;

  ~CertProvisioningInvalidationHandler() override;

  // Unregisters handler and unsubscribes given topic from invalidation service
  // (if provided).
  void Unregister();

  // invalidation::InvalidationHandler:
  void OnInvalidatorStateChange(invalidation::InvalidatorState state) override;
  void OnSuccessfullySubscribed(
      const invalidation::Topic& invalidation) override;
  void OnIncomingInvalidation(
      const invalidation::Invalidation& invalidation) override;
  std::string GetOwnerName() const override;
  bool IsPublicTopic(const invalidation::Topic& topic) const override;

  // invalidation::InvalidationListener::Observer
  void OnExpectationChanged(
      invalidation::InvalidationsExpected expected) override;
  void OnInvalidationReceived(
      const invalidation::DirectInvalidation& invalidation) override;
  std::string GetType() const override;

 private:
  // Returns true if `this` is observing any of
  // `invalidation_service_or_listener_`.
  bool IsRegistered() const;

  // Returns true if `IsRegistered()` and any of
  // `invalidation_service_or_listener_` is enabled.
  bool AreInvalidationsEnabled() const;

  // Registers the handler to `invalidation_service_or_listener_`.
  // Returns true if registered successfully or if already registered,
  // false otherwise.
  bool Register();
  // Registers the handler to `service` and subscribes with `topic_`.
  bool RegisterWithInvalidationService(
      invalidation::InvalidationService* service);

  // Sequence checker to ensure that calls from invalidation service are
  // consecutive.
  SEQUENCE_CHECKER(sequence_checker_);

  // Represents a handler's scope: user or device.
  const CertScope scope_;

  // An invalidation service providing the handler with incoming invalidations.
  const std::variant<raw_ptr<invalidation::InvalidationService>,
                     raw_ptr<invalidation::InvalidationListener>>
      invalidation_service_or_listener_ =
          static_cast<invalidation::InvalidationService*>(nullptr);

  // A topic representing certificate invalidations.
  const invalidation::Topic topic_;
  // A listener type for routing FCM invalidations.
  const std::string listener_type_;

  invalidation::InvalidationsExpected are_invalidations_expected_ =
      invalidation::InvalidationsExpected::kMaybe;

  // A callback to be called on incoming invalidation event.
  const OnInvalidationEventCallback on_invalidation_event_callback_;

  // Automatically unregisters `this` as an observer on destruction. Should be
  // destroyed first so the other fields are still valid and can be used during
  // the unregistration.
  base::ScopedObservation<invalidation::InvalidationService,
                          invalidation::InvalidationHandler>
      invalidation_service_observation_{this};
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
      const invalidation::Topic& topic,
      const std::string& listener_type,
      OnInvalidationEventCallback on_invalidation_event_callback) = 0;
  virtual void Unregister();

 protected:
  std::unique_ptr<internal::CertProvisioningInvalidationHandler>
      invalidation_handler_;
};

//=============== CertProvisioningUserInvalidatorFactory =======================

// This factory creates CertProvisioningInvalidators that use the passed user
// Profile's InvalidationService.
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
      const invalidation::Topic& topic,
      const std::string& listener_type,
      OnInvalidationEventCallback on_invalidation_event_callback) override;

 private:
  raw_ptr<Profile> profile_ = nullptr;
};

//=============== CertProvisioningDeviceInvalidatorFactory =====================

// This factory creates CertProvisioningInvalidators that use the device-wide
// `InvalidationService` or `InvalidationListener`.
class CertProvisioningDeviceInvalidatorFactory
    : public CertProvisioningInvalidatorFactory {
 public:
  CertProvisioningDeviceInvalidatorFactory();
  ~CertProvisioningDeviceInvalidatorFactory() override;

  explicit CertProvisioningDeviceInvalidatorFactory(
      std::variant<policy::AffiliatedInvalidationServiceProvider*,
                   invalidation::InvalidationListener*>
          invalidation_service_provider_or_listener);
  std::unique_ptr<CertProvisioningInvalidator> Create() override;

 private:
  std::variant<raw_ptr<policy::AffiliatedInvalidationServiceProvider>,
               raw_ptr<invalidation::InvalidationListener>>
      invalidation_service_provider_or_listener_ =
          static_cast<policy::AffiliatedInvalidationServiceProvider*>(nullptr);
};

//=============== CertProvisioningDeviceInvalidator ============================

class CertProvisioningDeviceInvalidator
    : public CertProvisioningInvalidator,
      public policy::AffiliatedInvalidationServiceProvider::Consumer {
 public:
  explicit CertProvisioningDeviceInvalidator(
      std::variant<policy::AffiliatedInvalidationServiceProvider*,
                   invalidation::InvalidationListener*>
          invalidation_service_provider_or_listener);
  ~CertProvisioningDeviceInvalidator() override;

  void Register(
      const invalidation::Topic& topic,
      const std::string& listener_type,
      OnInvalidationEventCallback on_invalidation_event_callback) override;
  void Unregister() override;

 private:
  // policy::AffiliatedInvalidationServiceProvider::Consumer
  void OnInvalidationServiceSet(
      invalidation::InvalidationService* invalidation_service) override;

  invalidation::Topic topic_;
  std::string listener_type_;
  OnInvalidationEventCallback on_invalidation_event_callback_;
  std::variant<raw_ptr<policy::AffiliatedInvalidationServiceProvider>,
               raw_ptr<invalidation::InvalidationListener>>
      invalidation_service_provider_or_listener_ =
          static_cast<policy::AffiliatedInvalidationServiceProvider*>(nullptr);
};

}  // namespace ash::cert_provisioning

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_INVALIDATOR_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_INVALIDATOR_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_INVALIDATOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/policy/invalidation/affiliated_invalidation_service_provider.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_util.h"

class Profile;

namespace invalidation {
class InvalidationService;
}  // namespace invalidation

namespace ash::cert_provisioning {

enum class CertScope;

using OnInvalidationCallback = base::RepeatingClosure;

//=============== CertProvisioningInvalidationHandler ==========================

namespace internal {

// Responsible for listening to events of certificate invalidations.
// Note: An instance of invalidator will not automatically unregister given
// topic when destroyed so that subscription can be preserved if browser
// restarts. A user must explicitly call |Unregister| if subscription is not
// needed anymore.
class CertProvisioningInvalidationHandler
    : public invalidation::InvalidationHandler {
 public:
  // Creates and registers the handler to |invalidation_service| with |topic|.
  // |on_invalidation_callback| will be called when incoming invalidation is
  // received. |scope| specifies a scope of invalidated certificate: user or
  // device.
  static std::unique_ptr<CertProvisioningInvalidationHandler> BuildAndRegister(
      CertScope scope,
      invalidation::InvalidationService* invalidation_service,
      const invalidation::Topic& topic,
      OnInvalidationCallback on_invalidation_callback);

  CertProvisioningInvalidationHandler(
      CertScope scope,
      invalidation::InvalidationService* invalidation_service,
      const invalidation::Topic& topic,
      OnInvalidationCallback on_invalidation_callback);
  CertProvisioningInvalidationHandler(
      const CertProvisioningInvalidationHandler&) = delete;
  CertProvisioningInvalidationHandler& operator=(
      const CertProvisioningInvalidationHandler&) = delete;

  ~CertProvisioningInvalidationHandler() override;

  // Unregisters handler and unsubscribes given topic from invalidation service.
  void Unregister();

  // invalidation::InvalidationHandler:
  void OnInvalidatorStateChange(invalidation::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const invalidation::Invalidation& invalidation) override;
  std::string GetOwnerName() const override;
  bool IsPublicTopic(const invalidation::Topic& topic) const override;

 private:
  // Returns true if `this` is observing `invalidation_service_`.
  bool IsRegistered() const;

  // Returns true if `IsRegistered()` and `invalidation_service_` is enabled.
  bool AreInvalidationsEnabled() const;

  // Registers the handler to |invalidation_service_| and subscribes with
  // |topic_|.
  // Returns true if registered successfully or if already registered,
  // false otherwise.
  bool Register();

  // Sequence checker to ensure that calls from invalidation service are
  // consecutive.
  SEQUENCE_CHECKER(sequence_checker_);

  // Represents a handler's scope: user or device.
  const CertScope scope_;

  // An invalidation service providing the handler with incoming invalidations.
  const raw_ptr<invalidation::InvalidationService, ExperimentalAsh>
      invalidation_service_;

  // A topic representing certificate invalidations.
  const invalidation::Topic topic_;

  // A callback to be called on incoming invalidation event.
  const OnInvalidationCallback on_invalidation_callback_;

  // Automatically unregisters `this` as an observer on destruction. Should be
  // destroyed first so the other fields are still valid and can be used during
  // the unregistration.
  base::ScopedObservation<invalidation::InvalidationService,
                          invalidation::InvalidationHandler>
      invalidation_service_observation_{this};
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

  virtual void Register(const invalidation::Topic& topic,
                        OnInvalidationCallback on_invalidation_callback) = 0;
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
  raw_ptr<Profile, ExperimentalAsh> profile_ = nullptr;
};

//=============== CertProvisioningUserInvalidator ==============================

class CertProvisioningUserInvalidator : public CertProvisioningInvalidator {
 public:
  explicit CertProvisioningUserInvalidator(Profile* profile);

  void Register(const invalidation::Topic& topic,
                OnInvalidationCallback on_invalidation_callback) override;

 private:
  raw_ptr<Profile, ExperimentalAsh> profile_ = nullptr;
};

//=============== CertProvisioningDeviceInvalidatorFactory =====================

// This factory creates CertProvisioningInvalidators that use the device-wide
// InvalidationService.
class CertProvisioningDeviceInvalidatorFactory
    : public CertProvisioningInvalidatorFactory {
 public:
  explicit CertProvisioningDeviceInvalidatorFactory(
      policy::AffiliatedInvalidationServiceProvider* service_provider);
  std::unique_ptr<CertProvisioningInvalidator> Create() override;

 private:
  raw_ptr<policy::AffiliatedInvalidationServiceProvider, ExperimentalAsh>
      service_provider_ = nullptr;
};

//=============== CertProvisioningDeviceInvalidator ============================

class CertProvisioningDeviceInvalidator
    : public CertProvisioningInvalidator,
      public policy::AffiliatedInvalidationServiceProvider::Consumer {
 public:
  explicit CertProvisioningDeviceInvalidator(
      policy::AffiliatedInvalidationServiceProvider* service_provider);
  ~CertProvisioningDeviceInvalidator() override;

  void Register(const invalidation::Topic& topic,
                OnInvalidationCallback on_invalidation_callback) override;
  void Unregister() override;

 private:
  // policy::AffiliatedInvalidationServiceProvider::Consumer
  void OnInvalidationServiceSet(
      invalidation::InvalidationService* invalidation_service) override;

  invalidation::Topic topic_;
  OnInvalidationCallback on_invalidation_callback_;
  raw_ptr<policy::AffiliatedInvalidationServiceProvider, ExperimentalAsh>
      service_provider_ = nullptr;
};

}  // namespace ash::cert_provisioning

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_INVALIDATOR_H_

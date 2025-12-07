// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_invalidator.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/profile_invalidation_provider.h"

namespace ash::cert_provisioning {

//=============== CertProvisioningInvalidationHandler ==========================

namespace internal {

CertProvisioningInvalidationHandler::CertProvisioningInvalidationHandler(
    invalidation::InvalidationListener* invalidation_listener,
    const std::string& listener_type,
    OnInvalidationEventCallback on_invalidation_event_callback)
    : invalidation_listener_(invalidation_listener),
      listener_type_(listener_type),
      on_invalidation_event_callback_(
          std::move(on_invalidation_event_callback)) {
  CHECK(!on_invalidation_event_callback_.is_null());

  invalidation_listener_observation_.Observe(invalidation_listener_);
}

CertProvisioningInvalidationHandler::~CertProvisioningInvalidationHandler() {
  // Explicitly reset observation of `InvalidationListener` as it needs
  // `GetType()` to remove observer and `GetType()` requires access to our
  // state.
  invalidation_listener_observation_.Reset();
}

void CertProvisioningInvalidationHandler::OnExpectationChanged(
    invalidation::InvalidationsExpected expected) {
  if (are_invalidations_expected_ ==
          invalidation::InvalidationsExpected::kMaybe &&
      expected == invalidation::InvalidationsExpected::kYes) {
    // If an invalidation is sent from the server-side before the device uploads
    // the token to receive it, the invalidation can get lost.
    // Emit kSuccessfullySubscribed after all initializations are complete to
    // cover for the potentially lost invalidations.
    on_invalidation_event_callback_.Run(
        InvalidationEvent::kSuccessfullySubscribed);
  }
  are_invalidations_expected_ = expected;
}

void CertProvisioningInvalidationHandler::OnInvalidationReceived(
    const invalidation::DirectInvalidation& invalidation) {
  on_invalidation_event_callback_.Run(InvalidationEvent::kInvalidationReceived);
}

std::string CertProvisioningInvalidationHandler::GetType() const {
  return listener_type_;
}

bool CertProvisioningInvalidationHandler::IsRegistered() const {
  return invalidation_listener_observation_.IsObservingSource(
      invalidation_listener_);
}

bool CertProvisioningInvalidationHandler::AreInvalidationsEnabled() const {
  if (!IsRegistered()) {
    return false;
  }

  return are_invalidations_expected_ ==
         invalidation::InvalidationsExpected::kYes;
}

}  // namespace internal

//=============== CertProvisioningUserInvalidator ==============================

CertProvisioningInvalidator::CertProvisioningInvalidator() = default;
CertProvisioningInvalidator::~CertProvisioningInvalidator() = default;

void CertProvisioningInvalidator::Unregister() {
  invalidation_handler_.reset();
}

//=============== CertProvisioningUserInvalidatorFactory =======================

CertProvisioningUserInvalidatorFactory::CertProvisioningUserInvalidatorFactory(
    Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
}

std::unique_ptr<CertProvisioningInvalidator>
CertProvisioningUserInvalidatorFactory::Create() {
  return std::make_unique<CertProvisioningUserInvalidator>(profile_);
}

//=============== CertProvisioningUserInvalidator ==============================

CertProvisioningUserInvalidator::CertProvisioningUserInvalidator(
    Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
}

void CertProvisioningUserInvalidator::Register(
    const std::string& listener_type,
    OnInvalidationEventCallback on_invalidation_event_callback) {
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(profile_);
  CHECK(invalidation_provider);

  invalidation_handler_ =
      std::make_unique<internal::CertProvisioningInvalidationHandler>(
          invalidation_provider->GetInvalidationListener(
              kCertProvisioningInvalidationProjectNumber),
          listener_type, std::move(on_invalidation_event_callback));
}

//=============== CertProvisioningDeviceInvalidatorFactory =====================

CertProvisioningDeviceInvalidatorFactory::
    CertProvisioningDeviceInvalidatorFactory(
        invalidation::InvalidationListener* invalidation_listener)
    : invalidation_listener_(invalidation_listener) {}

CertProvisioningDeviceInvalidatorFactory::
    CertProvisioningDeviceInvalidatorFactory() = default;
CertProvisioningDeviceInvalidatorFactory::
    ~CertProvisioningDeviceInvalidatorFactory() = default;

std::unique_ptr<CertProvisioningInvalidator>
CertProvisioningDeviceInvalidatorFactory::Create() {
  return std::make_unique<CertProvisioningDeviceInvalidator>(
      invalidation_listener_);
}

//=============== CertProvisioningDeviceInvalidator ============================

CertProvisioningDeviceInvalidator::CertProvisioningDeviceInvalidator(
    invalidation::InvalidationListener* invalidation_listener)
    : invalidation_listener_(invalidation_listener) {}

CertProvisioningDeviceInvalidator::~CertProvisioningDeviceInvalidator() =
    default;

void CertProvisioningDeviceInvalidator::Register(
    const std::string& listener_type,
    OnInvalidationEventCallback on_invalidation_event_callback) {
  listener_type_ = listener_type;
  CHECK(!listener_type_.empty());
  on_invalidation_event_callback_ = std::move(on_invalidation_event_callback);
  invalidation_handler_ =
      std::make_unique<internal::CertProvisioningInvalidationHandler>(
          invalidation_listener_, listener_type_,
          on_invalidation_event_callback_);
}

}  // namespace ash::cert_provisioning

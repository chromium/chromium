// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_invalidator.h"

#include <utility>

#include "base/functional/overloaded.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "components/invalidation/invalidation_factory.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace ash::cert_provisioning {

namespace {

// Topics that start with this prefix are considered to be "public" FCM topics.
// This allows us to migrate to "private" FCM topics (which would get a
// different prefix) server-side without client-side changes.
constexpr char kFcmCertProvisioningPublicTopicPrefix[] = "cert-";

// Shall be expanded to cert.[scope].[topic]
constexpr char kOwnerNameFormat[] = "cert.%s.%s";

const char* CertScopeToString(CertScope scope) {
  switch (scope) {
    case CertScope::kUser:
      return "user";
    case CertScope::kDevice:
      return "device";
  }

  NOTREACHED_IN_MIGRATION()
      << "Unknown cert scope: " << static_cast<int>(scope);
  return "";
}

}  // namespace

//=============== CertProvisioningInvalidationHandler ==========================

namespace internal {

// static
std::unique_ptr<CertProvisioningInvalidationHandler>
CertProvisioningInvalidationHandler::BuildAndRegister(
    CertScope scope,
    std::variant<invalidation::InvalidationService*,
                 invalidation::InvalidationListener*>
        invalidation_service_or_listener,
    const invalidation::Topic& topic,
    const std::string& listener_type,
    OnInvalidationEventCallback on_invalidation_event_callback) {
  auto invalidator = std::make_unique<CertProvisioningInvalidationHandler>(
      scope, std::move(invalidation_service_or_listener), topic, listener_type,
      std::move(on_invalidation_event_callback));

  if (!invalidator->Register()) {
    return nullptr;
  }

  return invalidator;
}

CertProvisioningInvalidationHandler::CertProvisioningInvalidationHandler(
    CertScope scope,
    std::variant<invalidation::InvalidationService*,
                 invalidation::InvalidationListener*>
        invalidation_service_or_listener,
    const invalidation::Topic& topic,
    const std::string& listener_type,
    OnInvalidationEventCallback on_invalidation_event_callback)
    : scope_(scope),
      invalidation_service_or_listener_(
          invalidation::PointerVariantToRawPointer(
              invalidation_service_or_listener)),
      topic_(topic),
      listener_type_(listener_type),
      on_invalidation_event_callback_(
          std::move(on_invalidation_event_callback)) {
  CHECK(!std::holds_alternative<raw_ptr<invalidation::InvalidationService>>(
            invalidation_service_or_listener_) ||
        std::get<raw_ptr<invalidation::InvalidationService>>(
            invalidation_service_or_listener_))
      << "InvalidationService is used but is null";
  CHECK(!std::holds_alternative<raw_ptr<invalidation::InvalidationListener>>(
            invalidation_service_or_listener_) ||
        std::get<raw_ptr<invalidation::InvalidationListener>>(
            invalidation_service_or_listener_))
      << "InvalidationListener is used but is null";
  CHECK(!on_invalidation_event_callback_.is_null());
}

CertProvisioningInvalidationHandler::~CertProvisioningInvalidationHandler() {
  // Unregister is not called here so that subscription can be preserved if
  // browser restarts. If subscription is not needed a user must call Unregister
  // explicitly.
}

bool CertProvisioningInvalidationHandler::Register() {
  if (IsRegistered()) {
    return true;
  }

  return std::visit(
      base::Overloaded{[this](invalidation::InvalidationService* service) {
                         return RegisterWithInvalidationService(service);
                       },
                       [this](invalidation::InvalidationListener* listener) {
                         invalidation_listener_observation_.Observe(listener);
                         return true;
                       }},
      invalidation_service_or_listener_);
}

bool CertProvisioningInvalidationHandler::RegisterWithInvalidationService(
    invalidation::InvalidationService* service) {
  OnInvalidatorStateChange(service->GetInvalidatorState());
  invalidation_service_observation_.Observe(service);

  if (!service->UpdateInterestedTopics(this,
                                       /*topics=*/{topic_})) {
    LOG(WARNING) << "Failed to register with topic " << topic_;
    return false;
  }

  return true;
}

void CertProvisioningInvalidationHandler::Unregister() {
  if (!IsRegistered()) {
    return;
  }

  if (std::holds_alternative<raw_ptr<invalidation::InvalidationService>>(
          invalidation_service_or_listener_)) {
    const auto service = std::get<raw_ptr<invalidation::InvalidationService>>(
        invalidation_service_or_listener_);
    // Assuming that updating invalidator's topics with an empty set can never
    // fail. Failure is only possible when setting a non-empty topic that is
    // already associated with some other invalidator.
    const bool topics_reset =
        service->UpdateInterestedTopics(this, invalidation::TopicSet());
    CHECK(topics_reset);
  }
  invalidation_service_observation_.Reset();
  invalidation_listener_observation_.Reset();
}

void CertProvisioningInvalidationHandler::OnInvalidatorStateChange(
    invalidation::InvalidatorState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CertProvisioningInvalidationHandler::OnSuccessfullySubscribed(
    const invalidation::Topic& topic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(topic, topic_)
      << "Successfully subscribed notification for wrong topic";

  on_invalidation_event_callback_.Run(
      InvalidationEvent::kSuccessfullySubscribed);
}

void CertProvisioningInvalidationHandler::OnIncomingInvalidation(
    const invalidation::Invalidation& invalidation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!AreInvalidationsEnabled()) {
    LOG(WARNING) << "Unexpected invalidation received.";
  }

  CHECK(invalidation.topic() == topic_)
      << "Incoming invalidation does not contain invalidation"
         " for certificate topic";

  on_invalidation_event_callback_.Run(InvalidationEvent::kInvalidationReceived);
}

std::string CertProvisioningInvalidationHandler::GetOwnerName() const {
  return base::StringPrintf(kOwnerNameFormat, CertScopeToString(scope_),
                            topic_.c_str());
}

bool CertProvisioningInvalidationHandler::IsPublicTopic(
    const invalidation::Topic& topic) const {
  return base::StartsWith(topic, kFcmCertProvisioningPublicTopicPrefix,
                          base::CompareCase::SENSITIVE);
}

void CertProvisioningInvalidationHandler::OnExpectationChanged(
    invalidation::InvalidationsExpected expected) {
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
  return std::visit(
      base::Overloaded{
          [this](invalidation::InvalidationService* service) {
            return invalidation_service_observation_.IsObservingSource(service);
          },
          [this](invalidation::InvalidationListener* listener) {
            return invalidation_listener_observation_.IsObservingSource(
                listener);
          }},
      invalidation_service_or_listener_);
}

bool CertProvisioningInvalidationHandler::AreInvalidationsEnabled() const {
  if (!IsRegistered()) {
    return false;
  }

  return std::visit(
      base::Overloaded{[](invalidation::InvalidationService* service) {
                         return service->GetInvalidatorState() ==
                                invalidation::InvalidatorState::kEnabled;
                       },
                       [this](invalidation::InvalidationListener* listener) {
                         return are_invalidations_expected_ ==
                                invalidation::InvalidationsExpected::kYes;
                       }},
      invalidation_service_or_listener_);
}

}  // namespace internal

//=============== CertProvisioningUserInvalidator ==============================

CertProvisioningInvalidator::CertProvisioningInvalidator() = default;
CertProvisioningInvalidator::~CertProvisioningInvalidator() = default;

void CertProvisioningInvalidator::Unregister() {
  if (invalidation_handler_) {
    invalidation_handler_->Unregister();
    invalidation_handler_.reset();
  }
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
    const invalidation::Topic& topic,
    const std::string& listener_type,
    OnInvalidationEventCallback on_invalidation_event_callback) {
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(profile_);
  CHECK(invalidation_provider);

  invalidation_handler_ =
      internal::CertProvisioningInvalidationHandler::BuildAndRegister(
          CertScope::kUser,
          invalidation_provider->GetInvalidationServiceOrListener(
              policy::kPolicyFCMInvalidationSenderID,
              invalidation::InvalidationListener::kProjectNumberEnterprise),
          topic, listener_type, std::move(on_invalidation_event_callback));

  if (!invalidation_handler_) {
    LOG(ERROR) << "Failed to register for invalidation topic";
  }
}

//=============== CertProvisioningDeviceInvalidatorFactory =====================

CertProvisioningDeviceInvalidatorFactory::
    CertProvisioningDeviceInvalidatorFactory(
        std::variant<policy::AffiliatedInvalidationServiceProvider*,
                     invalidation::InvalidationListener*>
            invalidation_service_provider_or_listener)
    : invalidation_service_provider_or_listener_(
          invalidation::PointerVariantToRawPointer(
              invalidation_service_provider_or_listener)) {
  CHECK(!std::holds_alternative<
            raw_ptr<policy::AffiliatedInvalidationServiceProvider>>(
            invalidation_service_provider_or_listener_) ||
        std::get<raw_ptr<policy::AffiliatedInvalidationServiceProvider>>(
            invalidation_service_provider_or_listener_))
      << "AffiliatedInvalidationServiceProvider is used but is null";
  CHECK(!std::holds_alternative<raw_ptr<invalidation::InvalidationListener>>(
            invalidation_service_provider_or_listener_) ||
        std::get<raw_ptr<invalidation::InvalidationListener>>(
            invalidation_service_provider_or_listener_))
      << "InvalidationListener is used but is null";
}

CertProvisioningDeviceInvalidatorFactory::
    CertProvisioningDeviceInvalidatorFactory() = default;
CertProvisioningDeviceInvalidatorFactory::
    ~CertProvisioningDeviceInvalidatorFactory() = default;

std::unique_ptr<CertProvisioningInvalidator>
CertProvisioningDeviceInvalidatorFactory::Create() {
  return std::make_unique<CertProvisioningDeviceInvalidator>(
      invalidation::RawPointerVariantToPointer(
          invalidation_service_provider_or_listener_));
}

//=============== CertProvisioningDeviceInvalidator ============================

CertProvisioningDeviceInvalidator::CertProvisioningDeviceInvalidator(
    std::variant<policy::AffiliatedInvalidationServiceProvider*,
                 invalidation::InvalidationListener*>
        invalidation_service_provider_or_listener)
    : invalidation_service_provider_or_listener_(
          invalidation::PointerVariantToRawPointer(
              invalidation_service_provider_or_listener)) {
  CHECK(!std::holds_alternative<
            raw_ptr<policy::AffiliatedInvalidationServiceProvider>>(
            invalidation_service_provider_or_listener_) ||
        std::get<raw_ptr<policy::AffiliatedInvalidationServiceProvider>>(
            invalidation_service_provider_or_listener_))
      << "AffiliatedInvalidationServiceProvider is used but is null";
  CHECK(!std::holds_alternative<raw_ptr<invalidation::InvalidationListener>>(
            invalidation_service_provider_or_listener_) ||
        std::get<raw_ptr<invalidation::InvalidationListener>>(
            invalidation_service_provider_or_listener_))
      << "InvalidationListener is used but is null";
}

CertProvisioningDeviceInvalidator::~CertProvisioningDeviceInvalidator() {
  // As mentioned in the class-level comment, this intentionally doesn't call
  // Unregister so that a subscription can be preserved across process restarts.
  //
  // Note that it is OK to call this even if this instance has not called
  // RegisterConsumer yet.
  std::visit(
      base::Overloaded{
          [this](
              policy::AffiliatedInvalidationServiceProvider* service_provider) {
            service_provider->UnregisterConsumer(this);
          },
          [](invalidation::InvalidationListener* listener) {
            // Do nothing.
          }},
      invalidation_service_provider_or_listener_);
}

void CertProvisioningDeviceInvalidator::Register(
    const invalidation::Topic& topic,
    const std::string& listener_type,
    OnInvalidationEventCallback on_invalidation_event_callback) {
  topic_ = topic;
  listener_type_ = listener_type;
  CHECK(!topic_.empty());
  on_invalidation_event_callback_ = std::move(on_invalidation_event_callback);
  std::visit(
      base::Overloaded{
          [this](
              policy::AffiliatedInvalidationServiceProvider* service_provider) {
            service_provider->RegisterConsumer(this);
          },
          [this](invalidation::InvalidationListener* listener) {
            invalidation_handler_ =
                internal::CertProvisioningInvalidationHandler::BuildAndRegister(
                    CertScope::kDevice, listener, topic_, listener_type_,
                    on_invalidation_event_callback_);
          }},
      invalidation_service_provider_or_listener_);
}

void CertProvisioningDeviceInvalidator::Unregister() {
  std::visit(
      base::Overloaded{
          [this](
              policy::AffiliatedInvalidationServiceProvider* service_provider) {
            service_provider->UnregisterConsumer(this);
          },
          [](invalidation::InvalidationListener* listener) {
            // Do nothing.
          }},
      invalidation_service_provider_or_listener_);
  CertProvisioningInvalidator::Unregister();
  topic_.clear();
}

void CertProvisioningDeviceInvalidator::OnInvalidationServiceSet(
    invalidation::InvalidationService* invalidation_service) {
  // This can only be called after Register() has been called, so the `topic_`
  // must be non-empty.
  CHECK(!topic_.empty());

  // Reset any previously active `invalidation_handler` as it could be referring
  // to the previous `invalidation_service`.
  invalidation_handler_.reset();

  if (!invalidation_service) {
    return;
  }

  invalidation_handler_ =
      internal::CertProvisioningInvalidationHandler::BuildAndRegister(
          CertScope::kDevice, invalidation_service, topic_, listener_type_,
          on_invalidation_event_callback_);
  if (!invalidation_handler_) {
    LOG(ERROR) << "Failed to register for invalidation topic";
  }
}

}  // namespace ash::cert_provisioning

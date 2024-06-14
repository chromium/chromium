// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker.h"

#include <stdint.h>

#include <vector>

#include "base/no_destructor.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_client.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_invalidator.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_metrics.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_serializer.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker_static.h"
#include "chrome/browser/profiles/profile.h"

namespace em = enterprise_management;

namespace ash {
namespace cert_provisioning {

// ============= CertProvisioningWorkerFactory =================================

CertProvisioningWorkerFactory* CertProvisioningWorkerFactory::test_factory_ =
    nullptr;

// static
CertProvisioningWorkerFactory* CertProvisioningWorkerFactory::Get() {
  if (test_factory_) [[unlikely]] {
    return test_factory_;
  }

  static base::NoDestructor<CertProvisioningWorkerFactory> factory;
  return factory.get();
}

std::unique_ptr<CertProvisioningWorker> CertProvisioningWorkerFactory::Create(
    std::string process_id,
    CertScope cert_scope,
    Profile* profile,
    PrefService* pref_service,
    const CertProfile& cert_profile,
    CertProvisioningClient* cert_provisioning_client,
    std::unique_ptr<CertProvisioningInvalidator> invalidator,
    base::RepeatingClosure state_change_callback,
    CertProvisioningWorkerCallback result_callback) {
  RecordEvent(cert_profile.protocol_version, cert_scope,
              CertProvisioningEvent::kWorkerCreated);
  switch (cert_profile.protocol_version) {
    case ProtocolVersion::kStatic:
      return std::make_unique<CertProvisioningWorkerStatic>(
          process_id, cert_scope, profile, pref_service, cert_profile,
          cert_provisioning_client, std::move(invalidator),
          std::move(state_change_callback), std::move(result_callback));
    case ProtocolVersion::kDynamic:
      return std::make_unique<CertProvisioningWorkerDynamic>(
          process_id, cert_scope, profile, pref_service, cert_profile,
          cert_provisioning_client, std::move(invalidator),
          std::move(state_change_callback), std::move(result_callback));
  }
}

std::unique_ptr<CertProvisioningWorker> CreateAndDeserializeWorker(
    ProtocolVersion protocol_version,
    CertScope cert_scope,
    Profile* profile,
    PrefService* pref_service,
    const base::Value::Dict& saved_worker,
    CertProvisioningClient* cert_provisioning_client,
    std::unique_ptr<CertProvisioningInvalidator> invalidator,
    base::RepeatingClosure state_change_callback,
    CertProvisioningWorkerCallback result_callback) {
  switch (protocol_version) {
    case ProtocolVersion::kStatic: {
      auto worker = std::make_unique<CertProvisioningWorkerStatic>(
          /*process_id=*/"", cert_scope, profile, pref_service, CertProfile(),
          cert_provisioning_client, std::move(invalidator),
          std::move(state_change_callback), std::move(result_callback));
      if (!CertProvisioningSerializer::DeserializeWorker(saved_worker,
                                                         worker.get())) {
        return {};
      }
      return worker;
    }
    case ProtocolVersion::kDynamic: {
      auto worker = std::make_unique<CertProvisioningWorkerDynamic>(
          /*process_id=*/"", cert_scope, profile, pref_service, CertProfile(),
          cert_provisioning_client, std::move(invalidator),
          std::move(state_change_callback), std::move(result_callback));
      if (!CertProvisioningSerializer::DeserializeWorker(saved_worker,
                                                         worker.get())) {
        return {};
      }
      return worker;
    }
  }
}

std::unique_ptr<CertProvisioningWorker>
CertProvisioningWorkerFactory::Deserialize(
    CertScope cert_scope,
    Profile* profile,
    PrefService* pref_service,
    const base::Value::Dict& saved_worker,
    CertProvisioningClient* cert_provisioning_client,
    std::unique_ptr<CertProvisioningInvalidator> invalidator,
    base::RepeatingClosure state_change_callback,
    CertProvisioningWorkerCallback result_callback) {
  std::optional<ProtocolVersion> protocol_version =
      CertProvisioningSerializer::GetProtocolVersion(saved_worker);
  if (!protocol_version) {
    LOG(ERROR) << "Could not parse protocol version";
    return {};
  }
  std::unique_ptr<CertProvisioningWorker> worker = CreateAndDeserializeWorker(
      *protocol_version, cert_scope, profile, pref_service, saved_worker,
      cert_provisioning_client, std::move(invalidator),
      std::move(state_change_callback), std::move(result_callback));
  if (!worker) {
    RecordEvent(*protocol_version, cert_scope,
                CertProvisioningEvent::kWorkerDeserializationFailed);
    return {};
  }
  CHECK_EQ(worker->GetCertProfile().protocol_version, *protocol_version);
  RecordEvent(*protocol_version, cert_scope,
              CertProvisioningEvent::kWorkerDeserialized);
  return worker;
}

// static
void CertProvisioningWorkerFactory::SetFactoryForTesting(
    CertProvisioningWorkerFactory* test_factory) {
  test_factory_ = test_factory;
}

}  // namespace cert_provisioning
}  // namespace ash

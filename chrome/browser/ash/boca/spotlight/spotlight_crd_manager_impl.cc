// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/spotlight/spotlight_crd_manager_impl.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_admin_session_controller.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/remote_commands/crd/device_command_start_crd_session_job.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace ash::boca {
namespace {
constexpr char kAccessCode[] = "accessCode";
constexpr int kCrdSessionType = policy::CrdSessionType::REMOTE_SUPPORT_SESSION;
constexpr int kIdlenessCutoffSec = 0;
constexpr bool kAckedUserPresence = true;
constexpr bool kShowConfirmationDialog = false;
constexpr bool kTerminateUponInput = false;
}  // namespace

SpotlightCrdManagerImpl::SpotlightCrdManagerImpl(PrefService* pref_service)
    : crd_controller_(std::make_unique<policy::CrdAdminSessionController>()) {
  crd_controller_->Init(
      pref_service,
      CHECK_DEREF(ash::Shell::Get()).security_curtain_controller());
  crd_job_ = std::make_unique<policy::DeviceCommandStartCrdSessionJob>(
      crd_controller_->GetDelegate());
}

SpotlightCrdManagerImpl::SpotlightCrdManagerImpl(
    std::unique_ptr<policy::DeviceCommandStartCrdSessionJob> crd_job)
    : crd_job_(std::move(crd_job)) {
  CHECK_IS_TEST();
}

SpotlightCrdManagerImpl::~SpotlightCrdManagerImpl() = default;

void SpotlightCrdManagerImpl::OnSessionStarted(
    const std::string& teacher_email) {
  if (!ash::features::IsBocaSpotlightEnabled()) {
    return;
  }
  base::Value::Dict payload;
  payload.Set("idlenessCutoffSec", kIdlenessCutoffSec);
  payload.Set("terminateUponInput", kTerminateUponInput);
  payload.Set("ackedUserPresence", kAckedUserPresence);
  payload.Set("crdSessionType", kCrdSessionType);
  payload.Set("showConfirmationDialog", kShowConfirmationDialog);
  payload.Set("adminEmail", teacher_email);

  std::optional<std::string> json_payload = base::WriteJson(payload);
  CHECK(json_payload.has_value());
  crd_job_->ParseCommandPayload(json_payload.value());
}

void SpotlightCrdManagerImpl::OnSessionEnded() {
  if (!ash::features::IsBocaSpotlightEnabled()) {
    return;
  }
  crd_job_->TerminateImpl();
}

void SpotlightCrdManagerImpl::InitiateSpotlightSession(
    ConnectionCodeCallback callback) {
  if (!ash::features::IsBocaSpotlightEnabled()) {
    return;
  }
  crd_job_->RunImpl(base::BindOnce(&SpotlightCrdManagerImpl::StartCrdResult,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(callback)));
}

void SpotlightCrdManagerImpl::StartCrdResult(
    ConnectionCodeCallback callback,
    policy::ResultType result,
    std::optional<std::string> payload) {
  if (result != policy::ResultType::kSuccess || !payload.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  base::Value::Dict root(base::JSONReader::ReadDict(payload.value()).value());
  if (auto* access_code = root.FindString(kAccessCode)) {
    std::move(callback).Run(access_code->c_str());
    return;
  }
  std::move(callback).Run(std::nullopt);
}
}  // namespace ash::boca

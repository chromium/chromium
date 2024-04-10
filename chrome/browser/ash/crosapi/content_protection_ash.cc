// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/content_protection_ash.h"

#include "ash/display/output_protection_delegate.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {

ContentProtectionAsh::ContentProtectionAsh() = default;
ContentProtectionAsh::~ContentProtectionAsh() {
  for (auto& pair : output_protection_delegates_) {
    pair.first->RemoveObserver(this);
  }
}

void ContentProtectionAsh::BindReceiver(
    mojo::PendingReceiver<mojom::ContentProtection> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

ash::OutputProtectionDelegate*
ContentProtectionAsh::FindOrCreateOutputProtectionDelegate(
    aura::Window* window) {
  auto it = output_protection_delegates_.find(window);
  if (it != output_protection_delegates_.end())
    return it->second.get();

  auto delegate = std::make_unique<ash::OutputProtectionDelegate>(window);
  ash::OutputProtectionDelegate* ptr = delegate.get();
  output_protection_delegates_[window] = std::move(delegate);
  window->AddObserver(this);
  return ptr;
}

void ContentProtectionAsh::EnableWindowProtection(
    const std::string& window_id,
    uint32_t desired_protection_mask,
    EnableWindowProtectionCallback callback) {
  aura::Window* window = crosapi::GetShellSurfaceWindow(window_id);
  if (!window) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  ash::OutputProtectionDelegate* delegate =
      FindOrCreateOutputProtectionDelegate(window);
  delegate->SetProtection(desired_protection_mask, std::move(callback));
}

void ContentProtectionAsh::QueryWindowStatus(
    const std::string& window_id,
    QueryWindowStatusCallback callback) {
  aura::Window* window = crosapi::GetShellSurfaceWindow(window_id);
  if (!window) {
    ExecuteWindowStatusCallback(std::move(callback), /*success=*/false,
                                /*link_mask=*/0,
                                /*protection_mask=*/0);
    return;
  }

  ash::OutputProtectionDelegate* delegate =
      FindOrCreateOutputProtectionDelegate(window);
  delegate->QueryStatus(
      base::BindOnce(&ContentProtectionAsh::ExecuteWindowStatusCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ContentProtectionAsh::GetSystemSalt(GetSystemSaltCallback callback) {
  ash::SystemSaltGetter::Get()->GetSystemSalt(std::move(callback));
}

void ContentProtectionAsh::ChallengePlatform(
    const std::string& service_id,
    const std::string& challenge,
    ChallengePlatformCallback callback) {
  // Remote attestation requires a cryptohome user. Since Lacros knows nothing
  // about cryptohome, we always choose to use the primary logged in user.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* user = user_manager->GetPrimaryUser();
  if (!user) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (!platform_verification_flow_) {
    platform_verification_flow_ =
        base::MakeRefCounted<ash::attestation::PlatformVerificationFlow>();
  }

  platform_verification_flow_->ChallengePlatformKey(
      user, service_id, challenge,
      base::BindOnce(&ContentProtectionAsh::OnChallengePlatform,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ContentProtectionAsh::IsVerifiedAccessEnabled(
    IsVerifiedAccessEnabledCallback callback) {
  bool enabled_for_device = false;
  ash::CrosSettings::Get()->GetBoolean(
      ash::kAttestationForContentProtectionEnabled, &enabled_for_device);
  std::move(callback).Run(enabled_for_device);
}

void ContentProtectionAsh::OnWindowDestroyed(aura::Window* window) {
  output_protection_delegates_.erase(window);
  // No need to call window->RemoveObserver() since Window* handles that before
  // calling this method.
}

void ContentProtectionAsh::ExecuteWindowStatusCallback(
    QueryWindowStatusCallback callback,
    bool success,
    uint32_t link_mask,
    uint32_t protection_mask) {
  if (success) {
    mojom::ContentProtectionWindowStatusPtr status =
        mojom::ContentProtectionWindowStatus::New();
    status->link_mask = link_mask;
    status->protection_mask = protection_mask;
    std::move(callback).Run(std::move(status));
  } else {
    std::move(callback).Run(nullptr);
  }
}

void ContentProtectionAsh::OnChallengePlatform(
    ChallengePlatformCallback callback,
    ash::attestation::PlatformVerificationFlow::Result result,
    const std::string& signed_data,
    const std::string& signature,
    const std::string& platform_key_certificate) {
  if (result != ash::attestation::PlatformVerificationFlow::SUCCESS) {
    std::move(callback).Run(nullptr);
    return;
  }

  mojom::ChallengePlatformResultPtr output =
      mojom::ChallengePlatformResult::New();
  output->signed_data = signed_data;
  output->signed_data_signature = signature;
  output->platform_key_certificate = platform_key_certificate;
  std::move(callback).Run(std::move(output));
}

}  // namespace crosapi

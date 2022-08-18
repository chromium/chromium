// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/hibernate/hibernate_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"

#if BUILDFLAG(ENABLE_HIBERNATE)
#include "chromeos/ash/components/dbus/hiberman/hiberman_client.h"  // nogncheck
#endif

namespace ash {

namespace {

HibernateManager* g_instance = nullptr;

}  // namespace

HibernateManager::HibernateManager() {
  DCHECK(!g_instance);
  g_instance = this;
}

HibernateManager::~HibernateManager() {
  g_instance = nullptr;
}

// static
HibernateManager* HibernateManager::Get() {
  return g_instance;
}

base::WeakPtr<HibernateManager> HibernateManager::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void HibernateManager::PrepareHibernateAndMaybeResumeAuthOp(
    std::unique_ptr<UserContext> user_context,
    AuthOperationCallback callback) {
  PrepareHibernateAndMaybeResume(
      std::move(user_context),
      base::BindOnce(&HibernateManager::ResumeFromHibernateAuthOpCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

#if BUILDFLAG(ENABLE_HIBERNATE)
void HibernateManager::PrepareHibernateAndMaybeResume(
    std::unique_ptr<UserContext> user_context,
    HibernateResumeCallback callback) {
  HibermanClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&HibernateManager::OnHibernateServiceAvailable,
                     weak_factory_.GetWeakPtr(), std::move(user_context),
                     std::move(callback)));
}

void HibernateManager::OnHibernateServiceAvailable(
    std::unique_ptr<UserContext> user_context,
    HibernateResumeCallback callback,
    bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "Hibernate service is unavailable";
    std::move(callback).Run(std::move(user_context), false);
  } else {
    // In a successful resume case, this function never returns, as execution
    // continues in the resumed hibernation image.
    HibermanClient::Get()->ResumeFromHibernateAS(
        user_context->GetAuthSessionId(),
        base::BindOnce(std::move(callback), std::move(user_context)));
  }
}

#else  // !ENABLE_HIBERNATE

void HibernateManager::PrepareHibernateAndMaybeResume(
    std::unique_ptr<UserContext> user_context,
    HibernateResumeCallback callback) {
  // If resume from hibernate is not enabled, just immediately turn around and
  // call the callback.
  std::move(callback).Run(std::move(user_context), true);
}

#endif

void HibernateManager::ResumeFromHibernateAuthOpCallback(
    AuthOperationCallback callback,
    std::unique_ptr<UserContext> user_context,
    bool resume_call_successful) {
  std::move(callback).Run(std::move(user_context), absl::nullopt);
}

}  // namespace ash

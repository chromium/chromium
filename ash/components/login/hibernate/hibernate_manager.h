// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_HIBERNATE_HIBERNATE_MANAGER_H_
#define ASH_COMPONENTS_LOGIN_HIBERNATE_HIBERNATE_MANAGER_H_

#include "ash/components/login/auth/public/auth_callbacks.h"
#include "ash/components/login/auth/public/user_context.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/hibernate/buildflags.h"  // ENABLE_HIBERNATE

namespace ash {

using HibernateResumeCallback =
    base::OnceCallback<void(std::unique_ptr<UserContext> user_context, bool)>;

// HibernateManager is used to initiate resume from hibernation.
class COMPONENT_EXPORT(ASH_LOGIN_HIBERNATE) HibernateManager {
 public:
  HibernateManager();

  // Not copyable or movable.
  HibernateManager(const HibernateManager&) = delete;
  HibernateManager& operator=(const HibernateManager&) = delete;

  ~HibernateManager();

  static HibernateManager* Get();

  base::WeakPtr<HibernateManager> AsWeakPtr();

  // Resume from hibernate, in the form of an AuthOperation.
  void PrepareHibernateAndMaybeResumeAuthOp(
      std::unique_ptr<UserContext> user_context,
      AuthOperationCallback callback);

  // Resume from hibernate. On a successful resume from hibernation, this never
  // returns. On failure, or if no hibernate image is available to resume to,
  // calls the callback.
  void PrepareHibernateAndMaybeResume(std::unique_ptr<UserContext> user_context,
                                      HibernateResumeCallback callback);

 private:
#if BUILDFLAG(ENABLE_HIBERNATE)
  void OnHibernateServiceAvailable(std::unique_ptr<UserContext> user_context,
                                   HibernateResumeCallback callback,
                                   bool service_is_available);
#endif

  void ResumeFromHibernateAuthOpCallback(
      AuthOperationCallback callback,
      std::unique_ptr<UserContext> user_context,
      bool resume_call_successful);

  base::WeakPtrFactory<HibernateManager> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_HIBERNATE_HIBERNATE_MANAGER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_POLICY_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_POLICY_SERVICE_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_attributes_storage_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/buildflags/buildflags.h"

class ProfileAttributesStorage;

#if BUILDFLAG(ENABLE_EXTENSIONS)
namespace extensions {
class ExtensionRegistrar;
class ExtensionSystem;
}  // namespace extensions
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// A keyed service responsible for managing sign-in related policies. May
// redirect modifications effects to other services when policies values are
// modified.
class SigninPolicyService : public KeyedService,
                            public ProfileAttributesStorageObserver {
 public:
  explicit SigninPolicyService(
      const base::FilePath& profile_path,
      ProfileAttributesStorage* profile_attributes_storage
#if BUILDFLAG(ENABLE_EXTENSIONS)
      ,
      extensions::ExtensionSystem* extension_system,
      extensions::ExtensionRegistrar* extension_registrar
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  );

  SigninPolicyService(const SigninPolicyService&) = delete;
  SigninPolicyService& operator=(const SigninPolicyService&) = delete;

  ~SigninPolicyService() override;

  // ProfileAttributesStorageObserver:
  void OnProfileSigninRequiredChanged(
      const base::FilePath& profile_path) override;

 private:
  // Callback to trigger policy checks related to extensions.
  void OnExtensionSystemReady();

  const base::FilePath profile_path_;
  const raw_ref<ProfileAttributesStorage> profile_attributes_storage_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const raw_ref<extensions::ExtensionRegistrar> extension_registrar_;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorageObserver>
      scoped_storage_observation_{this};

  base::WeakPtrFactory<SigninPolicyService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_POLICY_SERVICE_H_

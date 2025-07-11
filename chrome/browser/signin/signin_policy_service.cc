// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_policy_service.h"

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/one_shot_event.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

SigninPolicyService::SigninPolicyService(
    const base::FilePath& profile_path,
    ProfileAttributesStorage* profile_attributes_storage
#if BUILDFLAG(ENABLE_EXTENSIONS)
    ,
    extensions::ExtensionSystem* extension_system,
    extensions::ExtensionRegistrar* extension_registrar
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    )
    : profile_path_(profile_path),
      profile_attributes_storage_(CHECK_DEREF(profile_attributes_storage))
#if BUILDFLAG(ENABLE_EXTENSIONS)
      ,
      extension_registrar_(CHECK_DEREF(extension_registrar))
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
{
#if BUILDFLAG(ENABLE_EXTENSIONS)
  CHECK(extension_system);
  extension_system->ready().Post(
      FROM_HERE, base::BindOnce(&SigninPolicyService::OnExtensionSystemReady,
                                weak_ptr_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

SigninPolicyService::~SigninPolicyService() = default;

void SigninPolicyService::OnProfileSigninRequiredChanged(
    const base::FilePath& profile_path) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (profile_path_ != profile_path) {
    return;
  }

  ProfileAttributesEntry* entry =
      profile_attributes_storage_->GetProfileAttributesWithPath(profile_path);
  if (!entry) {
    return;
  }

  // When a profile is locked (sign in is required) due to enterprise policies,
  // all extensions should be blocked, mainly not to allow any background work
  // (opening a browser is also prevented in locked mode). When unlocking the
  // profile, extensions gets unblocked.
  entry->IsSigninRequired() ? extension_registrar_->BlockAllExtensions()
                            : extension_registrar_->UnblockAllExtensions();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

void SigninPolicyService::OnExtensionSystemReady() {
  scoped_storage_observation_.Observe(&profile_attributes_storage_.get());

  OnProfileSigninRequiredChanged(profile_path_);
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/global_features.h"

#include "base/check_is_test.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/system/platform_handle.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// This causes a gn error on Android builds, because gn does not understand
// buildflags, so we include it only on platforms where it is used.
#include "components/user_education/common/user_education_features.h"  // nogncheck
#include "chrome/browser/ui/webui/whats_new/whats_new_registrar.h"
#endif

namespace {

// This is the generic entry point for test code to stub out browser
// functionality. It is called by production code, but only used by tests.
GlobalFeatures::GlobalFeaturesFactory& GetFactory() {
  static base::NoDestructor<GlobalFeatures::GlobalFeaturesFactory> factory;
  return *factory;
}

}  // namespace

// static
std::unique_ptr<GlobalFeatures> GlobalFeatures::CreateGlobalFeatures() {
  if (GetFactory()) {
    CHECK_IS_TEST();
    return GetFactory().Run();
  }
  // Constructor is protected.
  return base::WrapUnique(new GlobalFeatures());
}

GlobalFeatures::~GlobalFeatures() = default;

// static
void GlobalFeatures::ReplaceGlobalFeaturesForTesting(
    GlobalFeaturesFactory factory) {
  GlobalFeatures::GlobalFeaturesFactory& f = GetFactory();
  f = std::move(factory);
}

void GlobalFeatures::Init() {
  system_permissions_platform_handle_ = CreateSystemPermissionsPlatformHandle();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (user_education::features::IsWhatsNewV2()) {
    whats_new_registry_ = CreateWhatsNewRegistry();
  }
#endif
}

std::unique_ptr<system_permission_settings::PlatformHandle>
GlobalFeatures::CreateSystemPermissionsPlatformHandle() {
  return system_permission_settings::PlatformHandle::Create();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
std::unique_ptr<whats_new::WhatsNewRegistry>
GlobalFeatures::CreateWhatsNewRegistry() {
  return whats_new::CreateWhatsNewRegistry();
}
#endif

GlobalFeatures::GlobalFeatures() = default;

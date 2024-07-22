// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/global_desktop_features.h"

#include "base/check_is_test.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_registrar.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/webui/whats_new_registry.h"

namespace {

// This is the generic entry point for test code to stub out browser
// functionality. It is called by production code, but only used by tests.
GlobalDesktopFeatures::GlobalDesktopFeaturesFactory& GetFactory() {
  static base::NoDestructor<GlobalDesktopFeatures::GlobalDesktopFeaturesFactory>
      factory;
  return *factory;
}

}  // namespace

// static
std::unique_ptr<GlobalDesktopFeatures>
GlobalDesktopFeatures::CreateGlobalDesktopFeatures() {
  if (GetFactory()) {
    CHECK_IS_TEST();
    return GetFactory().Run();
  }
  // Constructor is protected.
  return base::WrapUnique(new GlobalDesktopFeatures());
}

GlobalDesktopFeatures::~GlobalDesktopFeatures() = default;

// static
void GlobalDesktopFeatures::ReplaceGlobalDesktopFeaturesForTesting(
    GlobalDesktopFeaturesFactory factory) {
  GlobalDesktopFeatures::GlobalDesktopFeaturesFactory& f = GetFactory();
  f = std::move(factory);
}

void GlobalDesktopFeatures::Init() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (user_education::features::IsWhatsNewV2()) {
    whats_new_registry_ = CreateWhatsNewRegistry();
  }
#endif
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
std::unique_ptr<whats_new::WhatsNewRegistry>
GlobalDesktopFeatures::CreateWhatsNewRegistry() {
  return whats_new::CreateWhatsNewRegistry();
}
#endif

GlobalDesktopFeatures::GlobalDesktopFeatures() = default;

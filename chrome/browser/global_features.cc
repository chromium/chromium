// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/global_features.h"

#include "base/check_is_test.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_ANDROID)
// This causes a gn error on Android builds, because gn does not understand
// buildflags.
#include "components/user_education/common/user_education_features.h"  // nogncheck
#include "components/user_education/webui/whats_new_registry.h"  // nogncheck
#endif  // !BUILDFLAG(IS_ANDROID)

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
#if !BUILDFLAG(IS_ANDROID)
  if (user_education::features::IsWhatsNewV2()) {
    whats_new_registry_ = std::make_unique<whats_new::WhatsNewRegistry>();
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

GlobalFeatures::GlobalFeatures() = default;

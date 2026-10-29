// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_latch.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace glic {

namespace {

const void* const kGlicProviderLatchKey = &kGlicProviderLatchKey;

}  // namespace

GlicProviderLatch::GlicProviderLatch(
    GlicProvider provider,
    std::unique_ptr<const GlicProviderSpec> spec)
    : provider_(provider), spec_(std::move(spec)) {
  CHECK(spec_);
}

GlicProviderLatch::~GlicProviderLatch() = default;

// static
const GlicProviderLatch& GlicProviderLatch::Get(Profile& profile) {
  // Off-the-record profiles aren't latched; they share the latch of their
  // original profile.
  Profile* original_profile = profile.GetOriginalProfile();
  if (auto* latch = static_cast<GlicProviderLatch*>(
          original_profile->GetUserData(kGlicProviderLatchKey))) {
    return *latch;
  }
  static const base::NoDestructor<GlicProviderLatch> kFallbackLatch(
      GlicProvider::kGemini, CreateGlicProviderSpec(GlicProvider::kGemini, {}));
  return *kFallbackLatch;
}

// static
GlicProvider GlicProviderLatch::GetProvider(Profile& profile) {
  return Get(profile).provider_;
}

const GlicProviderSpec& GlicProviderLatch::GetSpec() const {
  return *spec_;
}

// static
void GlicProviderLatch::Set(base::PassKey<GlicProviderLatchFactory>,
                            content::BrowserContext& context,
                            GlicProvider provider,
                            GeicProviderInitOptions geic_options) {
  CHECK(!context.GetUserData(kGlicProviderLatchKey));
  context.SetUserData(kGlicProviderLatchKey,
                      base::WrapUnique(new GlicProviderLatch(
                          provider, CreateGlicProviderSpec(
                                        provider, std::move(geic_options)))));
}

}  // namespace glic

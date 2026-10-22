// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_latch.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"

namespace glic {

namespace {

const void* const kGlicProviderLatchKey = &kGlicProviderLatchKey;

}  // namespace

GlicProviderLatch::GlicProviderLatch(GlicProvider provider)
    : provider_(provider) {}

GlicProviderLatch::~GlicProviderLatch() = default;

// static
GlicProvider GlicProviderLatch::GetProvider(content::BrowserContext& context) {
  GlicProviderLatch* latch = Get(context);
  // Keyed service factories that read the provider must
  // DependsOn(GlicProviderLatchFactory).
  CHECK(latch);
  return latch->provider_;
}

// static
void GlicProviderLatch::Set(base::PassKey<GlicProviderLatchFactory>,
                            content::BrowserContext& context,
                            GlicProvider provider) {
  CHECK(!Get(context));
  context.SetUserData(kGlicProviderLatchKey,
                      base::WrapUnique(new GlicProviderLatch(provider)));
}

// static
GlicProviderLatch* GlicProviderLatch::Get(content::BrowserContext& context) {
  return static_cast<GlicProviderLatch*>(
      context.GetUserData(kGlicProviderLatchKey));
}

}  // namespace glic

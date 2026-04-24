// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"

#include "chrome/browser/profiles/profile.h"

namespace glic {

MockGlicKeyedService::MockGlicKeyedService(
    content::BrowserContext* context,
    signin::IdentityManager* identity_manager,
    ProfileManager* profile_manager,
    GlicProfileManager* glic_profile_manager,
    ContextualCueingService* contextual_cueing_service,
    actor::ActorKeyedService* actor_keyed_service)
    : GlicKeyedService(Profile::FromBrowserContext(context),
                       identity_manager,
                       profile_manager,
                       glic_profile_manager,
                       contextual_cueing_service,
                       actor_keyed_service) {}

MockGlicKeyedService::~MockGlicKeyedService() = default;

}  // namespace glic

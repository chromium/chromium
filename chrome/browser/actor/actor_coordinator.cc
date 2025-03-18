// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_coordinator.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

ActorCoordinator::ActorCoordinator() = default;

ActorCoordinator::~ActorCoordinator() = default;

void ActorCoordinator::Act(
    tabs::TabInterface* tab,
    const optimization_guide::proto::BrowserAction& action,
    ActionResultCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));

  // TODO(https://crbug.com/402086021): Use actor tool framework.
  std::move(callback).Run(/*succeeded=*/false);
}

}  // namespace actor

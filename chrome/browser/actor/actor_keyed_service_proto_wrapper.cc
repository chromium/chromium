// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service_proto_wrapper.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/notimplemented.h"

namespace actor {

ActorKeyedServiceProtoWrapper::ActorKeyedServiceProtoWrapper(
    ActorKeyedService* actor_service) {
  // TODO(crbug.com/565390794): Store actor_service.
  NOTIMPLEMENTED();
}

ActorKeyedServiceProtoWrapper::~ActorKeyedServiceProtoWrapper() = default;

void ActorKeyedServiceProtoWrapper::PerformActions(
    const optimization_guide::proto::Actions& actions,
    PerformActionsCallback callback) {
  // TODO(crbug.com/565390794): Implement proto actions execution and
  // observation.
  NOTIMPLEMENTED();
  std::move(callback).Run(optimization_guide::proto::ActionsResult());
}

}  // namespace actor

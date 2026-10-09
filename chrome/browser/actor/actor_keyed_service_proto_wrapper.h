// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_PROTO_WRAPPER_H_
#define CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_PROTO_WRAPPER_H_

#include "base/functional/callback_forward.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

class ActorKeyedService;

// Wraps `ActorKeyedService` to convert and execute an
// `optimization_guide::proto::Actions` request and collect post-action tab and
// window observations into an `optimization_guide::proto::ActionsResult`.
class ActorKeyedServiceProtoWrapper {
 public:
  using PerformActionsCallback =
      base::OnceCallback<void(optimization_guide::proto::ActionsResult)>;

  explicit ActorKeyedServiceProtoWrapper(ActorKeyedService* actor_service);
  ActorKeyedServiceProtoWrapper(const ActorKeyedServiceProtoWrapper&) = delete;
  ActorKeyedServiceProtoWrapper& operator=(
      const ActorKeyedServiceProtoWrapper&) = delete;
  ~ActorKeyedServiceProtoWrapper();

  void PerformActions(const optimization_guide::proto::Actions& actions,
                      PerformActionsCallback callback);
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_PROTO_WRAPPER_H_

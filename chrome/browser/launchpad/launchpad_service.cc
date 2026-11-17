// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/launchpad/launchpad_service.h"

#include <optional>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"

namespace launchpad {

LaunchpadService::LaunchpadService(
    personal_context::PersonalContextService* personal_context_service)
    : personal_context_service_(CHECK_DEREF(personal_context_service)) {}

LaunchpadService::~LaunchpadService() = default;

void LaunchpadService::GenerateTasks(GenerateTasksCallback callback) {
  personal_context::proto::AutoTodosRequest request_metadata;
  personal_context_service_->FetchContext(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
      request_metadata, /*options=*/{},
      base::BindOnce(&LaunchpadService::OnTasksFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LaunchpadService::OnTasksFetched(
    GenerateTasksCallback callback,
    personal_context::FetchContextResult result) {
  if (!result.response.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  personal_context::proto::AutoTodosResponse response;
  if (!response.ParseFromString(result.response->value())) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(std::move(response));
}

}  // namespace launchpad

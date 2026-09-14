// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_text_query_from_manta.h"

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/ash/input_method/editor_text_query_provider.h"
#include "components/manta/manta_service.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/orca_provider.h"

namespace ash::input_method {
namespace {

std::unique_ptr<manta::OrcaProvider> CreateProvider(
    manta::MantaService* service) {
  return service ? service->CreateOrcaProvider() : nullptr;
}

}  // namespace

EditorTextQueryFromManta::EditorTextQueryFromManta(
    manta::MantaService* manta_service)
    : provider_(CreateProvider(manta_service)) {}

EditorTextQueryFromManta::~EditorTextQueryFromManta() = default;

void EditorTextQueryFromManta::Call(
    const std::map<std::string, std::string> params,
    manta::MantaGenericCallback callback) {
  if (!provider_) {
    std::move(callback).Run(
        base::DictValue().Set("outputData", base::ListValue()),
        manta::MantaStatus{
            .status_code = manta::MantaStatusCode::kBackendFailure,
            .message = ""});
    return;
  }
  provider_->Call(params, std::move(callback));
}

}  // namespace ash::input_method

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_text_query_from_manta.h"

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/ash/input_method/editor_text_query_provider.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/orca_provider.h"

namespace ash::input_method {
namespace {

std::unique_ptr<manta::OrcaProvider> CreateProvider(Profile* profile) {
  if (!manta::features::IsMantaServiceEnabled()) {
    return nullptr;
  }

  manta::MantaService* service =
      manta::MantaServiceFactory::GetForProfile(profile);
  return service ? service->CreateOrcaProvider() : nullptr;
}

}  // namespace

EditorTextQueryFromManta::EditorTextQueryFromManta(Profile* profile)
    : provider_(CreateProvider(profile)) {}

EditorTextQueryFromManta::~EditorTextQueryFromManta() = default;

void EditorTextQueryFromManta::Call(
    const std::map<std::string, std::string> params,
    manta::MantaGenericCallback callback) {
  if (!provider_) {
    std::move(callback).Run(
        base::Value::Dict().Set("outputData", base::Value::List()),
        manta::MantaStatus{
            .status_code = manta::MantaStatusCode::kBackendFailure,
            .message = ""});
    return;
  }
  provider_->Call(params, std::move(callback));
}

}  // namespace ash::input_method

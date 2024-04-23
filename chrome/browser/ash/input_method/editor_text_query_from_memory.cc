// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_text_query_from_memory.h"

#include <map>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "chrome/browser/ash/input_method/editor_text_query_provider.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"

namespace ash::input_method {
namespace {

base::Value::Dict GenerateResultsFrom(const std::vector<std::string>& results) {
  base::Value::List result_list;
  for (const auto& result : results) {
    result_list.Append(base::Value::Dict().Set("text", result));
  }
  return base::Value::Dict().Set("outputData", std::move(result_list));
}

}  // namespace

EditorTextQueryFromMemory::EditorTextQueryFromMemory(
    base::span<const std::string> responses)
    : responses_(responses.begin(), responses.end()) {}

EditorTextQueryFromMemory::~EditorTextQueryFromMemory() = default;

void EditorTextQueryFromMemory::Call(
    const std::map<std::string, std::string> params,
    manta::MantaGenericCallback callback) {
  std::move(callback).Run(
      GenerateResultsFrom(responses_),
      manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk,
                         .message = ""});
}

}  // namespace ash::input_method

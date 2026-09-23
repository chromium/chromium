// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/audio_generation/overview_generation_broker.h"

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/i18n/tag_converters.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"

namespace readaloud {

OverviewGenerationBroker::OverviewGenerationBroker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

OverviewGenerationBroker::~OverviewGenerationBroker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

optimization_guide::proto::ReadAloudGenerateTextRequest
OverviewGenerationBroker::BuildGenerateTextRequest(
    std::string_view page_title,
    std::string_view page_content,
    const GURL& page_url,
    std::string_view language_code) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  optimization_guide::proto::ReadAloudGenerateTextRequest request;
  request.set_page_title(std::string(page_title));
  request.set_page_content(std::string(page_content));

  // Sanitize Web URL
  if (page_url.is_valid() && page_url.SchemeIsHTTPOrHTTPS()) {
    GURL::Replacements replacements;
    replacements.ClearUsername();
    replacements.ClearPassword();
    replacements.ClearQuery();
    replacements.ClearRef();
    request.set_page_url(page_url.ReplaceComponents(replacements).spec());
  }

  // Language code is optional. Defaults to English server-side.
  if (!language_code.empty()) {
    std::optional<base::i18n::LanguageTag> parsed =
        base::i18n::GetLanguageTagFromString(language_code);
    if (parsed) {
      request.set_language_code(std::string(parsed->tag_string()));
    }
  }
  return request;
}

void OverviewGenerationBroker::GenerateOverview(
    OptimizationGuideKeyedService* opt_guide_service,
    std::string_view page_title,
    std::string_view page_content,
    const GURL& page_url,
    std::string_view language_code,
    GenerateOverviewCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InvalidatePendingRequests();
  // Preconditions check
  if (!opt_guide_service || page_content.empty()) {
    // TODO(b/564908361): Record UMA: aborted overview generation
    // (null service or empty content).
    std::move(callback).Run(mojo_base::BigBuffer(), /*success=*/false);
    return;
  }

  // Construct request
  optimization_guide::proto::ReadAloudGenerateTextRequest request =
      BuildGenerateTextRequest(page_title, page_content, page_url,
                               language_code);

  // TODO(b/564908361): Record UMA: overview generation requested

  opt_guide_service->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kReadAloudGenerateText,
      request,
      /*options=*/{.execution_timeout = readaloud::kOverviewGenerationTimeout},
      base::BindOnce(&OverviewGenerationBroker::OnModelExecutionResult,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void OverviewGenerationBroker::InvalidatePendingRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
}

void OverviewGenerationBroker::OnModelExecutionResult(
    GenerateOverviewCallback callback,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.response.has_value()) {
    // TODO(b/564908361): Record UMA: model execution errors
    // with failure status codes.
    std::move(callback).Run(mojo_base::BigBuffer(), /*success=*/false);
    return;
  }

  // Rule of Two Security Boundary: Extract raw serialized proto::Any byte
  // string without deserializing ReadAloudGenerateTextResponse in the
  // privileged Browser process.
  const std::string& raw_bytes = result.response.value().value();
  if (raw_bytes.empty()) {
    std::move(callback).Run(mojo_base::BigBuffer(), /*success=*/false);
    return;
  }

  // TODO(b/564908361): Record UMA: generation outcome
  mojo_base::BigBuffer buffer(base::as_byte_span(raw_bytes));
  std::move(callback).Run(std::move(buffer), /*success=*/true);
}

}  // namespace readaloud

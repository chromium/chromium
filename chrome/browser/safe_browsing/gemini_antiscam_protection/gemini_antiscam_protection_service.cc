// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/features/gemini_antiscam_protection.pb.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace {

bool ContainsFieldType(content::WebContents* web_contents,
                       autofill::FieldTypeGroup field_type_group) {
  autofill::AutofillDriverFactory* adf =
      autofill::ContentAutofillDriverFactory::FromWebContents(web_contents);
  if (!adf) {
    return false;
  }
  return std::ranges::any_of(
      adf->GetExistingDrivers(), [&](autofill::AutofillDriver* d) {
        if (!d->IsActive()) {
          return false;
        }
        bool found = false;
        d->GetAutofillManager().ForEachCachedForm(
            [&found, field_type_group](const autofill::FormStructure& form) {
              found = found ||
                      std::ranges::any_of(
                          form.fields(),
                          [&](const std::unique_ptr<autofill::AutofillField>&
                                  field) {
                            return field->Type().GetGroups().contains(
                                field_type_group);
                          });
            });
        return found;
      });
}

optimization_guide::proto::GeminiAntiscamProtectionRequest
BuildGeminiAntiscamProtectionRequest(GURL url, std::string page_inner_text) {
  optimization_guide::proto::GeminiAntiscamProtectionRequest request;
  request.set_url(url.spec());
  request.set_page_content(page_inner_text);
  return request;
}

optimization_guide::proto::GeminiAntiscamProtectionResponse
BuildGeminiAntiscamProtectionResponse(float scam_score,
                                      std::string content_category,
                                      std::string justification) {
  optimization_guide::proto::GeminiAntiscamProtectionResponse response;
  response.set_scam_score(scam_score);
  response.set_content_category(content_category);
  response.set_justification(justification);
  return response;
}

// LINT.IfChange(GetContentCategory)

std::string GetContentCategory(
    std::optional<optimization_guide::proto::GeminiAntiscamProtectionResponse>
        response) {
  CHECK(response.has_value());
  if (!response->has_content_category()) {
    return "Empty";
  }
  const std::string& content_category = response->content_category();
  if (content_category == "phishing") {
    return "Phishing";
  }
  if (content_category == "tech_support") {
    return "TechSupport";
  }
  if (content_category == "investment(non-crypto)") {
    return "Investment";
  }
  if (content_category == "investment(crypto)") {
    return "Cryptocurrency";
  }
  if (content_category == "romance") {
    return "Romance";
  }
  if (content_category == "online_shopping") {
    return "OnlineShopping";
  }
  if (content_category == "prize") {
    return "Prize";
  }
  if (content_category == "job") {
    return "Job";
  }
  if (content_category == "charity") {
    return "Charity";
  }
  if (content_category == "government_impersonation") {
    return "GovernmentImpersonation";
  }
  if (content_category == "trojanized_software") {
    return "TrojanizedSoftware";
  }
  if (content_category == "miscellaneous") {
    return "Miscellaneous";
  }
  return "NoMatchFound";
}

// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/histograms.xml:GeminiAntiscamProtectionContentCategory)

void LogsGeminiAntiscamProtectionMQLS(
    base::WeakPtr<optimization_guide::ModelQualityLogsUploaderService>
        logs_uploader_service,
    optimization_guide::proto::GeminiAntiscamProtectionMetadata
        metadata_proto_log,
    GURL url,
    std::string page_inner_text,
    float scam_score,
    std::string content_category,
    std::string justification) {
  CHECK(logs_uploader_service);

  // Create request and response protos, for logging.
  optimization_guide::proto::GeminiAntiscamProtectionRequest request_proto_log =
      BuildGeminiAntiscamProtectionRequest(url, page_inner_text);
  optimization_guide::proto::GeminiAntiscamProtectionResponse
      response_proto_log = BuildGeminiAntiscamProtectionResponse(
          scam_score, content_category, justification);

  // Create `GeminiAntiscamProtectionLoggingData`, for uploading the log.
  std::unique_ptr<
      optimization_guide::proto::GeminiAntiscamProtectionLoggingData>
      logging_data = std::make_unique<
          optimization_guide::proto::GeminiAntiscamProtectionLoggingData>();
  *logging_data->mutable_request() = request_proto_log;
  *logging_data->mutable_response() = response_proto_log;
  *logging_data->mutable_metadata() = metadata_proto_log;

  // Upload log.
  auto mqls_log_entry =
      std::make_unique<optimization_guide::ModelQualityLogEntry>(
          logs_uploader_service);
  *mqls_log_entry->log_ai_data_request()->mutable_gemini_antiscam_protection() =
      *logging_data;
  optimization_guide::ModelQualityLogEntry::Upload(std::move(mqls_log_entry));
}

}  // namespace

namespace safe_browsing {

GeminiAntiscamProtectionService::GeminiAntiscamProtectionService(
    OptimizationGuideKeyedService* optimization_guide_keyed_service,
    history::HistoryService* history_service)
    : optimization_guide_keyed_service_(optimization_guide_keyed_service),
      history_service_(history_service) {}

GeminiAntiscamProtectionService::~GeminiAntiscamProtectionService() = default;

// static
optimization_guide::proto::GeminiAntiscamProtectionMetadata
GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
    content::WebContents* web_contents) {
  optimization_guide::proto::GeminiAntiscamProtectionMetadata metadata;
  metadata.set_page_contains_financial_fields(
      ContainsFieldType(web_contents, autofill::FieldTypeGroup::kCreditCard) ||
      ContainsFieldType(web_contents,
                        autofill::FieldTypeGroup::kStandaloneCvcField));
  metadata.set_page_contains_password_field(ContainsFieldType(
      web_contents, autofill::FieldTypeGroup::kPasswordField));
  metadata.set_page_contains_identity_fields(
      ContainsFieldType(web_contents, autofill::FieldTypeGroup::kAutofillAi));
  return metadata;
}

void GeminiAntiscamProtectionService::MaybeStartAntiscamProtection(
    optimization_guide::proto::GeminiAntiscamProtectionMetadata metadata,
    GURL url,
    ClientSideDetectionType request_type,
    bool did_match_high_confidence_allowlist,
    GURL last_committed_url,
    std::string page_inner_text) {
  // If the page already matches the allowlist, we don't need to run Gemini to
  // determine scamminess.
  if (did_match_high_confidence_allowlist) {
    return;
  }
  // Only run Gemini for CSD checks triggered via force request.
  if (request_type != ClientSideDetectionType::FORCE_REQUEST) {
    return;
  }
  // If the `url` does not match the last committed URL, then the user navigated
  // away and the `page_inner_text` is no longer valid. We should not run
  // Gemini.
  if (url != last_committed_url) {
    return;
  }

  // Query history service to determine if the URL has been visited before.
  // If the URL has not been visited before, run Gemini.
  history_service_->GetVisibleVisitCountToHost(
      url,
      base::BindOnce(&GeminiAntiscamProtectionService::DidGetVisibleVisitCount,
                     weak_factory_.GetWeakPtr(), metadata, url,
                     page_inner_text),
      &task_tracker_);
}

void GeminiAntiscamProtectionService::DidGetVisibleVisitCount(
    optimization_guide::proto::GeminiAntiscamProtectionMetadata metadata,
    GURL url,
    std::string page_inner_text,
    history::VisibleVisitCountToHostResult result) {
  bool is_history_service_result_valid = result.success;
  base::UmaHistogramBoolean(
      "SafeBrowsing.GeminiAntiscamProtection.IsHistoryServiceResultValid",
      is_history_service_result_valid);
  if (!is_history_service_result_valid) {
    // If the history service was not able to determine the number of visits, we
    // should not run Gemini.
    return;
  }
  bool has_user_visited_url = result.count > 1;
  base::UmaHistogramBoolean(
      "SafeBrowsing.GeminiAntiscamProtection.ShouldSkipDueToPreviousVisit",
      has_user_visited_url);
  if (has_user_visited_url) {
    // If the URL has been visited before, we don't need to run Gemini.
    return;
  }

  // Query server-side model.
  optimization_guide::proto::GeminiAntiscamProtectionRequest request =
      BuildGeminiAntiscamProtectionRequest(url, page_inner_text);
  optimization_guide_keyed_service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kGeminiAntiscamProtection,
      request, {},
      base::BindOnce(&GeminiAntiscamProtectionService::OnModelResponse,
                     weak_factory_.GetWeakPtr(), metadata,
                     base::TimeTicks::Now(), url, page_inner_text));
}

void GeminiAntiscamProtectionService::OnModelResponse(
    optimization_guide::proto::GeminiAntiscamProtectionMetadata metadata,
    base::TimeTicks start_time,
    GURL url,
    std::string page_inner_text,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  base::TimeDelta latency = base::TimeTicks::Now() - start_time;
  if (!result.response.has_value()) {
    base::UmaHistogramTimes(
        "SafeBrowsing.GeminiAntiscamProtection.FailedEmptyResponse.Latency",
        latency);
    return;
  }
  std::optional<optimization_guide::proto::GeminiAntiscamProtectionResponse>
      response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::GeminiAntiscamProtectionResponse>(
          result.response.value());
  if (!response) {
    base::UmaHistogramTimes(
        "SafeBrowsing.GeminiAntiscamProtection.FailedParsingError.Latency",
        latency);
    return;
  }
  base::UmaHistogramTimes(
      "SafeBrowsing.GeminiAntiscamProtection.Success.Latency", latency);

  bool scam_score_is_suspicious = false;
  if (response->has_scam_score()) {
    std::string content_category = GetContentCategory(response.value());
    base::UmaHistogramPercentage("SafeBrowsing.GeminiAntiscamProtection." +
                                     content_category + ".ScamScore",
                                 100 * response->scam_score());
    scam_score_is_suspicious =
        response->scam_score() >
        kGeminiAntiscamProtectionMinScamScoreLogPageContent.Get();
  }

  // Get model quality logs uploader service and log data to MQLS.
  auto* logs_uploader_service =
      optimization_guide_keyed_service_->GetModelQualityLogsUploaderService();
  if (!logs_uploader_service) {
    return;
  }
  LogsGeminiAntiscamProtectionMQLS(
      logs_uploader_service->GetWeakPtr(), metadata, url,
      scam_score_is_suspicious ? page_inner_text : "",
      response->has_scam_score() ? response->scam_score() : -1.0f,
      response->has_content_category() ? response->content_category() : "",
      response->has_justification() ? response->justification() : "");
}

}  // namespace safe_browsing

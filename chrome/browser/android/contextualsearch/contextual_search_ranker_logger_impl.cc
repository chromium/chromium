// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/android/contextualsearch/contextual_search_ranker_logger_impl.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/android/chrome_jni_headers/ContextualSearchRankerLoggerImpl_jni.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/assist_ranker/assist_ranker_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "components/assist_ranker/assist_ranker_service_impl.h"
#include "components/assist_ranker/binary_classifier_predictor.h"
#include "components/assist_ranker/predictor_config_definitions.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"

namespace content {
class BrowserContext;
}

namespace {

const char kContextualSearchRankerDidPredict[] = "OutcomeRankerDidPredict";
const char kContextualSearchRankerPrediction[] = "OutcomeRankerPrediction";
const char kContextualSearchImportantFeature[] = "DidOptIn";
const char kContextualSearchImportantOutcome[] = "OutcomeWasPanelOpened";
const char kContextualSearchRankerPredictionScore[] =
    "OutcomeRankerPredictionScore";

}  // namespace

ContextualSearchRankerLoggerImpl::ContextualSearchRankerLoggerImpl(JNIEnv* env,
                                                                   jobject obj)
    : java_object_(env, obj) {}

ContextualSearchRankerLoggerImpl::~ContextualSearchRankerLoggerImpl() {}

void ContextualSearchRankerLoggerImpl::SetupLoggingAndRanker(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  if (!web_contents)
    return;

  source_id_ = ukm::GetSourceIdForWebContentsDocument(web_contents);
  SetupRankerPredictor(*web_contents);
  // Start building example data based on features to be gathered and logged.
  ranker_example_ = std::make_unique<assist_ranker::RankerExample>();
}

void ContextualSearchRankerLoggerImpl::SetupRankerPredictor(
    content::WebContents& web_contents) {
  // Create one predictor for the current BrowserContext.
  if (browser_context_) {
    DCHECK(browser_context_ == web_contents.GetBrowserContext());
    return;
  }
  browser_context_ = web_contents.GetBrowserContext();

  assist_ranker::AssistRankerService* assist_ranker_service =
      assist_ranker::AssistRankerServiceFactory::GetForBrowserContext(
          browser_context_);
  if (assist_ranker_service) {
    predictor_ = assist_ranker_service->FetchBinaryClassifierPredictor(
        assist_ranker::GetContextualSearchPredictorConfig());
  }
}

void ContextualSearchRankerLoggerImpl::LogFeature(
    const std::string& feature_name,
    int value) {
  auto& features = *ranker_example_->mutable_features();
  features[feature_name].set_int32_value(value);
}

void ContextualSearchRankerLoggerImpl::LogInt32(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jstring>& j_feature,
    jint j_int) {
  std::string feature = base::android::ConvertJavaStringToUTF8(env, j_feature);
  LogFeature(feature, j_int);
}

AssistRankerPrediction ContextualSearchRankerLoggerImpl::RunInference(
    JNIEnv* env,
    jobject obj) {
  has_predicted_decision_ = true;
  bool prediction = false;
  bool was_able_to_predict = false;
  if (IsQueryEnabledInternal()) {
    was_able_to_predict = predictor_->Predict(*ranker_example_, &prediction);
    // Log to UMA whether we were able to predict or not.
    base::UmaHistogramBoolean("Search.ContextualSearch.Ranker.WasAbleToPredict",
                              was_able_to_predict);
    // TODO(chrome-ranker-team): this should be logged internally by Ranker.
    LogFeature(kContextualSearchRankerDidPredict,
               static_cast<int>(was_able_to_predict));
    if (was_able_to_predict) {
      LogFeature(kContextualSearchRankerPrediction,
                 static_cast<int>(prediction));
      // For offline validation also log the prediction score.
      // TODO(donnd): remove when https://crbug.com/914179 is resolved.
      float score;
      bool was_able_to_predict_score =
          predictor_->PredictScore(*ranker_example_, &score);
      if (was_able_to_predict_score)
        LogFeature(kContextualSearchRankerPredictionScore, score);
    }
  }
  AssistRankerPrediction prediction_enum;
  if (was_able_to_predict) {
    if (prediction) {
      prediction_enum = ASSIST_RANKER_PREDICTION_SHOW;
    } else {
      prediction_enum = ASSIST_RANKER_PREDICTION_SUPPRESS;
    }
  } else {
    prediction_enum = ASSIST_RANKER_PREDICTION_UNAVAILABLE;
  }
  // TODO(donnd): remove when behind-the-flag bug fixed (crbug.com/786589).
  DVLOG(0) << "prediction: " << prediction_enum;
  return prediction_enum;
}

void ContextualSearchRankerLoggerImpl::WriteLogAndReset(JNIEnv* env,
                                                        jobject obj) {
  if (predictor_ && ranker_example_ && source_id_ != ukm::kInvalidSourceId) {
    predictor_->LogExampleToUkm(*ranker_example_.get(), source_id_);
    source_id_ = ukm::kInvalidSourceId;
    logImportantFeaturePresent(kContextualSearchImportantFeature, false);
    logImportantFeaturePresent(kContextualSearchImportantOutcome, true);
  }
  has_predicted_decision_ = false;
  ranker_example_ = std::make_unique<assist_ranker::RankerExample>();
}

void ContextualSearchRankerLoggerImpl::logImportantFeaturePresent(
    const std::string& feature,
    bool is_outcome) const {
  if (ranker_example_->features().count(feature) > 0) {
    DCHECK(ranker_example_->features().count(feature) < 2);
    base::UmaHistogramBoolean("Search.ContextualSearch.Ranker.RecordedNative",
                              is_outcome);
  }
}

bool ContextualSearchRankerLoggerImpl::IsQueryEnabled(JNIEnv* env,
                                                      jobject obj) {
  return IsQueryEnabledInternal();
}

bool ContextualSearchRankerLoggerImpl::IsQueryEnabledInternal() {
  return predictor_ && predictor_->is_query_enabled();
}

// Java wrapper boilerplate

void ContextualSearchRankerLoggerImpl::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  delete this;
}

jlong JNI_ContextualSearchRankerLoggerImpl_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  ContextualSearchRankerLoggerImpl* ranker_logger_impl =
      new ContextualSearchRankerLoggerImpl(env, obj);
  return reinterpret_cast<intptr_t>(ranker_logger_impl);
}

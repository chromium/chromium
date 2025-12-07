// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/observer_list.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace {

void WritePredictionToConsoleLog(
    const NavigationPredictorKeyedService::Prediction& prediction) {
  if (!prediction.web_contents())
    return;

  base::Value::Dict message;

  base::Value::List url_list;
  for (const GURL& url : prediction.sorted_predicted_urls()) {
    url_list.Append(url.spec());
  }

  message.Set("predictions", std::move(url_list));
  if (prediction.source_document_url()) {
    message.Set("source_url", prediction.source_document_url()->spec());
  }

  std::string json_body;
  if (!base::JSONWriter::Write(message, &json_body)) {
    NOTREACHED();
  }

  prediction.web_contents()->GetPrimaryMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kInfo,
      "JSON Navigation Prediction: " + json_body);
}

}  // namespace

NavigationPredictorKeyedService::Prediction::Prediction(
    content::WebContents* web_contents,
    const std::optional<GURL>& source_document_url,
    PredictionSource prediction_source,
    const std::vector<GURL>& sorted_predicted_urls)
    : web_contents_(web_contents),
      source_document_url_(source_document_url),
      prediction_source_(prediction_source),
      sorted_predicted_urls_(sorted_predicted_urls) {
  DCHECK_EQ(prediction_source_,
            PredictionSource::kAnchorElementsParsedFromWebPage);
  DCHECK(!source_document_url->is_empty());
}

NavigationPredictorKeyedService::Prediction::Prediction(
    const NavigationPredictorKeyedService::Prediction& other)
    : web_contents_(other.web_contents_),
      prediction_source_(other.prediction_source_) {
  // Use non-default copy constructor operator that does deep-copy.
  source_document_url_ = other.source_document_url_;

  sorted_predicted_urls_.reserve(other.sorted_predicted_urls_.size());
  for (const auto& entry : other.sorted_predicted_urls_)
    sorted_predicted_urls_.push_back(entry);
}

NavigationPredictorKeyedService::Prediction&
NavigationPredictorKeyedService::Prediction::operator=(
    const NavigationPredictorKeyedService::Prediction& other) {
  // Use non-default assignment operator that does deep-copy.
  web_contents_ = other.web_contents_;

  source_document_url_.reset();
  source_document_url_ = other.source_document_url_;
  prediction_source_ = other.prediction_source_;

  sorted_predicted_urls_.clear();
  sorted_predicted_urls_.reserve(other.sorted_predicted_urls_.size());
  for (const auto& entry : other.sorted_predicted_urls_)
    sorted_predicted_urls_.push_back(entry);

  return *this;
}

NavigationPredictorKeyedService::Prediction::~Prediction() = default;

const std::optional<GURL>&
NavigationPredictorKeyedService::Prediction::source_document_url() const {
  DCHECK_EQ(PredictionSource::kAnchorElementsParsedFromWebPage,
            prediction_source_);
  return source_document_url_;
}

const std::vector<GURL>&
NavigationPredictorKeyedService::Prediction::sorted_predicted_urls() const {
  return sorted_predicted_urls_;
}

content::WebContents*
NavigationPredictorKeyedService::Prediction::web_contents() const {
  DCHECK_EQ(PredictionSource::kAnchorElementsParsedFromWebPage,
            prediction_source_);
  return web_contents_;
}

NavigationPredictorKeyedService::NavigationPredictorKeyedService(
    content::BrowserContext* browser_context)
    : search_engine_preconnector_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!browser_context->IsOffTheRecord());

#if !BUILDFLAG(IS_ANDROID)
  if (!SearchEnginePreconnector::ShouldBeEnabledAsKeyedService()) {
    // Start preconnecting to the search engine.
    search_engine_preconnector_.StartPreconnecting(/*with_startup_delay=*/true);
  }
#endif
}

NavigationPredictorKeyedService::~NavigationPredictorKeyedService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void NavigationPredictorKeyedService::OnPredictionUpdated(
    content::WebContents* web_contents,
    const GURL& document_url,
    PredictionSource prediction_source,
    const std::vector<GURL>& sorted_predicted_urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Currently, this method is called only for anchor elements parsed from
  // webpage.
  DCHECK_EQ(PredictionSource::kAnchorElementsParsedFromWebPage,
            prediction_source);

  last_prediction_ = Prediction(web_contents, document_url, prediction_source,
                                sorted_predicted_urls);

  for (auto& observer : observer_list_) {
    observer.OnPredictionUpdated(last_prediction_.value());
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "console-log-json-navigation-predictions-for-testing")) {
    WritePredictionToConsoleLog(last_prediction_.value());
  }
}

void NavigationPredictorKeyedService::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.AddObserver(observer);
  if (last_prediction_.has_value()) {
    observer->OnPredictionUpdated(last_prediction_.value());
  }
}

void NavigationPredictorKeyedService::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

SearchEnginePreconnector*
NavigationPredictorKeyedService::search_engine_preconnector() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return &search_engine_preconnector_;
}

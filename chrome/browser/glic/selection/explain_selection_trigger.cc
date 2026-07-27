// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/explain_selection_trigger.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace glic {

ExplainSelectionTrigger::ExplainSelectionTrigger() = default;

ExplainSelectionTrigger::~ExplainSelectionTrigger() = default;

// static
std::string ExplainSelectionTrigger::GetPromptTemplate() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch("glic-inline-prompt")) {
    return command_line->GetSwitchValueASCII("glic-inline-prompt");
  }
  if (command_line->HasSwitch("glic-inline-prompt-file")) {
    base::FilePath prompt_file =
        command_line->GetSwitchValuePath("glic-inline-prompt-file");
    std::string file_content;
    if (base::ReadFileToString(prompt_file, &file_content)) {
      return std::string(
          base::TrimWhitespaceASCII(file_content, base::TRIM_ALL));
    }
  }

  std::string param_prompt =
      features::kGlicSelectionPromptInlinePromptTemplate.Get();
  if (!param_prompt.empty()) {
    return param_prompt;
  }

  // TODO(b/539514187): Load default prompt template from Chrome branded and
  // translated resource strings when Finch parameter is not specified.
  return "";
}

// static
std::string ExplainSelectionTrigger::GetGeminiApiKey() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch("glic-gemini-api-key")) {
    return command_line->GetSwitchValueASCII("glic-gemini-api-key");
  }
  return "";
}

// static
bool ExplainSelectionTrigger::IsInlineFulfillmentSupported() {
  if (!features::kGlicSelectionPromptInlineFulfillment.Get()) {
    return false;
  }
  return !GetPromptTemplate().empty();
}

void ExplainSelectionTrigger::RequestExplanation(
    content::WebContents* web_contents,
    const std::string& selected_text,
    const std::string& surrounding_text,
    StreamUpdateCallback callback) {
  if (!web_contents || !callback) {
    if (callback) {
      callback.Run("", true, "Invalid web contents");
    }
    return;
  }

  if (!IsInlineFulfillmentSupported()) {
    callback.Run("", true,
                 "Inline fulfillment disabled or private prompt resource "
                 "unavailable");
    return;
  }

  start_time_ = base::TimeTicks::Now();

  // TODO(b/539511437): Evaluate whether enterprise Data Loss Prevention (DLP)
  // restrictions apply before sending selected text to the Gemini model.

  SendGeminiApiRequest(web_contents, selected_text, surrounding_text,
                       callback);
}

void ExplainSelectionTrigger::SendGeminiApiRequest(
    content::WebContents* web_contents,
    const std::string& selected_text,
    const std::string& surrounding_text,
    StreamUpdateCallback callback) {
  std::string api_key = GetGeminiApiKey();

  // Construct Gemini API generateContent payload
  std::string prompt_template = GetPromptTemplate();
  std::string formatted_prompt = prompt_template;
  size_t pos = formatted_prompt.find("$1");
  if (pos != std::string::npos) {
    formatted_prompt.replace(pos, 2, selected_text);
  } else {
    formatted_prompt += "\n" + selected_text;
  }

  size_t pos2 = formatted_prompt.find("$2");
  if (pos2 != std::string::npos) {
    formatted_prompt.replace(pos2, 2, surrounding_text);
  }

  base::DictValue payload;
  base::ListValue contents;
  base::DictValue user_content;
  user_content.Set("role", "user");

  base::ListValue parts;
  base::DictValue text_part;
  text_part.Set("text", formatted_prompt);
  parts.Append(std::move(text_part));

  user_content.Set("parts", std::move(parts));
  contents.Append(std::move(user_content));
  payload.Set("contents", std::move(contents));

  base::DictValue gen_config;
  gen_config.Set("temperature", 0.2);
  gen_config.Set("maxOutputTokens", 500);
  payload.Set("generationConfig", std::move(gen_config));

  std::string request_body;
  base::JSONWriter::Write(payload, &request_body);

  std::string model_name = "gemini-flash-lite-latest";
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch("glic-gemini-model")) {
    model_name = command_line->GetSwitchValueASCII("glic-gemini-model");
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  std::string url_str =
      "https://generativelanguage.googleapis.com/v1beta/models/" + model_name +
      ":generateContent";

  if (!api_key.empty()) {
    url_str += "?key=" + api_key;
    resource_request->headers.SetHeader("X-Goog-Api-Key", api_key);
  }

  resource_request->url = GURL(url_str);
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("glic_selection_ask_gemini_api", R"(
        semantics {
          sender: "Gemini in Chrome"
          description:
            "Sends selected text on a web page to the Gemini API to request "
            "an inline explanation response rendered directly on the selection "
            "widget."
          trigger: "User clicks the Ask Gemini button on text selection widget."
          data:
            "The text selected by the user on the web page and the prompt "
            "template used for generating the inline explanation."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              owners: "//chrome/browser/glic/OWNERS"
            }
          }
          user_data {
            type: USER_CONTENT
            type: WEB_CONTENT
            type: ACCESS_TOKEN
          }
          last_reviewed: "2026-07-23"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature via Chrome flags or "
            "settings for Glic selection prompt features."
          chrome_policy {
            GeminiSettings {
              GeminiSettings: 1
            }
            GenAiDefaultSettings {
              GenAiDefaultSettings: 2
            }
          }
        })");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(request_body, "application/json");
  url_loader_->SetAllowHttpErrorResults(true);

  auto url_loader_factory =
      web_contents->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  constexpr size_t kMaxGeminiResponseSizeBytes = 1024 * 1024;  // 1 MB
  url_loader_->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&ExplainSelectionTrigger::OnGeminiApiResponse,
                     weak_ptr_factory_.GetWeakPtr(), callback, selected_text),
      kMaxGeminiResponseSizeBytes);
}

void ExplainSelectionTrigger::OnGeminiApiResponse(
    StreamUpdateCallback callback,
    std::string selected_text,
    std::optional<std::string> response_body) {
  base::TimeDelta latency = base::TimeTicks::Now() - start_time_;
  base::UmaHistogramTimes(
      "OptimizationGuide.ModelExecution.ExplainSelection.TTFT", latency);
  base::UmaHistogramTimes(
      "OptimizationGuide.ModelExecution.ExplainSelection.TotalLatency",
      latency);

  if (!response_body || response_body->empty()) {
    callback.Run(/*text=*/"", /*is_complete=*/true,
                 /*error_message=*/l10n_util::GetStringUTF8(IDS_GLIC_ERROR_NOTICE));
    return;
  }

  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(*response_body, base::JSON_PARSE_RFC);
  if (parsed) {
    const base::ListValue* candidates = parsed->FindList("candidates");
    if (candidates && !candidates->empty() && (*candidates)[0].is_dict()) {
      const base::DictValue& first_candidate = (*candidates)[0].GetDict();
      const base::DictValue* content = first_candidate.FindDict("content");
      if (content) {
        const base::ListValue* parts = content->FindList("parts");
        if (parts && !parts->empty() && (*parts)[0].is_dict()) {
          const std::string* text = (*parts)[0].GetDict().FindString("text");
          if (text && !text->empty()) {
            callback.Run(*text, /*is_complete=*/true, /*error_message=*/"");
            return;
          }
        }
      }
    }

    const base::DictValue* error_dict = parsed->FindDict("error");
    if (error_dict) {
      const std::string* err_msg = error_dict->FindString("message");
      if (err_msg && !err_msg->empty()) {
        std::string err_out = "**Gemini API Error:** " + *err_msg;
        callback.Run(err_out, /*is_complete=*/true, /*error_message=*/"");
        return;
      }
    }
  }

  callback.Run(/*text=*/"", /*is_complete=*/true,
               /*error_message=*/l10n_util::GetStringUTF8(IDS_GLIC_ERROR_NOTICE));
}

}  // namespace glic

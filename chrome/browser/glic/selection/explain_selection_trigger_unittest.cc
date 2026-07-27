// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/explain_selection_trigger.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class ExplainSelectionTriggerTest : public ChromeRenderViewHostTestHarness {
 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ExplainSelectionTriggerTest, IsInlineFulfillmentSupportedDisabledByDefault) {
  feature_list_.InitAndDisableFeature(features::kGlicSelectionPrompt);
  EXPECT_FALSE(ExplainSelectionTrigger::IsInlineFulfillmentSupported());
}

TEST_F(ExplainSelectionTriggerTest, CustomPromptTemplateParamEnablesInlineFulfillment) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionPrompt,
      {{"inline_fulfillment", "true"},
       {"inline_prompt_template", "Custom test prompt template"}});

  EXPECT_TRUE(ExplainSelectionTrigger::IsInlineFulfillmentSupported());
  EXPECT_EQ(ExplainSelectionTrigger::GetPromptTemplate(),
            "Custom test prompt template");
}

TEST_F(ExplainSelectionTriggerTest, DefaultFinchBehavior) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionPrompt, {{"inline_fulfillment", "true"}});

  // Without an explicit inline_prompt_template param or switch, prompt template is empty.
  EXPECT_FALSE(ExplainSelectionTrigger::IsInlineFulfillmentSupported());
  EXPECT_TRUE(ExplainSelectionTrigger::GetPromptTemplate().empty());
}

TEST_F(ExplainSelectionTriggerTest, CommandLineSwitchPromptOverride) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionPrompt, {{"inline_fulfillment", "true"}});

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "glic-inline-prompt", "Runtime prompt switch test");

  EXPECT_TRUE(ExplainSelectionTrigger::IsInlineFulfillmentSupported());
  EXPECT_EQ(ExplainSelectionTrigger::GetPromptTemplate(),
            "Runtime prompt switch test");
}

TEST_F(ExplainSelectionTriggerTest, GeminiApiKeyCommandLineSwitch) {
  EXPECT_TRUE(ExplainSelectionTrigger::GetGeminiApiKey().empty());

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "glic-gemini-api-key", "CUSTOM_TEST_API_KEY");

  EXPECT_EQ(ExplainSelectionTrigger::GetGeminiApiKey(), "CUSTOM_TEST_API_KEY");
}

TEST_F(ExplainSelectionTriggerTest, RequestExplanationInvalidInputs) {
  ExplainSelectionTrigger trigger;
  bool callback_run = false;
  trigger.RequestExplanation(
      nullptr, "selected text", "surrounding text",
      base::BindLambdaForTesting([&](const std::string& explanation,
                                     bool is_complete,
                                     const std::string& error_message) {
        callback_run = true;
        EXPECT_EQ(error_message, "Invalid web contents");
      }));
  EXPECT_TRUE(callback_run);
}

TEST_F(ExplainSelectionTriggerTest, RequestExplanationSuccessfulResponse) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionPrompt,
      {{"inline_fulfillment", "true"},
       {"inline_prompt_template", "Explain text: $1"}});

  std::string json_response = R"({
    "candidates": [{
      "content": {
        "parts": [{
          "text": "Quantum computing processes information using qubits."
        }]
      }
    }]
  })";

  content::URLLoaderInterceptor interceptor(
      base::BindLambdaForTesting([&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "generativelanguage.googleapis.com") {
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-Type: application/json\n\n",
              json_response, params->client.get());
          return true;
        }
        return false;
      }));

  ExplainSelectionTrigger trigger;
  std::string received_explanation;
  bool is_complete_received = false;

  base::RunLoop run_loop;
  trigger.RequestExplanation(
      web_contents(), "quantum computing", "context",
      base::BindLambdaForTesting([&](const std::string& explanation,
                                     bool is_complete,
                                     const std::string& error_message) {
        received_explanation = explanation;
        is_complete_received = is_complete;
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_TRUE(is_complete_received);
  EXPECT_EQ(received_explanation,
            "Quantum computing processes information using qubits.");
}

TEST_F(ExplainSelectionTriggerTest, RequestExplanationErrorResponse) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionPrompt,
      {{"inline_fulfillment", "true"},
       {"inline_prompt_template", "Explain text: $1"}});

  std::string json_error = R"({
    "error": {
      "message": "Resource quota exceeded"
    }
  })";

  content::URLLoaderInterceptor interceptor(
      base::BindLambdaForTesting([&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "generativelanguage.googleapis.com") {
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-Type: application/json\n\n",
              json_error, params->client.get());
          return true;
        }
        return false;
      }));

  ExplainSelectionTrigger trigger;
  std::string received_explanation;

  base::RunLoop run_loop;
  trigger.RequestExplanation(
      web_contents(), "test input", "context",
      base::BindLambdaForTesting([&](const std::string& explanation,
                                     bool is_complete,
                                     const std::string& error_message) {
        received_explanation = explanation;
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_EQ(received_explanation, "**Gemini API Error:** Resource quota exceeded");
}

TEST_F(ExplainSelectionTriggerTest, RequestExplanationEmptyResponse) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionPrompt,
      {{"inline_fulfillment", "true"},
       {"inline_prompt_template", "Explain text: $1"}});

  content::URLLoaderInterceptor interceptor(
      base::BindLambdaForTesting([&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "generativelanguage.googleapis.com") {
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-Type: application/json\n\n",
              "", params->client.get());
          return true;
        }
        return false;
      }));

  ExplainSelectionTrigger trigger;
  std::string received_error;

  base::RunLoop run_loop;
  trigger.RequestExplanation(
      web_contents(), "fallback test", "context",
      base::BindLambdaForTesting([&](const std::string& explanation,
                                     bool is_complete,
                                     const std::string& error_message) {
        received_error = error_message;
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_FALSE(received_error.empty());
}

}  // namespace glic

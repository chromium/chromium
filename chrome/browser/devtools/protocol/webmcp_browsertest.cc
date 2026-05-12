// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "components/actor/core/actor_features.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"

#if (!BUILDFLAG(IS_ANDROID))
namespace {
class TestDevToolsClient : public ::content::DevToolsAgentHostClient {
 public:
  TestDevToolsClient() = default;
  ~TestDevToolsClient() override { Detach(); }

  void WaitForInvokedEvent() {
    ASSERT_TRUE(
        base::test::RunUntil([this]() { return !invoked_events_.empty(); }));
  }

  void WaitForRespondedEvent() {
    ASSERT_TRUE(
        base::test::RunUntil([this]() { return !responded_events_.empty(); }));
  }

  void AttachToAndEnableWebMCP(
      scoped_refptr<content::DevToolsAgentHost> agent_host) {
    agent_host_ = agent_host;
    agent_host_->AttachClient(this);

    const std::string enable_message =
        R"JSON({"id": 1, "method": "WebMCP.enable"})JSON";
    agent_host_->DispatchProtocolMessage(this,
                                         base::as_byte_span(enable_message));
  }

  void Detach() {
    if (agent_host_) {
      agent_host_->DetachClient(this);
      agent_host_ = nullptr;
    }
  }

  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    std::string_view message_str(reinterpret_cast<const char*>(message.data()),
                                 message.size());
    std::optional<base::Value> parsed = base::test::ParseJson(message_str);
    if (!parsed || !parsed->is_dict()) {
      return;
    }

    const base::DictValue& dict = parsed->GetDict();

    const std::string* method = dict.FindString("method");
    if (!method) {
      return;
    }

    if (*method == "WebMCP.toolInvoked") {
      invoked_events_.push_back(std::move(*parsed));
    } else if (*method == "WebMCP.toolResponded") {
      responded_events_.push_back(std::move(*parsed));
    }
  }

  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}

  const std::vector<base::Value>& invoked_events() const {
    return invoked_events_;
  }
  const std::vector<base::Value>& responded_events() const {
    return responded_events_;
  }

 private:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::vector<base::Value> invoked_events_;
  std::vector<base::Value> responded_events_;
};

class DevToolsScriptToolTest : public actor::ActorToolsTest,
                               public testing::WithParamInterface<bool> {
 public:
  DevToolsScriptToolTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        blink::features::kWebMCP, blink::features::kDevToolsWebMCPSupport,
        actor::kGlicActorEnableScriptTools};
    std::vector<base::test::FeatureRef> disabled_features;

    if (GetParam()) {
      enabled_features.push_back(::features::kBackForwardCache);
    } else {
      disabled_features.push_back(::features::kBackForwardCache);
    }

    features_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUpOnMainThread() override {
    actor::ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  struct ToolResult {
    actor::mojom::ActionResultPtr action_result;
    actor::mojom::ScriptToolResponsePtr response;
  };

  ToolResult RunScriptTool(std::unique_ptr<actor::ToolRequest> action) {
    actor::ActResultFuture result;
    actor_task().Act(actor::ToRequestList(std::move(action)),
                     result.GetCallback());
    ExpectOkResult(result);

    const auto& action_results = result.Get();
    EXPECT_EQ(action_results.size(), 1u);
    EXPECT_TRUE(action_results.at(0).result);
    actor::mojom::ActionResultPtr action_result =
        action_results.at(0).result->Clone();
    actor::mojom::ScriptToolResponsePtr response =
        std::move(action_results.at(0).result->script_tool_response);
    EXPECT_TRUE(response);
    return {std::move(action_result), std::move(response)};
  }

 private:
  base::test::ScopedFeatureList features_;
};
}  // namespace

IN_PROC_BROWSER_TEST_P(DevToolsScriptToolTest, EmitsCdpEvents) {
  const GURL url = embedded_test_server()->GetURL("/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestDevToolsClient client;
  client.AttachToAndEnableWebMCP(
      content::DevToolsAgentHost::GetOrCreateFor(web_contents()));

  const std::string input_arguments = R"JSON({"text": "test_input"})JSON";
  auto action =
      actor::MakeScriptToolRequest(*main_frame(), "echo", input_arguments);
  auto [action_result, response] = RunScriptTool(std::move(action));

  EXPECT_EQ(response->result, "test_input");

  client.WaitForInvokedEvent();
  ASSERT_EQ(client.invoked_events().size(), 1u);
  const base::DictValue& invoked_event = client.invoked_events()[0].GetDict();
  const base::DictValue* invoked_params = invoked_event.FindDict("params");
  ASSERT_TRUE(invoked_params);
  const std::string* tool_name = invoked_params->FindString("toolName");
  ASSERT_TRUE(tool_name);
  EXPECT_EQ(*tool_name, "echo");
  const std::string* input = invoked_params->FindString("input");
  ASSERT_TRUE(input);
  EXPECT_EQ(*input, input_arguments);
  const std::string* invocation_id = invoked_params->FindString("invocationId");
  ASSERT_TRUE(invocation_id);

  client.WaitForRespondedEvent();
  ASSERT_EQ(client.responded_events().size(), 1u);
  const base::DictValue& responded_event =
      client.responded_events()[0].GetDict();
  const base::DictValue* responded_params = responded_event.FindDict("params");
  ASSERT_TRUE(responded_params);
  const std::string* responded_invocation_id =
      responded_params->FindString("invocationId");
  ASSERT_TRUE(responded_invocation_id);
  EXPECT_EQ(*responded_invocation_id, *invocation_id);
  const std::string* status = responded_params->FindString("status");
  ASSERT_TRUE(status);
  EXPECT_EQ(*status, "Completed");

  client.Detach();
}

IN_PROC_BROWSER_TEST_P(DevToolsScriptToolTest, EmitsCdpEventsOnFailure) {
  const GURL url = embedded_test_server()->GetURL("/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestDevToolsClient client;
  client.AttachToAndEnableWebMCP(
      content::DevToolsAgentHost::GetOrCreateFor(web_contents()));

  const std::string input_arguments = R"JSON({"text": "test_input"})JSON";
  auto action =
      actor::MakeScriptToolRequest(*main_frame(), "invalid", input_arguments);
  actor::ActResultFuture result;
  actor_task().Act(actor::ToRequestList(std::move(action)),
                   result.GetCallback());
  ExpectErrorResult(result,
                    actor::mojom::ActionResultCode::kScriptToolInvalidName);

  client.WaitForInvokedEvent();
  ASSERT_EQ(client.invoked_events().size(), 1u);
  const base::DictValue& invoked_event = client.invoked_events()[0].GetDict();
  const base::DictValue* invoked_params = invoked_event.FindDict("params");
  ASSERT_TRUE(invoked_params);
  const std::string* tool_name = invoked_params->FindString("toolName");
  ASSERT_TRUE(tool_name);
  EXPECT_EQ(*tool_name, "invalid");
  const std::string* input = invoked_params->FindString("input");
  ASSERT_TRUE(input);
  EXPECT_EQ(*input, input_arguments);
  const std::string* invocation_id = invoked_params->FindString("invocationId");
  ASSERT_TRUE(invocation_id);

  client.WaitForRespondedEvent();
  ASSERT_EQ(client.responded_events().size(), 1u);
  const base::DictValue& responded_event =
      client.responded_events()[0].GetDict();
  const base::DictValue* responded_params = responded_event.FindDict("params");
  ASSERT_TRUE(responded_params);
  const std::string* responded_invocation_id =
      responded_params->FindString("invocationId");
  ASSERT_TRUE(responded_invocation_id);
  EXPECT_EQ(*responded_invocation_id, *invocation_id);
  const std::string* status = responded_params->FindString("status");
  ASSERT_TRUE(status);
  EXPECT_EQ(*status, "Error");
  const std::string* error_text = responded_params->FindString("errorText");
  ASSERT_TRUE(error_text);
  EXPECT_EQ(*error_text, "Tool not found: invalid");

  client.Detach();
}

IN_PROC_BROWSER_TEST_P(DevToolsScriptToolTest, EmitsCdpEventsDeclarativeTool) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/declarative_script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestDevToolsClient client;
  client.AttachToAndEnableWebMCP(
      content::DevToolsAgentHost::GetOrCreateFor(web_contents()));

  const std::string declarative_input =
      R"JSON(
        {
          "text": "text #1",
          "text2": "text #2",
          "select": "Option 2"
        }
      )JSON";
  auto action = actor::MakeScriptToolRequest(*main_frame(), "declarative_tool",
                                             declarative_input);
  auto [action_result, response] = RunScriptTool(std::move(action));

  client.WaitForInvokedEvent();
  ASSERT_EQ(client.invoked_events().size(), 1u);
  const base::DictValue& invoked_event = client.invoked_events()[0].GetDict();
  const base::DictValue* invoked_params = invoked_event.FindDict("params");
  ASSERT_TRUE(invoked_params);
  const std::string* tool_name = invoked_params->FindString("toolName");
  ASSERT_TRUE(tool_name);
  EXPECT_EQ(*tool_name, "declarative_tool");
  const std::string* input = invoked_params->FindString("input");
  ASSERT_TRUE(input);
  // Compare parsed JSON to ignore whitespace differences
  std::optional<base::Value> parsed_input = base::test::ParseJson(*input);
  std::optional<base::Value> parsed_expected_input =
      base::test::ParseJson(declarative_input);
  ASSERT_TRUE(parsed_input);
  ASSERT_TRUE(parsed_expected_input);
  EXPECT_EQ(*parsed_input, *parsed_expected_input);
  const std::string* invocation_id = invoked_params->FindString("invocationId");
  ASSERT_TRUE(invocation_id);

  client.WaitForRespondedEvent();
  ASSERT_EQ(client.responded_events().size(), 1u);
  const base::DictValue& responded_event =
      client.responded_events()[0].GetDict();
  const base::DictValue* responded_params = responded_event.FindDict("params");
  ASSERT_TRUE(responded_params);
  const std::string* responded_invocation_id =
      responded_params->FindString("invocationId");
  ASSERT_TRUE(responded_invocation_id);
  EXPECT_EQ(*responded_invocation_id, *invocation_id);
  const std::string* status = responded_params->FindString("status");
  ASSERT_TRUE(status);
  EXPECT_EQ(*status, "Completed");

  client.Detach();
}

IN_PROC_BROWSER_TEST_P(DevToolsScriptToolTest,
                       EmitsCdpEventsDeclarativeToolCrossDocument) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/declarative_script_tool_cross_document.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestDevToolsClient client;
  client.AttachToAndEnableWebMCP(
      content::DevToolsAgentHost::GetOrCreateFor(web_contents()));

  const std::string declarative_input =
      R"JSON(
        {
          "echo": "hello world"
        }
      )JSON";
  auto action = actor::MakeScriptToolRequest(*main_frame(), "declarative_tool",
                                             declarative_input);

  auto [action_result, response] = RunScriptTool(std::move(action));
  ASSERT_TRUE(response);
  EXPECT_EQ(response->input_arguments, declarative_input);
  EXPECT_EQ(response->tool->name, "declarative_tool");

  client.WaitForInvokedEvent();
  ASSERT_EQ(client.invoked_events().size(), 1u);
  const base::DictValue& invoked_event = client.invoked_events()[0].GetDict();
  const base::DictValue* invoked_params = invoked_event.FindDict("params");
  ASSERT_TRUE(invoked_params);
  const std::string* tool_name = invoked_params->FindString("toolName");
  ASSERT_TRUE(tool_name);
  EXPECT_EQ(*tool_name, "declarative_tool");
  const std::string* input = invoked_params->FindString("input");
  ASSERT_TRUE(input);
  EXPECT_EQ(*input, declarative_input);
  const std::string* invocation_id = invoked_params->FindString("invocationId");
  ASSERT_TRUE(invocation_id);

  // Wait for the toolResponded event instead of asserting it doesn't exist.
  client.WaitForRespondedEvent();
  ASSERT_EQ(client.responded_events().size(), 1u);
  const base::DictValue& responded_event =
      client.responded_events()[0].GetDict();
  const base::DictValue* responded_params = responded_event.FindDict("params");
  ASSERT_TRUE(responded_params);
  const std::string* responded_invocation_id =
      responded_params->FindString("invocationId");
  ASSERT_TRUE(responded_invocation_id);
  EXPECT_EQ(*responded_invocation_id, *invocation_id);
  const std::string* status = responded_params->FindString("status");
  ASSERT_TRUE(status);
  EXPECT_EQ(*status, "Completed");

  client.Detach();
}

INSTANTIATE_TEST_SUITE_P(All,
                         DevToolsScriptToolTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "BFCacheEnabled"
                                             : "BFCacheDisabled";
                         });

#endif  // !BUILDFLAG(IS_ANDROID)

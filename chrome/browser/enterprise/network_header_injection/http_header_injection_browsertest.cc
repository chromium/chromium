// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/enterprise/network_header_injection/core/features.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_rule.h"
#include "components/policy/core/common/policy_map.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/install_default_websocket_handlers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_custom_headers {

class HttpHeaderInjectionBrowserTest : public policy::PolicyTest {
 protected:
  struct TestRule {
    std::vector<std::string> patterns;
    std::vector<std::pair<std::string, std::string>> headers;
  };

  HttpHeaderInjectionBrowserTest() {
    feature_list_.InitAndEnableFeature(kHttpHeadersInjection);
  }
  ~HttpHeaderInjectionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    policy::PolicyTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HttpHeaderInjectionBrowserTest::HandleRequest,
                            base::Unretained(this), "/match"));

    net::test_server::InstallDefaultWebSocketHandlers(embedded_test_server());

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      std::string_view handle_path,
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != handle_path) {
      return nullptr;
    }

    last_request_headers_ = request.headers;

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content("OK");
    return response;
  }

  void SetPolicy(const std::vector<TestRule>& rules) {
    base::ListValue rules_list;
    for (const auto& test_rule : rules) {
      base::DictValue rule_dict;

      base::ListValue pattern_list;
      for (const auto& pattern : test_rule.patterns) {
        pattern_list.Append(pattern);
      }
      rule_dict.Set(kKeyPatterns, std::move(pattern_list));

      base::ListValue header_list;
      for (const auto& [name, value] : test_rule.headers) {
        base::DictValue header_dict;
        header_dict.Set(kKeyHeaderName, name);
        header_dict.Set(kKeyHeaderValue, value);
        header_list.Append(std::move(header_dict));
      }
      rule_dict.Set(kKeyHeaders, std::move(header_list));

      rules_list.Append(std::move(rule_dict));
    }

    policy::PolicyMap policies;
    policies.Set("HttpHeaderInjection", policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                 base::Value(std::move(rules_list)), nullptr);
    UpdateProviderPolicy(policies);
  }

  void CheckHeaderValuePresent(const std::string& name,
                               const std::string& value) {
    auto it = last_request_headers_.find(name);
    ASSERT_TRUE(it != last_request_headers_.end())
        << "Header " << name << " not found";
    EXPECT_EQ(it->second, value);
  }

  void CheckHeaderNotPresent(const std::string& name) {
    auto it = last_request_headers_.find(name);
    EXPECT_TRUE(it == last_request_headers_.end())
        << "Header " << name << " unexpectedly found with value "
        << (it != last_request_headers_.end() ? it->second : "");
  }

  scoped_refptr<const extensions::Extension> LoadExtensionWithHeaderRules(
      const std::string& resource_type) {
    extensions::TestExtensionDir test_dir;
    test_dir.WriteManifest(R"(
      {
        "name": "Test Extension",
        "version": "1.0",
        "manifest_version": 3,
        "permissions": ["declarativeNetRequest"],
        "host_permissions": ["<all_urls>"],
        "declarative_net_request": {
          "rule_resources": [{
            "id": "ruleset_1",
            "enabled": true,
            "path": "rules.json"
          }]
        }
      }
    )");
    test_dir.WriteFile(
        FILE_PATH_LITERAL("rules.json"),
        base::ReplaceStringPlaceholders(R"(
      [{
        "id": 1,
        "priority": 1,
        "action": {
          "type": "modifyHeaders",
          "requestHeaders": [{
            "header": "X-Extension-Header",
            "operation": "set",
            "value": "ExtensionValue"
          }, {
            "header": "X-Test-Header",
            "operation": "set",
            "value": "ExtensionValue"
          }]
        },
        "condition": {
          "urlFilter": "*",
          "resourceTypes": ["$1"]
        }
      }]
    )",
                                        {resource_type}, nullptr));

    extensions::ChromeTestExtensionLoader loader(browser()->GetProfile());

    auto* rules_monitor_service =
        extensions::declarative_net_request::RulesMonitorService::Get(
            browser()->GetProfile());
    extensions::declarative_net_request::RulesetManagerObserver ruleset_waiter(
        rules_monitor_service->ruleset_manager());

    scoped_refptr<const extensions::Extension> extension =
        loader.LoadExtension(test_dir.UnpackedPath());

    ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

    return extension;
  }

  base::test::ScopedFeatureList feature_list_;
  net::test_server::HttpRequest::HeaderMap last_request_headers_;
};

// Tests that a header is correctly injected when the request URL matches the
// configured domain.
IN_PROC_BROWSER_TEST_F(HttpHeaderInjectionBrowserTest, BasicInjection) {
  SetPolicy({{.patterns = {"example.com"},
              .headers = {{"X-Test-Header", "TestValue"}}}});

  GURL url = embedded_test_server()->GetURL("example.com", "/match");
  ASSERT_TRUE(NavigateToUrl(url, this));

  CheckHeaderValuePresent("X-Test-Header", "TestValue");
}

// Tests that no header is injected when the request URL does not match any
// configured domain.
IN_PROC_BROWSER_TEST_F(HttpHeaderInjectionBrowserTest, NoMatch) {
  SetPolicy({{.patterns = {"example.com"},
              .headers = {{"X-Test-Header", "TestValue"}}}});

  GURL url = embedded_test_server()->GetURL("not-example.com", "/match");
  ASSERT_TRUE(NavigateToUrl(url, this));

  CheckHeaderNotPresent("X-Test-Header");
}

// Tests that a rule with multiple domains correctly injects headers for all of
// them.
IN_PROC_BROWSER_TEST_F(HttpHeaderInjectionBrowserTest, MultipleDomains) {
  SetPolicy({{.patterns = {"example.com", "example.org"},
              .headers = {{"X-Test-Header", "TestValue"}}}});

  // Test first domain
  GURL url1 = embedded_test_server()->GetURL("example.com", "/match");
  ASSERT_TRUE(NavigateToUrl(url1, this));
  CheckHeaderValuePresent("X-Test-Header", "TestValue");

  // Test second domain
  GURL url2 = embedded_test_server()->GetURL("example.org", "/match");
  ASSERT_TRUE(NavigateToUrl(url2, this));
  CheckHeaderValuePresent("X-Test-Header", "TestValue");
}

// Tests that a rule with multiple headers correctly injects all of them.
IN_PROC_BROWSER_TEST_F(HttpHeaderInjectionBrowserTest, MultipleHeaders) {
  SetPolicy({{.patterns = {"example.com"},
              .headers = {{"X-Test-Header-1", "Value1"},
                          {"X-Test-Header-2", "Value2"}}}});

  GURL url = embedded_test_server()->GetURL("example.com", "/match");
  ASSERT_TRUE(NavigateToUrl(url, this));

  CheckHeaderValuePresent("X-Test-Header-1", "Value1");
  CheckHeaderValuePresent("X-Test-Header-2", "Value2");
}

// Tests that a more specific domain rule takes precedence over a less
// specific one.
IN_PROC_BROWSER_TEST_F(HttpHeaderInjectionBrowserTest, Precedence) {
  SetPolicy({{.patterns = {"sub.example.com"},
              .headers = {{"X-Test-Header", "Value2"}}},
             {.patterns = {"example.com"},
              .headers = {{"X-Test-Header", "Value1"}}}});

  // Navigate to more specific domain
  GURL url = embedded_test_server()->GetURL("sub.example.com", "/match");
  ASSERT_TRUE(NavigateToUrl(url, this));

  // Verify more specific rule won
  CheckHeaderValuePresent("X-Test-Header", "Value2");
}

// Tests that enterprise header injection works alongside an extension that also
// modifies headers.
IN_PROC_BROWSER_TEST_F(HttpHeaderInjectionBrowserTest, ExtensionInteraction) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionWithHeaderRules("main_frame");
  ASSERT_TRUE(extension);

  SetPolicy({{.patterns = {"example.com"},
              .headers = {{"X-Test-Header", "TestValue"}}}});

  GURL url = embedded_test_server()->GetURL("example.com", "/match");
  ASSERT_TRUE(NavigateToUrl(url, this));

  // Verify enterprise policy took precedence over the extension's modification.
  CheckHeaderValuePresent("X-Test-Header", "TestValue");
  // Verify the extension's unrelated header was still successfully injected
  // alongside the enterprise policy.
  CheckHeaderValuePresent("X-Extension-Header", "ExtensionValue");
}

// Tests that custom headers are correctly injected into WebSocket handshake
// requests.
IN_PROC_BROWSER_TEST_F(HttpHeaderInjectionBrowserTest, WebSocketInjection) {
  SetPolicy({{.patterns = {"example.com"},
              .headers = {{"X-Test-Header", "TestValue"}}}});

  GURL url = embedded_test_server()->GetURL("example.com", "/empty.html");
  ASSERT_TRUE(NavigateToUrl(url, this));

  GURL ws_url = net::test_server::GetWebSocketURL(
      *embedded_test_server(), "example.com", "/echo-request-headers");

  std::string script = content::JsReplace(
      "new Promise(resolve => {"
      "  let ws = new WebSocket($1);"
      "  ws.onmessage = e => {"
      "    resolve(JSON.parse(e.data));"
      "  };"
      "  ws.onerror = () => resolve('ERROR');"
      "});",
      ws_url);

  content::EvalJsResult eval_result =
      content::EvalJs(chrome_test_utils::GetActiveWebContents(this), script);
  const base::DictValue& headers = eval_result.ExtractDict();

  const std::string* test_header = headers.FindString("x-test-header");
  ASSERT_TRUE(test_header);
  EXPECT_EQ("TestValue", *test_header);
}

// Tests that enterprise headers injected into WebSocket handshake requests take
// precedence over modifications made by extensions.
IN_PROC_BROWSER_TEST_F(HttpHeaderInjectionBrowserTest,
                       WebSocketExtensionInteraction) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionWithHeaderRules("websocket");
  ASSERT_TRUE(extension);

  SetPolicy({{.patterns = {"example.com"},
              .headers = {{"X-Test-Header", "TestValue"}}}});

  GURL url = embedded_test_server()->GetURL("example.com", "/empty.html");
  ASSERT_TRUE(NavigateToUrl(url, this));

  GURL ws_url = net::test_server::GetWebSocketURL(
      *embedded_test_server(), "example.com", "/echo-request-headers");

  std::string script = content::JsReplace(
      "new Promise(resolve => {"
      "  let ws = new WebSocket($1);"
      "  ws.onmessage = e => {"
      "    resolve(JSON.parse(e.data));"
      "  };"
      "  ws.onerror = () => resolve('ERROR');"
      "});",
      ws_url);

  content::EvalJsResult eval_result =
      content::EvalJs(chrome_test_utils::GetActiveWebContents(this), script);
  const base::DictValue& headers = eval_result.ExtractDict();

  // Verify enterprise policy took precedence over the extension's modification.
  const std::string* test_header = headers.FindString("x-test-header");
  ASSERT_TRUE(test_header);
  EXPECT_EQ("TestValue", *test_header);

  // Verify the extension's unrelated header was still successfully injected
  // alongside the enterprise policy.
  const std::string* ext_header = headers.FindString("x-extension-header");
  ASSERT_TRUE(ext_header);
  EXPECT_EQ("ExtensionValue", *ext_header);
}

}  // namespace enterprise_custom_headers

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/translate_page_tool.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/actor/tools/translate_page_tool_request.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace actor {

namespace {

class ActorTranslatePageToolBrowserTest : public ActorToolsTest {
 public:
  ActorTranslatePageToolBrowserTest() = default;
  ~ActorTranslatePageToolBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    ActorToolsTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        translate::switches::kTranslateScriptURL,
        embedded_test_server()->GetURL("/mock_translate_script.js").spec());
  }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&ActorTranslatePageToolBrowserTest::HandleRequest,
                            base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().GetPath() != "/mock_translate_script.js") {
      return nullptr;
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);

    std::string script = R"JS(
      var google = {};
      google.translate = (function() {
        return {
          TranslateService: function() {
            return {
              isAvailable : function() { return true; },
              restore : function() { return; },
              getDetectedLanguage : function() { return "es"; },
              translatePage : function(sourceLang, targetLang,
                                       onTranslateProgress) {
                onTranslateProgress(100, true, false);
              }
            };
          }
        };
      })();
      cr.googleTranslate.onTranslateElementLoad();
    )JS";

    http_response->set_content(script);
    http_response->set_content_type("text/javascript");
    return std::move(http_response);
  }
};

IN_PROC_BROWSER_TEST_F(ActorTranslatePageToolBrowserTest,
                       TranslateDefaultLanguage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeTranslatePageRequest(*active_tab());
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectOkResult(result);

  ChromeTranslateClient* translate_client =
      ChromeTranslateClient::FromWebContents(web_contents());
  ASSERT_TRUE(translate_client);

  std::string source_language;
  std::string expected_target_language;
  translate_client->GetTranslateLanguages(web_contents(), &source_language,
                                          &expected_target_language,
                                          /*for_display=*/false);
  EXPECT_EQ(expected_target_language,
            translate_client->GetLanguageState().current_language());
}

IN_PROC_BROWSER_TEST_F(ActorTranslatePageToolBrowserTest,
                       TranslateSpecificTargetLanguage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeTranslatePageRequest(*active_tab(), "fr");
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectOkResult(result);

  ChromeTranslateClient* translate_client =
      ChromeTranslateClient::FromWebContents(web_contents());
  ASSERT_TRUE(translate_client);
  EXPECT_EQ("fr", translate_client->GetLanguageState().current_language());
}

IN_PROC_BROWSER_TEST_F(ActorTranslatePageToolBrowserTest,
                       TranslateUnsupportedLanguage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeTranslatePageRequest(*active_tab(), "unsupported-lang-xyz");
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kTranslateUnsupportedLanguage);
}

}  // namespace
}  // namespace actor

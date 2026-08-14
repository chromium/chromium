// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_handler.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_page.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"

namespace contextual_tasks {

using testing::_;
using testing::NiceMock;
using testing::Return;

class ContextualTasksExtensionHandlerBrowserTestBase
    : public InProcessBrowserTest {
 public:
  ContextualTasksExtensionHandlerBrowserTestBase(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ~ContextualTasksExtensionHandlerBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();

    ContextualTasksExtensionHandler::CreateForCurrentDocument(rfh);
    handler_ = ContextualTasksExtensionHandler::GetForCurrentDocument(rfh);
    ASSERT_NE(handler_, nullptr);

    // Bind the mock page to the handler.
    mojo::PendingReceiver<mojom::ExtensionPageHandler> page_handler_receiver;
    handler_->CreateExtensionPageHandler(mock_page_.BindAndGetRemote(),
                                         std::move(page_handler_receiver));

    // Bind the mock searchbox page to the handler.
    mojo::PendingReceiver<composebox::mojom::PageHandler> composebox_receiver;
    mojo::PendingReceiver<searchbox::mojom::PageHandler> searchbox_receiver;
    static_cast<composebox::mojom::PageHandlerFactory*>(handler_)
        ->CreatePageHandler(std::move(composebox_receiver),
                            mock_searchbox_page_.BindAndGetRemote(),
                            std::move(searchbox_receiver));

    // Set up mock session handle and controller.
    auto session_handle = std::make_unique<
        NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
    mock_session_handle_ = session_handle.get();

    mock_controller_ = std::make_unique<
        NiceMock<contextual_search::MockContextualSearchContextController>>();

    ON_CALL(*mock_session_handle_, GetController())
        .WillByDefault(Return(mock_controller_.get()));

    lens::ClientToAimMessage non_empty_message;
    non_empty_message.mutable_submit_query();
    ON_CALL(*mock_controller_, CreateClientToAimRequest(_))
        .WillByDefault(Return(non_empty_message));

    // Set up file info for the active tab context.
    SessionID active_tab_id =
        sessions::SessionTabHelper::IdForTab(web_contents_);
    contextual_search::FileInfo file_info;
    file_info.tab_session_id = active_tab_id;
    lens::LensOverlayRequestId request_id;
    request_id.set_context_id(12345);
    file_info.request_id = request_id;
    std::vector<contextual_search::FileInfo> file_infos = {file_info};

    ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
        .WillByDefault(Return(file_infos));

    ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
        ->SetTaskSession(std::nullopt, std::move(session_handle),
                         /*input_state_model=*/nullptr);
  }

  void TearDownOnMainThread() override {
    handler_ = nullptr;
    mock_session_handle_ = nullptr;
    mock_controller_.reset();
    web_contents_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<ContextualTasksExtensionHandler> handler_ = nullptr;
  NiceMock<MockContextualTasksExtensionPage> mock_page_;
  NiceMock<MockSearchboxPage> mock_searchbox_page_;
  raw_ptr<contextual_search::MockContextualSearchSessionHandle>
      mock_session_handle_ = nullptr;
  std::unique_ptr<contextual_search::MockContextualSearchContextController>
      mock_controller_;
};

class ContextualTasksExtensionHandlerBrowserTest
    : public ContextualTasksExtensionHandlerBrowserTestBase {
 public:
  ContextualTasksExtensionHandlerBrowserTest()
      : ContextualTasksExtensionHandlerBrowserTestBase(
            {kContextualTasks, kContextualTasksRearchitecture},
            {}) {}
};

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       SubmitQuery) {
  base::RunLoop run_loop;

  // We expect PostAimMessage to be called on the mock page.
  EXPECT_CALL(mock_page_, PostAimMessage(_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  // Call SubmitQuery on the handler (which implements
  // searchbox::mojom::PageHandler). We cast it to make sure we are calling the
  // interface method.
  static_cast<searchbox::mojom::PageHandler*>(handler_)->SubmitQuery(
      "test query", 0, false, false, false, false, /*is_voice_search=*/false);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       GetInputState) {
  base::RunLoop run_loop;

  static_cast<searchbox::mojom::PageHandler*>(handler_)->GetInputState(
      base::BindLambdaForTesting(
          [&](const std::optional<omnibox::InputState>& state) {
            // Since we are mocking the session, we expect a valid model to be
            // created and a default InputState to be returned (not nullopt).
            EXPECT_TRUE(state.has_value());
            run_loop.Quit();
          }));

  run_loop.Run();
}

class ContextualTasksExtensionResourcesTest : public InProcessBrowserTest {
 public:
  ContextualTasksExtensionResourcesTest() {
    feature_list_.InitWithFeatures(
        {kContextualTasks, kContextualTasksRearchitecture,
         extensions_features::kApiContextualTasksPrivate},
        {});
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  }
  ~ContextualTasksExtensionResourcesTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionResourcesTest, ResourcesLoad) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->GetProfile());
  bool found = false;
  for (const auto& ext : registry->enabled_extensions()) {
    if (ext->id() == "glbjnfimcajjenihimblfaponejbkoph") {
      found = true;
    }
  }
  EXPECT_TRUE(found) << "Contextual Tasks extension not loaded!";

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetFilter(base::BindRepeating(
      [](const content::WebContentsConsoleObserver::Message& message) {
        return message.log_level == blink::mojom::ConsoleMessageLevel::kError;
      }));

  GURL extension_url(
      "chrome-extension://glbjnfimcajjenihimblfaponejbkoph/input_plate.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_url));

  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Verify that the custom properties are set on the host element.
  {
    std::string check_vars_script = R"(
      (() => {
        const app = document.querySelector('contextual-tasks-composebox');
        if (!app) return 'NO_APP';
        const style = window.getComputedStyle(app);
        const spark = style.getPropertyValue('--search-spark-icon-url').trim();
        const camera = style.getPropertyValue('--camera-icon-url').trim();
        return JSON.stringify({spark, camera});
      })()
    )";
    content::EvalJsResult result =
        content::EvalJs(web_contents, check_vars_script);
    std::string json_str = result.ExtractString();
    ASSERT_NE(json_str, "NO_APP");
    EXPECT_NE(json_str.find("chrome://resources/cr_components/searchbox/icons/"
                            "search_spark.svg"),
              std::string::npos);
    EXPECT_NE(
        json_str.find(
            "chrome://resources/cr_components/searchbox/icons/camera.svg"),
        std::string::npos);
  }

  // Type text to enable cancel button and trigger potential resource loads.
  {
    std::string type_script = R"(
      (async () => {
        const app = document.querySelector('contextual-tasks-composebox');
        if (!app) return 'NO_APP';
        const composebox = app.shadowRoot.querySelector('#composebox');
        if (!composebox) return 'NO_COMPOSEBOX';
        const composeboxInput =
            composebox.shadowRoot.querySelector('#composeboxInput');
        if (!composeboxInput) return 'NO_INPUT';

        const cancelContainer =
            composeboxInput.shadowRoot.querySelector('#cancelContainer');
        if (!cancelContainer) return 'NO_CANCEL_CONTAINER_IN_TYPE';
        cancelContainer.style.setProperty('transition', 'none', 'important');
        cancelContainer.style.setProperty('opacity', '1', 'important');

        const textarea = composeboxInput.shadowRoot.querySelector('#input');
        if (!textarea) return 'NO_TEXTAREA';
        textarea.value = 'test';
        textarea.dispatchEvent(new Event('input', { bubbles: true }));
        await composeboxInput.updateComplete;
        await composebox.updateComplete;
        return 'OK';
      })()
    )";
    content::EvalJsResult result = content::EvalJs(web_contents, type_script);
    ASSERT_EQ(result.ExtractString(), "OK");
  }

  // Verify that the aimIcon actually uses the chrome:// URL.
  {
    std::string check_icon_script = R"(
      (() => {
        const app = document.querySelector('contextual-tasks-composebox');
        if (!app) return 'NO_APP';
        const composebox = app.shadowRoot.querySelector('#composebox');
        if (!composebox) return 'NO_COMPOSEBOX';
        const composeboxInput =
            composebox.shadowRoot.querySelector('#composeboxInput');
        if (!composeboxInput) return 'NO_INPUT';
        const aimIcon = composeboxInput.shadowRoot.querySelector('#aimIcon');
        if (!aimIcon) return 'NO_AIM_ICON';
        const style = window.getComputedStyle(aimIcon);
        const aimIconMask = style.webkitMaskImage || style.maskImage || 'NONE';

        const cancelIcon =
            composeboxInput.shadowRoot.querySelector('#cancelIcon');
        if (!cancelIcon) return 'NO_CANCEL_ICON';
        const cancelContainer =
            composeboxInput.shadowRoot.querySelector('#cancelContainer');
        if (!cancelContainer) return 'NO_CANCEL_CONTAINER';

        const cancelStyle = window.getComputedStyle(cancelIcon);
        const containerStyle = window.getComputedStyle(cancelContainer);

        const submitButton =
            composebox.shadowRoot.querySelector('cr-composebox-submit');
        let submitIconImage = 'NO_SUBMIT_BUTTON';
        let submitBgColor = 'NO_SUBMIT_BUTTON';
        if (submitButton) {
          const submitIcon =
              submitButton.shadowRoot.querySelector('#submitIcon');
          if (submitIcon) {
            const submitStyle = window.getComputedStyle(submitIcon);
            submitIconImage =
                submitStyle.getPropertyValue('--cr-icon-image').trim();
          } else {
            submitIconImage = 'NO_SUBMIT_ICON';
          }
          const submitEnergy =
              submitButton.shadowRoot.querySelector('#submitEnergy');
          if (submitEnergy) {
            const energyStyle = window.getComputedStyle(submitEnergy);
            submitBgColor = energyStyle.backgroundColor;
          } else {
            submitBgColor = 'NO_SUBMIT_ENERGY';
          }
        }

        const composeboxAttrs =
            Array.from(composebox.attributes).map(a => `${a.name}=${a.value}`);

        return JSON.stringify({
          aimIconMask,
          cancelIconDisplay: cancelStyle.display,
          cancelIconOpacity: cancelStyle.opacity,
          cancelContainerDisplay: containerStyle.display,
          cancelContainerOpacity: containerStyle.opacity,
          cancelIconImage:
              cancelStyle.getPropertyValue('--cr-icon-image').trim(),
          submitIconImage,
          submitBgColor,
          composeboxAttrs,
          composeboxInput_input: composeboxInput.input,
          composebox_input: composebox.input,
          composebox_submitEnabled: composebox.submitEnabled,
        });
      })()
    )";
    content::EvalJsResult result =
        content::EvalJs(web_contents, check_icon_script);
    std::string json_str = result.ExtractString();
    ASSERT_NE(json_str, "NO_APP");
    ASSERT_NE(json_str, "NO_COMPOSEBOX");
    ASSERT_NE(json_str, "NO_INPUT");
    ASSERT_NE(json_str, "NO_AIM_ICON");
    ASSERT_NE(json_str, "NO_CANCEL_ICON");
    ASSERT_NE(json_str, "NO_CANCEL_CONTAINER");
    // Parse JSON in C++ to check values.
    std::optional<base::Value> value =
        base::JSONReader::Read(json_str, base::JSON_PARSE_RFC);
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(value->is_dict());
    const base::DictValue& dict = value->GetDict();

    const std::string* aim_mask = dict.FindString("aimIconMask");
    ASSERT_TRUE(aim_mask);
    EXPECT_NE(aim_mask->find("chrome://resources/cr_components/searchbox/icons/"
                             "search_spark.svg"),
              std::string::npos);

    const std::string* cancel_image = dict.FindString("cancelIconImage");
    ASSERT_TRUE(cancel_image);
    EXPECT_NE(cancel_image->find("chrome://resources/images/icon_clear.svg"),
              std::string::npos);

    const std::string* submit_image = dict.FindString("submitIconImage");
    ASSERT_TRUE(submit_image);
    ASSERT_NE(*submit_image, "NO_SUBMIT_BUTTON");
    ASSERT_NE(*submit_image, "NO_SUBMIT_ICON");
    EXPECT_NE(
        submit_image->find("chrome://resources/images/icon_arrow_upward.svg"),
        std::string::npos);

    const std::string* submit_bg = dict.FindString("submitBgColor");
    ASSERT_TRUE(submit_bg);
    ASSERT_NE(*submit_bg, "NO_SUBMIT_BUTTON");
    ASSERT_NE(*submit_bg, "NO_SUBMIT_ENERGY");
    EXPECT_EQ(*submit_bg, "rgb(51, 110, 243)");

    const std::string* cancel_display = dict.FindString("cancelIconDisplay");
    ASSERT_TRUE(cancel_display);
    EXPECT_EQ(*cancel_display, "block");

    const std::string* container_opacity =
        dict.FindString("cancelContainerOpacity");
    ASSERT_TRUE(container_opacity);
    EXPECT_EQ(*container_opacity, "1");
  }
  EXPECT_TRUE(console_observer.messages().empty());
}

}  // namespace contextual_tasks

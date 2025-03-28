// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/base64.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_identifier.h"

namespace glic::test {

namespace {

using optimization_guide::proto::BrowserAction;
using optimization_guide::proto::ClickAction;

// TODO(https://crbug.com/402086021): Get the actual target details for the
// button in the test page.
constexpr int32_t kContentNodeId = 123;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTabId);

class GlicActorControllerUiTest : public test::InteractiveGlicTest {
 public:
  GlicActorControllerUiTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicActor);
  }
  ~GlicActorControllerUiTest() override = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/actor");
    test::InteractiveGlicTest::SetUpOnMainThread();
  }

  // Calls actInFocusedTab() and waits until the promise resolves and succeeds.
  // TODO(https://crbug.com/402086021): Change script to just return the promise
  // instead of waiting. This needs to be done when changing ActionSucceeds test
  // to explicitly check that the action worked in the page.
  auto ExecuteAction(std::string_view encodedActionProto,
                     base::Value::Dict contextOptions) {
    return Steps(CheckJsResult(
        kGlicContentsElementId,
        content::JsReplace(R"js(
                        async () => {
                          const base64ToArrayBuffer = (base64) => {
                            const bytes = window.atob(base64);
                            const len = bytes.length;
                            const ret = new Uint8Array(len);
                            for (var i = 0; i < len; i++) {
                              ret[i] = bytes.charCodeAt(i);
                            }
                            return ret.buffer;
                          }
                          // As in TODO above, remove the async from the
                          // function and return the promise directly
                          // return client.browser.actInFocusedTab({
                          await client.browser.actInFocusedTab({
                            actionProto: base64ToArrayBuffer($1),
                            tabContextOptions: $2
                          });
                          return true;
                        }
                      )js",
                           encodedActionProto, std::move(contextOptions))));
  }

  // Calls actInFocusedTab() and waits until the promise rejects with an error.
  // Note: This will fail the test if the promise succeeds.
  auto ExecuteActionExpectingError(
      std::string_view encodedActionProto,
      base::Value::Dict contextOptions,
      glic::mojom::ActInFocusedTabErrorReason error_reason) {
    return Steps(CheckJsResult(
        kGlicContentsElementId,
        content::JsReplace(R"js(
                        async () => {
                          const base64ToArrayBuffer = (base64) => {
                            const bytes = window.atob(base64);
                            const len = bytes.length;
                            const ret = new Uint8Array(len);
                            for (var i = 0; i < len; i++) {
                              ret[i] = bytes.charCodeAt(i);
                            }
                            return ret.buffer;
                          }
                          try {
                            await client.browser.actInFocusedTab({
                              actionProto: base64ToArrayBuffer($1),
                              tabContextOptions: $2
                            });
                          } catch (err) {
                            return err.reason;
                          }
                        }
                      )js",
                           encodedActionProto, std::move(contextOptions)),
        ::testing::Eq(static_cast<int>(error_reason))));
  }

  std::string EncodeActionProto(const BrowserAction& action) {
    std::string serialized_message = action.SerializeAsString();
    return base::Base64Encode(serialized_message);
  }

  base::Value::Dict UpdatedContextOptions() {
    return base::Value::Dict()
        .Set("annotatedPageContent", true)
        .Set("viewportScreenshot", true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(https://crbug.com/402086021): Enable test after using real nodeId in
// proto.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, DISABLED_ActionSucceeds) {
  std::string encodedProto =
      EncodeActionProto(actor::MakeClick(kContentNodeId));
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(kActiveTabId,
                                      embedded_test_server()->GetURL(
                                          "/page_with_clickable_element.html")),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  ExecuteAction(encodedProto, UpdatedContextOptions()));
  // TODO(https://crbug.com/402086021): Check result after implementing tool
  // calling to do the action.
  // WaitForJsResult(kActiveTabId, "() => button_clicked"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ActionProtoInvalid) {
  std::string encodedProto = base::Base64Encode("invalid serialized bytes");
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/page_with_clickable_element.html")),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ExecuteActionExpectingError(
          encodedProto, UpdatedContextOptions(),
          glic::mojom::ActInFocusedTabErrorReason::kInvalidActionProto));
}

// TODO(https://crbug.com/402086021): Enable test after implementing tool
// calling to do the action.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       DISABLED_ActionTargetNotFound) {
  std::string encodedProto =
      EncodeActionProto(actor::MakeClick(kContentNodeId));
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/page_with_clickable_element.html")),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ExecuteActionExpectingError(
          encodedProto, UpdatedContextOptions(),
          glic::mojom::ActInFocusedTabErrorReason::kTargetNotFound));
}

class GlicActorControllerWithActorDisabledUiTest
    : public test::InteractiveGlicTest {
 public:
  GlicActorControllerWithActorDisabledUiTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kGlicActor);
  }
  ~GlicActorControllerWithActorDisabledUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorControllerWithActorDisabledUiTest,
                       ActorNotAvailable) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  InAnyContext(CheckJsResult(
                      kGlicContentsElementId,
                      "() => { return !(client.browser.actInFocusedTab); }")));
}

}  //  namespace

}  // namespace glic::test

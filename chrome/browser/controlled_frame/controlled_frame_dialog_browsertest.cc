// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <utility>

#include "chrome/browser/controlled_frame/controlled_frame_permission_request_test_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace controlled_frame {

class ControlledFrameDialogBrowserTest
    : public ControlledFrameTestBase,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    ControlledFrameTestBase::SetUpOnMainThread();
    StartContentServer("web_apps/simple_isolated_app");
  }

 protected:
  const std::string handle_dialog_str() const {
    return GetParam() ? "ok" : "cancel";
  }
};

IN_PROC_BROWSER_TEST_P(ControlledFrameDialogBrowserTest, Confirm) {
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt, "/index.html");
  ASSERT_EQ("SUCCESS", content::EvalJs(app_frame, content::JsReplace(
                                                      R"(
      (function() {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame) {
          return 'FAIL: Could not find a controlledframe element.';
        }
        frame.addEventListener('dialog', (e) => {
          document.testLastDialog = e;
          e.dialog[$1]();
        });
        return 'SUCCESS';
      })();
    )",
                                                      handle_dialog_str())));

  EXPECT_EQ(GetParam(), content::EvalJs(controlled_frame,
                                        R"(
      (async function() {
        try {
          return await confirm('confirm test text');
        } catch (err) {
          return 'FAIL: ' + err.name + ': ' + err.message;
        }
      })();
    )"));

  EXPECT_EQ("confirm",
            content::EvalJs(app_frame, "document.testLastDialog.messageType;"));
  EXPECT_EQ("confirm test text",
            content::EvalJs(app_frame, "document.testLastDialog.messageText;"));
  EXPECT_EQ("", content::EvalJs(app_frame,
                                "document.testLastDialog.defaultPromptText;"));
}

IN_PROC_BROWSER_TEST_P(ControlledFrameDialogBrowserTest, Prompt) {
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt, "/index.html");
  ASSERT_EQ("SUCCESS", content::EvalJs(app_frame, content::JsReplace(
                                                      R"(
      (function() {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame) {
          return 'FAIL: Could not find a controlledframe element.';
        }
        frame.addEventListener('dialog', (e) => {
          document.testLastDialog = e;
          e.dialog[$1]('prompt non-default value');
        });
        return 'SUCCESS';
      })();
    )",
                                                      handle_dialog_str())));

  EXPECT_EQ(
      (GetParam() ? base::Value("prompt non-default value") : base::Value()),
      content::EvalJs(controlled_frame,
                      R"(
      (async function() {
        try {
          return await prompt('prompt test text', 'prompt default value');
        } catch (err) {
          return 'FAIL: ' + err.name + ': ' + err.message;
        }
      })();
    )"));

  EXPECT_EQ("prompt",
            content::EvalJs(app_frame, "document.testLastDialog.messageType;"));
  EXPECT_EQ("prompt test text",
            content::EvalJs(app_frame, "document.testLastDialog.messageText;"));
  EXPECT_EQ(
      "prompt default value",
      content::EvalJs(app_frame, "document.testLastDialog.defaultPromptText;"));
}

IN_PROC_BROWSER_TEST_P(ControlledFrameDialogBrowserTest, Alert) {
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt, "/index.html");
  ASSERT_EQ("SUCCESS", content::EvalJs(app_frame, content::JsReplace(
                                                      R"(
      (function() {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame) {
          return 'FAIL: Could not find a controlledframe element.';
        }
        frame.addEventListener('dialog', (e) => {
          document.testLastDialog = e;
          e.dialog[$1]();
        });
        return 'SUCCESS';
      })();
    )",
                                                      handle_dialog_str())));

  EXPECT_EQ(nullptr, content::EvalJs(controlled_frame,
                                     R"(
      (async function() {
        try {
          return await alert('alert test text');
        } catch (err) {
          return 'FAIL: ' + err.name + ': ' + err.message;
        }
      })();
    )"));

  EXPECT_EQ("alert",
            content::EvalJs(app_frame, "document.testLastDialog.messageType;"));
  EXPECT_EQ("alert test text",
            content::EvalJs(app_frame, "document.testLastDialog.messageText;"));
  EXPECT_EQ("", content::EvalJs(app_frame,
                                "document.testLastDialog.defaultPromptText;"));
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/
                         ,
                         ControlledFrameDialogBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Ok" : "Cancel";
                         });

}  // namespace controlled_frame

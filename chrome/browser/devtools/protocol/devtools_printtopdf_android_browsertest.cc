// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"
#include "url/gurl.h"

class DevToolsPrintToPdfAndroidTest : public DevToolsProtocolTestBase {};

IN_PROC_BROWSER_TEST_F(DevToolsPrintToPdfAndroidTest, TrustedClientPrintsPdf) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL("data:text/html,print to PDF")));
  Attach();

  const base::DictValue* result =
      SendCommandSync("Page.printToPDF", base::DictValue());
  ASSERT_TRUE(result);
  const std::string* encoded_pdf = result->FindString("data");
  ASSERT_TRUE(encoded_pdf);

  std::string pdf;
  ASSERT_TRUE(base::Base64Decode(*encoded_pdf, &pdf));
  EXPECT_TRUE(base::StartsWith(pdf, "%PDF-"));
}

IN_PROC_BROWSER_TEST_F(DevToolsPrintToPdfAndroidTest,
                       TrustedClientRetainsContentPageCommands) {
  Attach();

  ASSERT_TRUE(SendCommandSync("Page.enable"));
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), GURL("data:text/html,content Page command")));
  WaitForNotification("Page.loadEventFired", /*allow_existing=*/true);

  const base::DictValue* sub_apps_result =
      SendCommandSync("Page.getSubApps", base::DictValue());
  ASSERT_TRUE(sub_apps_result);
  const base::ListValue* sub_apps = sub_apps_result->FindList("subApps");
  ASSERT_TRUE(sub_apps);
  EXPECT_TRUE(sub_apps->empty());
}

IN_PROC_BROWSER_TEST_F(DevToolsPrintToPdfAndroidTest,
                       TrustedClientCannotUseDesktopOnlyPageCommands) {
  Attach();

  struct Command {
    const char* method;
    const char* mode;
  };
  // An invalid RPH mode makes the pre-fix browser handler claim the command
  // without consulting Android's protocol-handler registry.
  constexpr Command kCommands[] = {
      {"Page.setSPCTransactionMode", "none"},
      {"Page.setRPHRegistrationMode", "invalid"},
  };

  for (const auto& command : kCommands) {
    SCOPED_TRACE(command.method);
    base::DictValue params;
    params.Set("mode", command.mode);
    EXPECT_FALSE(SendCommandSync(command.method, std::move(params)));
    ASSERT_TRUE(error());
    EXPECT_EQ(error()->FindInt("code"),
              static_cast<int>(crdtp::DispatchCode::METHOD_NOT_FOUND));
  }
}

IN_PROC_BROWSER_TEST_F(DevToolsPrintToPdfAndroidTest,
                       UntrustedClientCannotPrintPdf) {
  SetIsTrusted(false);
  Attach();

  EXPECT_FALSE(SendCommandSync("Page.printToPDF", base::DictValue()));
  ASSERT_TRUE(error());
  EXPECT_EQ(error()->FindInt("code"),
            static_cast<int>(crdtp::DispatchCode::METHOD_NOT_FOUND));
}

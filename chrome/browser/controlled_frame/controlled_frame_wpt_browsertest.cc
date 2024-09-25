// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_expected_support.h"
#include "chrome/browser/controlled_frame/controlled_frame_test_base.h"
#include "chrome/browser/controlled_frame/scoped_test_driver_proxy.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/policy_helper_public.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace controlled_frame {

namespace {
const auto kTestFiles = testing::Values("add_content_scripts.window.js",
                                        "camera.window.js",
                                        "geolocation.window.js",
                                        "new_window.window.js",
                                        "no_callback.window.js",
                                        "scheme.window.js");

constexpr char kTestDirectory[] = "chrome/test/data/controlled_frame";
constexpr char kTestHarnessPath[] =
    "third_party/blink/web_tests/external/wpt/resources/testharness.js";
constexpr char kTestDriverPath[] =
    "third_party/blink/web_tests/external/wpt/resources/testdriver.js";

constexpr char kHtmlWrapperSrc[] = R"(
  <!doctype html>
  <meta charset=utf-8>
  <script src="/resources/testharness.js"></script>
  <script src="/resources/testharnessreport.js"></script>
  <script src="/resources/testdriver.js"></script>
  <script src="/resources/testdriver-vendor.js"></script>
  <body>
)";

constexpr char kWptReporterSrc[] = R"(
  (function() {
    const policy = window.trustedTypes.createPolicy('policy', {
      createScriptURL: url => url,
    });

    function isFailure(test) {
      return test.status !== 0;
    }

    function generateMessage(test) {
      const status_descriptions = ['PASS', 'FAIL', 'TIMEOUT', 'NOTRUN',
                                   'PRECONDITION_FAILED'];
      let msg = `${status_descriptions[test.status]}: ${test.name}`;
      if (test.message) {
        msg += `\n  ${test.message}`;
      }
      if (test.stack) {
        msg += `\n${test.stack}`;
      }
      return msg;
    }

    window.results = new Promise(async (resolve, reject) => {
      add_completion_callback((tests) => {
        if (tests.length === 0) {
          reject('No test results found');
          return;
        }
        const failed_tests = tests.filter(isFailure);
        if (failed_tests.length === 0) {
          resolve(tests.map(generateMessage).join('\n'));
          return;
        }
        reject(failed_tests.map(generateMessage).join('\n\n'));
      });

      const testSrc = new URLSearchParams(location.search).get('test');
      if (!testSrc) {
        reject('Missing test parameter in URL query');
        return;
      }

      const testBody = await (await fetch(testSrc)).text();
      const importRegex = /^\/\/ META script=(.*)/gm;
      const imports = [...testBody.matchAll(importRegex)]
          .map((match) => match[1]);

      imports.push(testSrc);
      for (const importSrc of imports) {
        const script = document.createElement('script');
        script.src = policy.createScriptURL(importSrc);
        script.async = false;
        document.head.appendChild(script);
      }
    });
  })();
)";

}  // namespace

// These tests wrap WPT infrastructure in a browser test to allow us to run
// WPTs within an IWA. When IWA support is officially added to WPTs, we can
// move all the Javascript tests referenced here into the main WPT test
// directory and remove this test.
class ControlledFrameWptBrowserTest
    : public ControlledFrameTestBase,
      public testing::WithParamInterface<std::string> {};

// This test includes the WPT test harness and an adapter script that will:
//   * Register a completion callback with the test harness.
//   * Inject a script tag with a src equal to the URL query. This is how the
//     test configures which WPT *.window.js test file is run.
//   * Put the results in a top-level window.results promise that will contain
//     the list of passed tests if there were no failures, or reject the promise
//     with error information for failing tests otherwise.
IN_PROC_BROWSER_TEST_P(ControlledFrameWptBrowserTest, Run) {
  device::ScopedGeolocationOverrider overrider(/*latitude=*/1, /*longitude=*/2);

  base::FilePath test_data_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));

  // Enable all permissions policies by default in the IWA hosting the tests.
  // There's currently no way within in an individual WPT to declare that a
  // specific permissions policy is needed, so we have to enable them all. If a
  // test needs one disabled, it can achieve that through an iframe that
  // doesn't delegate the feature.
  web_app::ManifestBuilder manifest;
  for (auto const& [feature, _] :
       blink::GetPermissionsPolicyFeatureToNameMap()) {
    manifest.AddPermissionsPolicyWildcard(feature);
  }
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(manifest)
          .AddFolderFromDisk("/", test_data_dir.AppendASCII(kTestDirectory))
          .AddFileFromDisk("/resources/testharness.js",
                           test_data_dir.AppendASCII(kTestHarnessPath))
          .AddFileFromDisk("/resources/testdriver.js",
                           test_data_dir.AppendASCII(kTestDriverPath))
          .AddHtml("/", kHtmlWrapperSrc)
          .AddJs("/resources/testharnessreport.js", kWptReporterSrc)
          .AddJs("/resources/testdriver-vendor.js",
                 ScopedTestDriverProxy::testdriver_override_script_src())
          .BuildBundle();

  app->TrustSigningKey();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->Install(profile()));

  std::string test_params = "?test=" + GetParam() + "&https_origin=" +
                            https_server()->base_url().spec();
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id(), test_params);
  ScopedTestDriverProxy scoped_test_driver_proxy(app_frame);

  content::EvalJsResult results = content::EvalJs(app_frame, "window.results");
  if (!results.value.is_none()) {
    CHECK(results.value.is_string());
    LOG(INFO) << "Results:\n" << results.value.GetString();
  }
  ASSERT_THAT(results, content::EvalJsResult::IsOk());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ControlledFrameWptBrowserTest,
    kTestFiles,
    [](const testing::TestParamInfo<std::string>& param_info) {
      std::string test_name = param_info.param;
      base::ReplaceSubstringsAfterOffset(&test_name, 0, ".window.js", "");
      base::ReplaceChars(test_name, ".", "_", &test_name);
      return test_name;
    });

}  // namespace controlled_frame

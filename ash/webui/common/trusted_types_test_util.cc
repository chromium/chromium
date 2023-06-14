// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/trusted_types_test_util.h"

#include "content/public/test/browser_test_utils.h"

namespace ash::test_util {

::testing::AssertionResult AddTestStaticUrlPolicy(
    const content::ToRenderFrameHost& execution_target) {
  constexpr char kScript[] = R"(
        (() => {
            window.testStaticUrlPolicy = trustedTypes.createPolicy(
            'ash-webui-test-script',
            {createScriptURL: url => url});
        })();
    )";
  return content::ExecJs(execution_target, kScript);
}

}  // namespace ash::test_util

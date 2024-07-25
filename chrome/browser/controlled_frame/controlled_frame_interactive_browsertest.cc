// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/controlled_frame/controlled_frame_permission_request_test_base.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace controlled_frame {

using ControlledFramePermissionRequestInteractiveTest =
    ControlledFramePermissionRequestTestBase;

#if !BUILDFLAG(IS_MAC)  // Pointer lock isn't available on MacOS bots.
// This is an interactive_ui_test because pointer locks affect global system
// state, which could interact poorly with other concurrently run tests.
IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestInteractiveTest,
                       PointerLock) {
  PermissionRequestTestCase test_case;
  test_case.test_script = R"(
    (async function() {
      try {
        await document.body.requestPointerLock();
        return 'SUCCESS';
      } catch (err) {
        return `FAIL: ${err.name}: ${err.message}`;
      }
    })();
  )";
  test_case.permission_name = "pointerLock";
  test_case.embedder_content_settings_type.insert(
      ContentSettingsType::POINTER_LOCK);

  PermissionRequestTestParam test_param = GetParam();
  RunTestAndVerify(test_case, test_param);
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/
                         ,
                         ControlledFramePermissionRequestInteractiveTest,
                         testing::ValuesIn(
                             GetDefaultPermissionRequestTestParams()),
                         [](const testing::TestParamInfo<
                             PermissionRequestTestParam>& info) {
                           return info.param.name;
                         });
#endif  // !BUILDFLAG(IS_MAC)

}  // namespace controlled_frame

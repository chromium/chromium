// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/networking_attributes.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/networking_attributes.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"

using NetworkingAttributesLacrosBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(NetworkingAttributesLacrosBrowserTest,
                       GetNetworkDetails) {
  base::test::TestFuture<crosapi::mojom::GetNetworkDetailsResultPtr> future;
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::NetworkingAttributes>()
      ->GetNetworkDetails(future.GetCallback());

  // TODO(https://crbug.com/1207872): Write more robust tests. This API call
  // fails because the ash user is not affiliated with the device.
  ASSERT_TRUE(future.Take()->is_error_message());
}

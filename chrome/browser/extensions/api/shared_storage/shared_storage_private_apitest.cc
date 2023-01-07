// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "build/buildflag.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/containers/contains.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace extensions {

using SharedStoragePrivateApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateApiTest, Test) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto capabilities = chromeos::BrowserParamsProxy::Get()->AshCapabilities();
  if (!capabilities || !base::Contains(*capabilities, "b/231890240")) {
    LOG(WARNING) << "Unsupported ash version for shared storage.";
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  ASSERT_TRUE(RunExtensionTest("shared_storage_private")) << message_;
}

}  // namespace extensions

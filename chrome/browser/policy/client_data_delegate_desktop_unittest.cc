// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/client_data_delegate_desktop.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif  // BUILDFLAG(IS_WIN)

namespace policy {

TEST(ClientDataDelegateDesktopTest,
     FillRegisterBrowserRequest_BrowserDeviceIdentifier) {
#if BUILDFLAG(IS_WIN)
  base::win::ScopedCOMInitializer com_initializer;
#endif  // BUILDFLAG(IS_WIN)

  base::test::TaskEnvironment task_environment;

  ClientDataDelegateDesktop client_data_delegate;
  enterprise_management::RegisterBrowserRequest request;
  client_data_delegate.FillRegisterBrowserRequest(&request, base::DoNothing());
  task_environment.RunUntilIdle();

  EXPECT_EQ(request.machine_name(), GetMachineName());
  std::unique_ptr<enterprise_management::BrowserDeviceIdentifier>
      expected_browser_device_identifier = GetBrowserDeviceIdentifier();
  EXPECT_EQ(request.browser_device_identifier().computer_name(),
            expected_browser_device_identifier->computer_name());
  EXPECT_EQ(request.browser_device_identifier().serial_number(),
            expected_browser_device_identifier->serial_number());
}

}  // namespace policy

// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/wmi.h"

#include <windows.h>

#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

using Microsoft::WRL::ComPtr;

namespace base {
namespace win {

class WMITest : public ::testing::Test {
 private:
  ScopedCOMInitializer com_initializer;
};

TEST_F(WMITest, TestLocalConnectionSecurityBlanket) {
  ComPtr<IWbemServices> wmi_services = nullptr;
  EXPECT_TRUE(CreateLocalWmiConnection(true, &wmi_services));
  ASSERT_NE(wmi_services.Get(), nullptr);
  ULONG refs = wmi_services.Reset();
  EXPECT_EQ(0u, refs);
}

TEST_F(WMITest, TestLocalConnectionNoSecurityBlanket) {
  ComPtr<IWbemServices> wmi_services = nullptr;
  EXPECT_TRUE(CreateLocalWmiConnection(false, &wmi_services));
  ASSERT_NE(wmi_services.Get(), nullptr);
  ULONG refs = wmi_services.Reset();
  EXPECT_EQ(0u, refs);
}

TEST_F(WMITest, TestCreateClassMethod) {
  ComPtr<IWbemServices> wmi_services = nullptr;
  EXPECT_TRUE(CreateLocalWmiConnection(true, &wmi_services));
  ASSERT_NE(wmi_services.Get(), nullptr);
  ComPtr<IWbemClassObject> class_method = nullptr;
  EXPECT_TRUE(CreateWmiClassMethodObject(
      wmi_services.Get(), L"Win32_ShortcutFile", L"Rename", &class_method));
  ASSERT_NE(class_method.Get(), nullptr);
  ULONG refs = class_method.Reset();
  EXPECT_EQ(0u, refs);
  refs = wmi_services.Reset();
  EXPECT_EQ(0u, refs);
}

// Creates an instance of cmd which executes 'echo' and exits immediately.
TEST_F(WMITest, TestLaunchProcess) {
  int pid = 0;
  bool result = WmiLaunchProcess(L"cmd.exe /c echo excelent!", &pid);
  EXPECT_TRUE(result);
  EXPECT_GT(pid, 0);
}

TEST_F(WMITest, TestComputerSystemInfo) {
  WmiComputerSystemInfo info = WmiComputerSystemInfo::Get();
  EXPECT_FALSE(info.serial_number().empty());
}

}  // namespace win
}  // namespace base

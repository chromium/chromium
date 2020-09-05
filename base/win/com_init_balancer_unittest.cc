// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/com_init_balancer.h"

#include <shlobj.h>
#include <wrl/client.h>

#include "base/test/gtest_util.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

using Microsoft::WRL::ComPtr;

TEST(TestComInitBalancer, BalancedPairsWithComBalancerEnabled) {
  {
    // Assert COM has initialized correctly.
    ScopedCOMInitializer com_initializer(
        ScopedCOMInitializer::Uninitialization::kBlockPremature);
    ASSERT_TRUE(com_initializer.Succeeded());

    // Create COM object successfully.
    ComPtr<IUnknown> shell_link;
    HRESULT hr = ::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_ALL,
                                    IID_PPV_ARGS(&shell_link));
    EXPECT_TRUE(SUCCEEDED(hr));
  }

  // ScopedCOMInitializer has gone out of scope and COM has been uninitialized.
  EXPECT_DCHECK_DEATH(AssertComInitialized());
}

TEST(TestComInitBalancer, UnbalancedPairsWithComBalancerEnabled) {
  {
    // Assert COM has initialized correctly.
    ScopedCOMInitializer com_initializer(
        ScopedCOMInitializer::Uninitialization::kBlockPremature);
    ASSERT_TRUE(com_initializer.Succeeded());

    // Attempt to prematurely uninitialize the COM library.
    ::CoUninitialize();
    ::CoUninitialize();

    // Assert COM is still initialized.
    AssertComInitialized();

    // Create COM object successfully.
    ComPtr<IUnknown> shell_link;
    HRESULT hr = ::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_ALL,
                                    IID_PPV_ARGS(&shell_link));
    EXPECT_TRUE(SUCCEEDED(hr));
  }

  // ScopedCOMInitializer has gone out of scope and COM has been uninitialized.
  EXPECT_DCHECK_DEATH(AssertComInitialized());
}

TEST(TestComInitBalancer, BalancedPairsWithComBalancerDisabled) {
  {
    // Assert COM has initialized correctly.
    ScopedCOMInitializer com_initializer(
        ScopedCOMInitializer::Uninitialization::kAllow);
    ASSERT_TRUE(com_initializer.Succeeded());

    // Create COM object successfully.
    ComPtr<IUnknown> shell_link;
    HRESULT hr = ::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_ALL,
                                    IID_PPV_ARGS(&shell_link));
    EXPECT_TRUE(SUCCEEDED(hr));
  }

  // ScopedCOMInitializer has gone out of scope and COM has been uninitialized.
  EXPECT_DCHECK_DEATH(AssertComInitialized());
}

TEST(TestComInitBalancer, UnbalancedPairsWithComBalancerDisabled) {
  // Assert COM has initialized correctly.
  ScopedCOMInitializer com_initializer(
      ScopedCOMInitializer::Uninitialization::kAllow);
  ASSERT_TRUE(com_initializer.Succeeded());

  // Attempt to prematurely uninitialize the COM library.
  ::CoUninitialize();
  ::CoUninitialize();

  // Assert COM is not initialized.
  EXPECT_DCHECK_DEATH(AssertComInitialized());

  // Create COM object unsuccessfully.
  ComPtr<IUnknown> shell_link;
  HRESULT hr = ::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&shell_link));
  EXPECT_TRUE(FAILED(hr));
  EXPECT_EQ(CO_E_NOTINITIALIZED, hr);
}

TEST(TestComInitBalancer, OneRegisteredSpyRefCount) {
  ScopedCOMInitializer com_initializer(
      ScopedCOMInitializer::Uninitialization::kBlockPremature);
  ASSERT_TRUE(com_initializer.Succeeded());

  // Reference count should be 1 after initialization.
  EXPECT_EQ(DWORD(1), com_initializer.GetCOMBalancerReferenceCountForTesting());

  // Attempt to prematurely uninitialize the COM library.
  ::CoUninitialize();

  // Expect reference count to remain at 1.
  EXPECT_EQ(DWORD(1), com_initializer.GetCOMBalancerReferenceCountForTesting());
}

TEST(TestComInitBalancer, ThreeRegisteredSpiesRefCount) {
  ScopedCOMInitializer com_initializer_1(
      ScopedCOMInitializer::Uninitialization::kBlockPremature);
  ScopedCOMInitializer com_initializer_2(
      ScopedCOMInitializer::Uninitialization::kBlockPremature);
  ScopedCOMInitializer com_initializer_3(
      ScopedCOMInitializer::Uninitialization::kBlockPremature);
  ASSERT_TRUE(com_initializer_1.Succeeded());
  ASSERT_TRUE(com_initializer_2.Succeeded());
  ASSERT_TRUE(com_initializer_3.Succeeded());

  // Reference count should be 3 after initialization.
  EXPECT_EQ(DWORD(3),
            com_initializer_1.GetCOMBalancerReferenceCountForTesting());
  EXPECT_EQ(DWORD(3),
            com_initializer_2.GetCOMBalancerReferenceCountForTesting());
  EXPECT_EQ(DWORD(3),
            com_initializer_3.GetCOMBalancerReferenceCountForTesting());

  // Attempt to prematurely uninitialize the COM library.
  ::CoUninitialize();  // Reference count -> 2.
  ::CoUninitialize();  // Reference count -> 1.
  ::CoUninitialize();

  // Expect reference count to remain at 1.
  EXPECT_EQ(DWORD(1),
            com_initializer_1.GetCOMBalancerReferenceCountForTesting());
  EXPECT_EQ(DWORD(1),
            com_initializer_2.GetCOMBalancerReferenceCountForTesting());
  EXPECT_EQ(DWORD(1),
            com_initializer_3.GetCOMBalancerReferenceCountForTesting());
}

}  // namespace win
}  // namespace base

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/com_init_check_hook.h"

#include <objbase.h>
#include <shlobj.h>
#include <wrl/client.h>

#include "base/test/gtest_util.h"
#include "base/win/com_init_util.h"
#include "base/win/patch_util.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

using Microsoft::WRL::ComPtr;

TEST(ComInitCheckHook, AssertNotInitialized) {
  ComInitCheckHook com_check_hook;
  AssertComApartmentType(ComApartmentType::NONE);
  ComPtr<IUnknown> shell_link;
#if defined(COM_INIT_CHECK_HOOK_ENABLED)
  EXPECT_DCHECK_DEATH(::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_ALL,
                                         IID_PPV_ARGS(&shell_link)));
#else
  EXPECT_EQ(CO_E_NOTINITIALIZED,
            ::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_ALL,
                               IID_PPV_ARGS(&shell_link)));
#endif
}

TEST(ComInitCheckHook, HookRemoval) {
  AssertComApartmentType(ComApartmentType::NONE);
  { ComInitCheckHook com_check_hook; }
  ComPtr<IUnknown> shell_link;
  EXPECT_EQ(CO_E_NOTINITIALIZED,
            ::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_ALL,
                               IID_PPV_ARGS(&shell_link)));
}

TEST(ComInitCheckHook, NoAssertComInitialized) {
  ComInitCheckHook com_check_hook;
  ScopedCOMInitializer com_initializer;
  ComPtr<IUnknown> shell_link;
  EXPECT_TRUE(SUCCEEDED(::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_ALL,
                                           IID_PPV_ARGS(&shell_link))));
}

TEST(ComInitCheckHook, MultipleHooks) {
  ComInitCheckHook com_check_hook_1;
  ComInitCheckHook com_check_hook_2;
  AssertComApartmentType(ComApartmentType::NONE);
  ComPtr<IUnknown> shell_link;
#if defined(COM_INIT_CHECK_HOOK_ENABLED)
  EXPECT_DCHECK_DEATH(::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_ALL,
                                         IID_PPV_ARGS(&shell_link)));
#else
  EXPECT_EQ(CO_E_NOTINITIALIZED,
            ::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_ALL,
                               IID_PPV_ARGS(&shell_link)));
#endif
}

TEST(ComInitCheckHook, UnexpectedHook) {
#if defined(COM_INIT_CHECK_HOOK_ENABLED)
  HMODULE ole32_library = ::LoadLibrary(L"ole32.dll");
  ASSERT_TRUE(ole32_library);

  uint32_t co_create_instance_padded_address =
      reinterpret_cast<uint32_t>(
          GetProcAddress(ole32_library, "CoCreateInstance")) -
      5;
  const unsigned char* co_create_instance_bytes =
      reinterpret_cast<const unsigned char*>(co_create_instance_padded_address);
  const unsigned char original_byte = co_create_instance_bytes[0];
  const unsigned char unexpected_byte = 0xdb;
  ASSERT_EQ(static_cast<DWORD>(NO_ERROR),
            internal::ModifyCode(
                reinterpret_cast<void*>(co_create_instance_padded_address),
                reinterpret_cast<const void*>(&unexpected_byte),
                sizeof(unexpected_byte)));

  EXPECT_DCHECK_DEATH({ ComInitCheckHook com_check_hook; });

  // If this call fails, really bad things are going to happen to other tests
  // so CHECK here.
  CHECK_EQ(static_cast<DWORD>(NO_ERROR),
           internal::ModifyCode(
               reinterpret_cast<void*>(co_create_instance_padded_address),
               reinterpret_cast<const void*>(&original_byte),
               sizeof(original_byte)));

  ::FreeLibrary(ole32_library);
  ole32_library = nullptr;
#endif
}

TEST(ComInitCheckHook, ExternallyHooked) {
#if defined(COM_INIT_CHECK_HOOK_ENABLED)
  HMODULE ole32_library = ::LoadLibrary(L"ole32.dll");
  ASSERT_TRUE(ole32_library);

  uint32_t co_create_instance_address = reinterpret_cast<uint32_t>(
      GetProcAddress(ole32_library, "CoCreateInstance"));
  const unsigned char* co_create_instance_bytes =
      reinterpret_cast<const unsigned char*>(co_create_instance_address);
  const unsigned char original_byte = co_create_instance_bytes[0];
  const unsigned char jmp_byte = 0xe9;
  ASSERT_EQ(static_cast<DWORD>(NO_ERROR),
            internal::ModifyCode(
                reinterpret_cast<void*>(co_create_instance_address),
                reinterpret_cast<const void*>(&jmp_byte), sizeof(jmp_byte)));

  // Externally patched instances should crash so we catch these cases on bots.
  EXPECT_DCHECK_DEATH({ ComInitCheckHook com_check_hook; });

  // If this call fails, really bad things are going to happen to other tests
  // so CHECK here.
  CHECK_EQ(
      static_cast<DWORD>(NO_ERROR),
      internal::ModifyCode(reinterpret_cast<void*>(co_create_instance_address),
                           reinterpret_cast<const void*>(&original_byte),
                           sizeof(original_byte)));

  ::FreeLibrary(ole32_library);
  ole32_library = nullptr;
#endif
}

TEST(ComInitCheckHook, UnexpectedChangeDuringHook) {
#if defined(COM_INIT_CHECK_HOOK_ENABLED)
  HMODULE ole32_library = ::LoadLibrary(L"ole32.dll");
  ASSERT_TRUE(ole32_library);

  uint32_t co_create_instance_padded_address =
      reinterpret_cast<uint32_t>(
          GetProcAddress(ole32_library, "CoCreateInstance")) -
      5;
  const unsigned char* co_create_instance_bytes =
      reinterpret_cast<const unsigned char*>(co_create_instance_padded_address);
  const unsigned char original_byte = co_create_instance_bytes[0];
  const unsigned char unexpected_byte = 0xdb;
  ASSERT_EQ(static_cast<DWORD>(NO_ERROR),
            internal::ModifyCode(
                reinterpret_cast<void*>(co_create_instance_padded_address),
                reinterpret_cast<const void*>(&unexpected_byte),
                sizeof(unexpected_byte)));

  EXPECT_DCHECK_DEATH({
    ComInitCheckHook com_check_hook;

    internal::ModifyCode(
        reinterpret_cast<void*>(co_create_instance_padded_address),
        reinterpret_cast<const void*>(&unexpected_byte),
        sizeof(unexpected_byte));
  });

  // If this call fails, really bad things are going to happen to other tests
  // so CHECK here.
  CHECK_EQ(static_cast<DWORD>(NO_ERROR),
           internal::ModifyCode(
               reinterpret_cast<void*>(co_create_instance_padded_address),
               reinterpret_cast<const void*>(&original_byte),
               sizeof(original_byte)));

  ::FreeLibrary(ole32_library);
  ole32_library = nullptr;
#endif
}

}  // namespace win
}  // namespace base

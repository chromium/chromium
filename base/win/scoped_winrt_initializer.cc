// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_winrt_initializer.h"

#include <roapi.h>
#include <windows.h>

#include "base/check_op.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/windows_version.h"

namespace base {
namespace win {

namespace {

FARPROC LoadComBaseFunction(const char* function_name) {
  static HMODULE const handle = []() {
    // Mitigate the issues caused by loading DLLs on a background thread
    // (http://crbug/973868).
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    return ::LoadLibraryEx(L"combase.dll", nullptr,
                           LOAD_LIBRARY_SEARCH_SYSTEM32);
  }();
  return handle ? ::GetProcAddress(handle, function_name) : nullptr;
}

decltype(&::RoInitialize) GetRoInitializeFunction() {
  static decltype(&::RoInitialize) const function =
      reinterpret_cast<decltype(&::RoInitialize)>(
          LoadComBaseFunction("RoInitialize"));
  return function;
}

decltype(&::RoUninitialize) GetRoUninitializeFunction() {
  static decltype(&::RoUninitialize) const function =
      reinterpret_cast<decltype(&::RoUninitialize)>(
          LoadComBaseFunction("RoUninitialize"));
  return function;
}

HRESULT CallRoInitialize(RO_INIT_TYPE init_type) {
  auto ro_initialize_func = GetRoInitializeFunction();
  if (!ro_initialize_func)
    return E_FAIL;
  return ro_initialize_func(init_type);
}

void CallRoUninitialize() {
  auto ro_uninitialize_func = GetRoUninitializeFunction();
  if (ro_uninitialize_func)
    ro_uninitialize_func();
}

}  // namespace

ScopedWinrtInitializer::ScopedWinrtInitializer()
    : hr_(CallRoInitialize(RO_INIT_MULTITHREADED)) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GE(GetVersion(), Version::WIN8);
#if DCHECK_IS_ON()
  if (SUCCEEDED(hr_))
    AssertComApartmentType(ComApartmentType::MTA);
  else
    DCHECK_NE(RPC_E_CHANGED_MODE, hr_) << "Invalid COM thread model change";
#endif
}

ScopedWinrtInitializer::~ScopedWinrtInitializer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (SUCCEEDED(hr_))
    CallRoUninitialize();
}

bool ScopedWinrtInitializer::Succeeded() const {
  return SUCCEEDED(hr_);
}

}  // namespace win
}  // namespace base

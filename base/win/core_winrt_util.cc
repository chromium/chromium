// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/core_winrt_util.h"

#include "base/threading/scoped_thread_priority.h"

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

decltype(&::RoActivateInstance) GetRoActivateInstanceFunction() {
  static decltype(&::RoActivateInstance) const function =
      reinterpret_cast<decltype(&::RoActivateInstance)>(
          LoadComBaseFunction("RoActivateInstance"));
  return function;
}

decltype(&::RoGetActivationFactory) GetRoGetActivationFactoryFunction() {
  static decltype(&::RoGetActivationFactory) const function =
      reinterpret_cast<decltype(&::RoGetActivationFactory)>(
          LoadComBaseFunction("RoGetActivationFactory"));
  return function;
}

}  // namespace

namespace base {
namespace win {

bool ResolveCoreWinRTDelayload() {
  // TODO(finnur): Add AssertIOAllowed once crbug.com/770193 is fixed.
  return GetRoActivateInstanceFunction() && GetRoGetActivationFactoryFunction();
}

HRESULT RoGetActivationFactory(HSTRING class_id,
                               const IID& iid,
                               void** out_factory) {
  auto get_factory_func = GetRoGetActivationFactoryFunction();
  if (!get_factory_func)
    return E_FAIL;
  return get_factory_func(class_id, iid, out_factory);
}

HRESULT RoActivateInstance(HSTRING class_id, IInspectable** instance) {
  auto activate_instance_func = GetRoActivateInstanceFunction();
  if (!activate_instance_func)
    return E_FAIL;
  return activate_instance_func(class_id, instance);
}

}  // namespace win
}  // namespace base

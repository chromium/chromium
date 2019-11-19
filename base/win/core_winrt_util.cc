// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/core_winrt_util.h"

namespace {

FARPROC LoadComBaseFunction(const char* function_name) {
  static HMODULE const handle =
      ::LoadLibraryEx(L"combase.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
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
  return GetRoInitializeFunction() && GetRoUninitializeFunction() &&
         GetRoActivateInstanceFunction() && GetRoGetActivationFactoryFunction();
}

HRESULT RoInitialize(RO_INIT_TYPE init_type) {
  auto ro_initialize_func = GetRoInitializeFunction();
  if (!ro_initialize_func)
    return E_FAIL;
  return ro_initialize_func(init_type);
}

void RoUninitialize() {
  auto ro_uninitialize_func = GetRoUninitializeFunction();
  if (ro_uninitialize_func)
    ro_uninitialize_func();
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

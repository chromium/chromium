// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/wmi.h"

#include <objbase.h>

#include <windows.h>

#include <stdint.h>

#include <string_view>
#include <utility>

#include "base/location.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"

using Microsoft::WRL::ComPtr;

namespace base {
namespace win {

const wchar_t kCimV2ServerName[] = L"ROOT\\CIMV2";

const wchar_t kSecurityCenter2ServerName[] = L"ROOT\\SecurityCenter2";

namespace {

constexpr wchar_t kSerialNumberQuery[] = L"SELECT SerialNumber FROM Win32_Bios";

// Instantiates `wmi_services` with a connection to `server_name` in WMI. Will
// set a security blanket if `set_blanket` is true.
std::optional<WmiError> CreateLocalWmiConnection(
    bool set_blanket,
    const std::wstring& server_name,
    ComPtr<IWbemServices>* wmi_services) {
  DCHECK(wmi_services);
  ComPtr<IWbemLocator> wmi_locator;
  HRESULT hr =
      ::CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&wmi_locator));
  if (FAILED(hr))
    return WmiError::kFailedToCreateInstance;

  ComPtr<IWbemServices> wmi_services_r;
  hr = wmi_locator->ConnectServer(base::win::ScopedBstr(server_name).Get(),
                                  nullptr, nullptr, nullptr, 0, nullptr,
                                  nullptr, &wmi_services_r);
  if (FAILED(hr))
    return WmiError::kFailedToConnectToWMI;

  if (set_blanket) {
    hr = ::CoSetProxyBlanket(wmi_services_r.Get(), RPC_C_AUTHN_WINNT,
                             RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                             RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (FAILED(hr))
      return WmiError::kFailedToSetSecurityBlanket;
  }

  *wmi_services = std::move(wmi_services_r);
  return std::nullopt;
}

// Runs `query` through `wmi_services` and sets the results' `enumerator`.
bool TryRunQuery(const std::wstring& query,
                 const ComPtr<IWbemServices>& wmi_services,
                 ComPtr<IEnumWbemClassObject>* enumerator) {
  DCHECK(enumerator);
  base::win::ScopedBstr query_language(L"WQL");
  base::win::ScopedBstr query_bstr(query);

  ComPtr<IEnumWbemClassObject> enumerator_r;
  HRESULT hr = wmi_services->ExecQuery(
      query_language.Get(), query_bstr.Get(),
      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
      &enumerator_r);

  if (FAILED(hr))
    return false;

  *enumerator = std::move(enumerator_r);
  return true;
}

}  // namespace

std::optional<WmiError> RunWmiQuery(const std::wstring& server_name,
                                    const std::wstring& query,
                                    ComPtr<IEnumWbemClassObject>* enumerator) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  DCHECK(enumerator);

  ComPtr<IWbemServices> wmi_services;
  auto error = CreateLocalWmiConnection(/*set_blanket=*/true, server_name,
                                        &wmi_services);

  if (error.has_value())
    return error;

  if (!TryRunQuery(query, wmi_services, enumerator))
    return WmiError::kFailedToExecWMIQuery;

  return std::nullopt;
}

bool CreateLocalWmiConnection(bool set_blanket,
                              ComPtr<IWbemServices>* wmi_services) {
  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  auto error =
      CreateLocalWmiConnection(set_blanket, kCimV2ServerName, wmi_services);
  return !error.has_value();
}

ComPtr<IWbemServices> CreateWmiConnection(bool set_blanket,
                                          const std::wstring& resource) {
  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  ComPtr<IWbemServices> wmi_services = nullptr;
  auto error = CreateLocalWmiConnection(set_blanket, resource, &wmi_services);
  if (error.has_value())
    return nullptr;
  return wmi_services;
}

bool CreateWmiClassMethodObject(IWbemServices* wmi_services,
                                std::wstring_view class_name,
                                std::wstring_view method_name,
                                ComPtr<IWbemClassObject>* class_instance) {
  // We attempt to instantiate a COM object that represents a WMI object plus
  // a method rolled into one entity.
  ScopedBstr b_class_name(class_name);
  ScopedBstr b_method_name(method_name);
  ComPtr<IWbemClassObject> class_object;
  HRESULT hr;
  hr = wmi_services->GetObject(b_class_name.Get(), 0, nullptr, &class_object,
                               nullptr);
  if (FAILED(hr))
    return false;

  ComPtr<IWbemClassObject> params_def;
  hr = class_object->GetMethod(b_method_name.Get(), 0, &params_def, nullptr);
  if (FAILED(hr))
    return false;

  if (!params_def.Get()) {
    // You hit this special case if the WMI class is not a CIM class. MSDN
    // sometimes tells you this. Welcome to WMI hell.
    return false;
  }

  hr = params_def->SpawnInstance(0, &(*class_instance));
  return SUCCEEDED(hr);
}

// The code in Launch() basically calls the Create Method of the Win32_Process
// CIM class is documented here:
// http://msdn2.microsoft.com/en-us/library/aa389388(VS.85).aspx
// NOTE: The documentation for the Create method suggests that the ProcessId
// parameter and return value are of type uint32_t, but when we call the method
// the values in the returned out_params, are VT_I4, which is int32_t.
bool WmiLaunchProcess(const std::wstring& command_line, int* process_id) {
  ComPtr<IWbemServices> wmi_local;
  if (!CreateLocalWmiConnection(true, &wmi_local))
    return false;

  static constexpr wchar_t class_name[] = L"Win32_Process";
  static constexpr wchar_t method_name[] = L"Create";
  ComPtr<IWbemClassObject> process_create;
  if (!CreateWmiClassMethodObject(wmi_local.Get(), class_name, method_name,
                                  &process_create)) {
    return false;
  }

  ScopedVariant b_command_line(command_line.c_str());

  if (FAILED(process_create->Put(L"CommandLine", 0, b_command_line.AsInput(),
                                 0))) {
    return false;
  }

  ComPtr<IWbemClassObject> out_params;
  HRESULT hr = wmi_local->ExecMethod(
      ScopedBstr(class_name).Get(), ScopedBstr(method_name).Get(), 0, nullptr,
      process_create.Get(), &out_params, nullptr);
  if (FAILED(hr))
    return false;

  // We're only expecting int32_t or uint32_t values, so no need for
  // ScopedVariant.
  VARIANT ret_value = {{{VT_EMPTY}}};
  hr = out_params->Get(L"ReturnValue", 0, &ret_value, nullptr, nullptr);
  if (FAILED(hr) || V_I4(&ret_value) != 0)
    return false;

  VARIANT pid = {{{VT_EMPTY}}};
  hr = out_params->Get(L"ProcessId", 0, &pid, nullptr, nullptr);
  if (FAILED(hr) || V_I4(&pid) == 0)
    return false;

  if (process_id)
    *process_id = V_I4(&pid);

  return true;
}

// static
WmiComputerSystemInfo WmiComputerSystemInfo::Get() {
  static const base::NoDestructor<WmiComputerSystemInfo> static_info([] {
    WmiComputerSystemInfo info;
    ComPtr<IEnumWbemClassObject> enumerator_bios;
    auto error =
        RunWmiQuery(kCimV2ServerName, kSerialNumberQuery, &enumerator_bios);
    if (!error.has_value())
      info.PopulateSerialNumber(enumerator_bios);
    return info;
  }());
  return *static_info;
}

void WmiComputerSystemInfo::PopulateSerialNumber(
    const ComPtr<IEnumWbemClassObject>& enumerator_bios) {
  ComPtr<IWbemClassObject> class_obj;
  ULONG items_returned = 0;
  HRESULT hr =
      enumerator_bios->Next(WBEM_INFINITE, 1, &class_obj, &items_returned);
  if (FAILED(hr) || !items_returned)
    return;

  ScopedVariant serial_number;
  hr = class_obj->Get(L"SerialNumber", 0, serial_number.Receive(), nullptr,
                      nullptr);
  if (SUCCEEDED(hr) && serial_number.type() == VT_BSTR) {
    serial_number_.assign(V_BSTR(serial_number.ptr()),
                          ::SysStringLen(V_BSTR(serial_number.ptr())));
  }
}

}  // namespace win
}  // namespace base

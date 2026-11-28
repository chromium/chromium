// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/xsmp_api_linux.h"

#include <dlfcn.h>

#include <optional>
#include <tuple>

#include "base/compiler_specific.h"
#include "base/scoped_native_library.h"

namespace xsmp {

namespace {

using IceErrorHandler =
    void (*)(IceConn, int, int, unsigned long, int, int, IcePointer);
using IceIOErrorHandler = void (*)(IceConn);
using SmcErrorHandler =
    void (*)(SmcConn, int, int, unsigned long, int, int, SmPointer);

void OnIceIOError(IceConn /*ice_conn*/) {}

void OnIceError(IceConn /*ice_conn*/,
                int /*swap*/,
                int /*offending_minor_opcode*/,
                unsigned long /*offending_sequence*/,
                int /*error_class*/,
                int /*severity*/,
                IcePointer /*values*/) {}

void OnSmcError(SmcConn /*smc_conn*/,
                int /*swap*/,
                int /*offending_minor_opcode*/,
                unsigned long /*offending_sequence*/,
                int /*error_class*/,
                int /*severity*/,
                SmPointer /*values*/) {}

template <typename Fn>
bool LoadSymbol(const base::ScopedNativeLibrary& lib,
                const char* name,
                Fn& fn) {
  fn = reinterpret_cast<Fn>(lib.GetFunctionPointer(name));
  return fn != nullptr;
}

DISABLE_CFI_DLSYM std::optional<XsmpApi> Load() {
  base::ScopedNativeLibrary ice_lib(
      dlopen("libICE.so.6", RTLD_LAZY | RTLD_LOCAL));
  if (!ice_lib.is_valid()) {
    return std::nullopt;
  }
  base::ScopedNativeLibrary sm_lib(
      dlopen("libSM.so.6", RTLD_LAZY | RTLD_LOCAL));
  if (!sm_lib.is_valid()) {
    return std::nullopt;
  }

  XsmpApi api;
  IceIOErrorHandler (*set_ice_io_err)(IceIOErrorHandler) = nullptr;
  IceErrorHandler (*set_ice_err)(IceErrorHandler) = nullptr;
  SmcErrorHandler (*set_smc_err)(SmcErrorHandler) = nullptr;
  if (!LoadSymbol(ice_lib, "IceConnectionNumber", api.connection_number) ||
      !LoadSymbol(ice_lib, "IceProcessMessages", api.process_messages) ||
      !LoadSymbol(ice_lib, "IceSetIOErrorHandler", set_ice_io_err) ||
      !LoadSymbol(ice_lib, "IceSetErrorHandler", set_ice_err) ||
      !LoadSymbol(sm_lib, "SmcOpenConnection", api.open_connection) ||
      !LoadSymbol(sm_lib, "SmcCloseConnection", api.close_connection) ||
      !LoadSymbol(sm_lib, "SmcGetIceConnection", api.get_ice_connection) ||
      !LoadSymbol(sm_lib, "SmcSaveYourselfDone", api.save_yourself_done) ||
      !LoadSymbol(sm_lib, "SmcSetErrorHandler", set_smc_err) ||
      !LoadSymbol(sm_lib, "SmcSetProperties", api.set_properties)) {
    return std::nullopt;
  }

  // Ensure a crashed or misbehaving session manager never terminates the
  // browser.
  set_ice_io_err(&OnIceIOError);
  set_ice_err(&OnIceError);
  set_smc_err(&OnSmcError);

  // Keep both libraries loaded for the lifetime of the process since `api`
  // holds function pointers into them.
  std::ignore = ice_lib.release();
  std::ignore = sm_lib.release();
  return api;
}

}  // namespace

// static
const XsmpApi* XsmpApi::Get() {
  static const std::optional<XsmpApi> api = Load();
  return api ? &*api : nullptr;
}

}  // namespace xsmp

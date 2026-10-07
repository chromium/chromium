// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_WRL_MODULE_H_
#define BASE_WIN_WRL_MODULE_H_

// <wrl/module.h> incorrectly ignores __WRL_CLASSIC_COM_STRICT__ and calls
// RoOriginateError for
//   * Module::RegisterWinRTObject
//   * Module::UnregisterWinRTObject
//   * Module::RegisterCOMObject
// Using an NTDDI_VERSION < NTDDI_WINBLUE works around this bug.
#ifdef NTDDI_VERSION
#define OLD_NTDDI_VERSION NTDDI_VERSION
#undef NTDDI_VERSION
#endif

#define NTDDI_VERSION NTDDI_WIN8
#define __WRL_DISABLE_STATIC_INITIALIZE__
#define __WRL_CLASSIC_COM_STRICT__
#include <wrl/module.h>
#undef __WRL_CLASSIC_COM_STRICT__
#undef __WRL_DISABLE_STATIC_INITIALIZE__
#undef NTDDI_VERSION

#ifdef OLD_NTDDI_VERSION
#define NTDDI_VERSION OLD_NTDDI_VERSION
#undef OLD_NTDDI_VERSION
#endif

#endif  // BASE_WIN_WRL_MODULE_H_

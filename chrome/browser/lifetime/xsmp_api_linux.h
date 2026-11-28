// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_XSMP_API_LINUX_H_
#define CHROME_BROWSER_LIFETIME_XSMP_API_LINUX_H_

#include "base/memory/raw_ptr_exclusion.h"

struct _IceConn;
struct _SmcConn;

// Minimal X11R6 ICE / XSMP client C ABI definitions (from <X11/ICE/ICElib.h>
// and <X11/SM/SMlib.h>), declared directly because libICE.so.6 and libSM.so.6
// are loaded via dlopen() and <X11/*> headers are disallowed by presubmit.
namespace xsmp {

using IcePointer = void*;
using SmPointer = void*;
using IceConn = _IceConn*;
using SmcConn = _SmcConn*;

enum IceProcessMessagesStatus {
  IceProcessMessagesSuccess = 0,
  IceProcessMessagesIOError = 1,
  IceProcessMessagesConnectionClosed = 2,
};

enum SmcCloseStatus {
  SmcClosedNow = 0,
  SmcClosedASAP = 1,
  SmcConnectionInUse = 2,
};

struct SmPropValue {
  int length;
  SmPointer value;
};

struct SmProp {
  RAW_PTR_EXCLUSION char* name;
  RAW_PTR_EXCLUSION char* type;
  int num_vals;
  RAW_PTR_EXCLUSION SmPropValue* vals;
};

using SmcSaveYourselfProc = void (*)(SmcConn, SmPointer, int, int, int, int);
using SmcDieProc = void (*)(SmcConn, SmPointer);
using SmcSaveCompleteProc = void (*)(SmcConn, SmPointer);
using SmcShutdownCancelledProc = void (*)(SmcConn, SmPointer);

struct SmcCallbacks {
  struct {
    SmcSaveYourselfProc callback;
    SmPointer client_data;
  } save_yourself;
  struct {
    SmcDieProc callback;
    SmPointer client_data;
  } die;
  struct {
    SmcSaveCompleteProc callback;
    SmPointer client_data;
  } save_complete;
  struct {
    SmcShutdownCancelledProc callback;
    SmPointer client_data;
  } shutdown_cancelled;
};

inline constexpr int SmProtoMajor = 1;
inline constexpr int SmProtoMinor = 0;
inline constexpr int SmSaveGlobal = 0;
inline constexpr unsigned char SmRestartIfRunning = 0;

inline constexpr unsigned long SmcSaveYourselfProcMask = 1UL << 0;
inline constexpr unsigned long SmcDieProcMask = 1UL << 1;
inline constexpr unsigned long SmcSaveCompleteProcMask = 1UL << 2;
inline constexpr unsigned long SmcShutdownCancelledProcMask = 1UL << 3;

inline constexpr char SmCloneCommand[] = "CloneCommand";
inline constexpr char SmProgram[] = "Program";
inline constexpr char SmRestartCommand[] = "RestartCommand";
inline constexpr char SmRestartStyleHint[] = "RestartStyleHint";
inline constexpr char SmUserID[] = "UserID";
inline constexpr char SmCARD8[] = "CARD8";
inline constexpr char SmARRAY8[] = "ARRAY8";
inline constexpr char SmLISTofARRAY8[] = "LISTofARRAY8";

// The libICE.so.6 and libSM.so.6 client entry points, loaded at runtime so
// systems without the X11 session management libraries do not need them
// installed. Callers must be annotated with DISABLE_CFI_DLSYM.
struct XsmpApi {
  // Returns the loaded API, or nullptr if either library or any symbol is
  // missing. The first successful call replaces the default libICE and libSM
  // error handlers, which call exit(1), with handlers that do nothing.
  static const XsmpApi* Get();

  int (*connection_number)(IceConn) = nullptr;
  IceProcessMessagesStatus (*process_messages)(IceConn, void*, int*) = nullptr;
  SmcConn (*open_connection)(char*,
                             SmPointer,
                             int,
                             int,
                             unsigned long,
                             SmcCallbacks*,
                             const char*,
                             char**,
                             int,
                             char*) = nullptr;
  SmcCloseStatus (*close_connection)(SmcConn, int, char**) = nullptr;
  IceConn (*get_ice_connection)(SmcConn) = nullptr;
  void (*save_yourself_done)(SmcConn, int) = nullptr;
  void (*set_properties)(SmcConn, int, SmProp**) = nullptr;
};

}  // namespace xsmp

#endif  // CHROME_BROWSER_LIFETIME_XSMP_API_LINUX_H_

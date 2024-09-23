// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#import "chrome/browser/mac/exception_processor.h"

#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <libunwind.h>
#include <objc/objc-exception.h>

#include <type_traits>

#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/crash/core/common/crash_key.h"

static objc_exception_preprocessor g_next_preprocessor = nullptr;

// It is believed that crashing on exceptions is breaking bucketing on the crash
// server; see the analysis on https://crbug.com/358138966. Disable it by
// default for now, while leaving an emergency re-enablement switch, and remove
// it entirely in a few releases.
BASE_FEATURE(kForceCrashOnExceptions,
             "ForceCrashOnExceptions",
             base::FEATURE_DISABLED_BY_DEFAULT);

static const char* const kExceptionSinkholes[] = {
  "CFRunLoopRunSpecific",
  "_CFXNotificationPost",
  "__CFRunLoopRun",
  "__NSFireDelayedPerform",
  "_dispatch_client_callout",
};

// This function is used to make it clear to the crash processor that this is
// a forced exception crash.
NOINLINE static void TERMINATING_FROM_UNCAUGHT_NSEXCEPTION(id exception) {
  NSString* exception_message_ns = [NSString
      stringWithFormat:@"%@: %@", [exception name], [exception reason]];
  std::string exception_message = base::SysNSStringToUTF8(exception_message_ns);

  static crash_reporter::CrashKeyString<256> crash_key("nsexception");
  crash_key.Set(exception_message);

  LOG(FATAL) << "Terminating from Objective-C exception: " << exception_message;
}

static id ObjcExceptionPreprocessor(id exception) {
  static bool seen_first_exception = false;

  static crash_reporter::CrashKeyString<256> firstexception("firstexception");
  static crash_reporter::CrashKeyString<256> lastexception("lastexception");

  static crash_reporter::CrashKeyString<1024> firstexception_bt(
      "firstexception_bt");
  static crash_reporter::CrashKeyString<1024> lastexception_bt(
      "lastexception_bt");

  auto* key = seen_first_exception ? &lastexception : &firstexception;
  auto* bt_key = seen_first_exception ? &lastexception_bt : &firstexception_bt;

  NSString* value = [NSString stringWithFormat:@"%@ reason %@",
      [exception name], [exception reason]];
  key->Set(base::SysNSStringToUTF8(value));

  // This exception preprocessor runs prior to the one in libobjc, which sets
  // the -[NSException callStackReturnAddresses].
  crash_reporter::SetCrashKeyStringToStackTrace(bt_key,
                                                base::debug::StackTrace());

  seen_first_exception = true;

  if (base::FeatureList::IsEnabled(kForceCrashOnExceptions)) {
    // Unwind the stack looking for any exception handlers. If an exception
    // handler is encountered, test to see if it is a function known to catch-
    // and-rethrow as a "top-level" exception handler. Various routines in
    // Cocoa do this, and it obscures the crashing stack, since the original
    // throw location is no longer present on the stack (just the re-throw) when
    // Crashpad captures the crash report.
    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local(&cursor, &context);

    // Get the base address for the image that contains this function.
    Dl_info dl_info;
    const void* this_base_address = 0;
    if (dladdr(reinterpret_cast<const void*>(&ObjcExceptionPreprocessor),
               &dl_info) != 0) {
      this_base_address = dl_info.dli_fbase;
    }

    while (unw_step(&cursor) > 0) {
      unw_proc_info_t frame_info;
      if (unw_get_proc_info(&cursor, &frame_info) != UNW_ESUCCESS) {
        continue;
      }

      // This frame has an exception handler.
      if (frame_info.handler != 0) {
        char proc_name[64];
        unw_word_t offset;
        if (unw_get_proc_name(&cursor, proc_name, sizeof(proc_name), &offset) !=
            UNW_ESUCCESS) {
          // The symbol has no name, so see if it belongs to the same image as
          // this function.
          if (dladdr(reinterpret_cast<const void*>(frame_info.start_ip),
                     &dl_info) != 0) {
            if (dl_info.dli_fbase == this_base_address) {
              // This is a handler in our image, so allow it to run.
              break;
            }
          }

          // This handler does not belong to us, so continue the search.
          continue;
        }

        // Check if the function is one that is known to obscure (by way of
        // catch-and-rethrow) exception stack traces. If it is, sinkhole it
        // by crashing here at the point of throw.
        for (const char* sinkhole : kExceptionSinkholes) {
          if (strcmp(sinkhole, proc_name) == 0) {
            TERMINATING_FROM_UNCAUGHT_NSEXCEPTION(exception);
          }
        }

        VLOG(1) << "Stopping search for exception handler at " << proc_name;

        break;
      }
    }
  }

  // Forward to the next preprocessor.
  if (g_next_preprocessor)
    return g_next_preprocessor(exception);

  return exception;
}

void InstallObjcExceptionPreprocessor() {
  if (g_next_preprocessor)
    return;

  g_next_preprocessor =
      objc_setExceptionPreprocessor(&ObjcExceptionPreprocessor);
}

void UninstallObjcExceptionPreprocessor() {
  objc_setExceptionPreprocessor(g_next_preprocessor);
  g_next_preprocessor = nullptr;
}

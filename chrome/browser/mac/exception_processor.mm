// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/mac/exception_processor.h"

#import <Foundation/Foundation.h>
#include <objc/objc-exception.h>

#include "base/debug/stack_trace.h"
#include "base/strings/sys_string_conversions.h"
#include "components/crash/core/common/crash_key.h"

namespace {

objc_exception_preprocessor g_next_preprocessor = nullptr;

bool g_first_exception_seen = false;

id ObjcExceptionPreprocessor(id exception) {
  static crash_reporter::CrashKeyString<256> firstexception("firstexception");
  static crash_reporter::CrashKeyString<256> lastexception("lastexception");

  static crash_reporter::CrashKeyString<1024> firstexception_bt(
      "firstexception_bt");
  static crash_reporter::CrashKeyString<1024> lastexception_bt(
      "lastexception_bt");

  auto* key = g_first_exception_seen ? &lastexception : &firstexception;
  auto* bt_key =
      g_first_exception_seen ? &lastexception_bt : &firstexception_bt;

  NSString* value = [NSString
      stringWithFormat:@"%@ reason %@", [exception name], [exception reason]];
  key->Set(base::SysNSStringToUTF8(value));

  // This exception preprocessor runs prior to the one in libobjc, which sets
  // the -[NSException callStackReturnAddresses].
  crash_reporter::SetCrashKeyStringToStackTrace(bt_key,
                                                base::debug::StackTrace());

  g_first_exception_seen = true;

  // Forward to the next preprocessor.
  if (g_next_preprocessor) {
    return g_next_preprocessor(exception);
  }

  return exception;
}

}  // namespace

void InstallObjcExceptionPreprocessor() {
  if (g_next_preprocessor) {
    return;
  }

  g_next_preprocessor =
      objc_setExceptionPreprocessor(&ObjcExceptionPreprocessor);
}

void ResetObjcExceptionStateForTesting() {
  g_first_exception_seen = false;
}

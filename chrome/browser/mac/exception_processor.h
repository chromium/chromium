// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_EXCEPTION_PROCESSOR_H_
#define CHROME_BROWSER_MAC_EXCEPTION_PROCESSOR_H_

#include <stddef.h>

#include "base/feature_list.h"

@class NSException;

// Installs the Objective-C exception preprocessor. This records crash keys for
// NSException objects. The preprocessor will also make fatal any exception that
// is not handled.
void InstallObjcExceptionPreprocessor();

// The items below are exposed only for testing.
////////////////////////////////////////////////////////////////////////////////

BASE_DECLARE_FEATURE(kForceCrashOnExceptions);

// Removes the exception preprocessor if it is installed.
void UninstallObjcExceptionPreprocessor();

#endif  // CHROME_BROWSER_MAC_EXCEPTION_PROCESSOR_H_

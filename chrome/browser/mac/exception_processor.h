// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_EXCEPTION_PROCESSOR_H_
#define CHROME_BROWSER_MAC_EXCEPTION_PROCESSOR_H_

// Installs the Objective-C exception preprocessor. This records crash keys for
// NSException objects.
void InstallObjcExceptionPreprocessor();

// The items below are exposed only for testing.
////////////////////////////////////////////////////////////////////////////////

#if defined(UNIT_TEST)

void ResetObjcExceptionStateForTesting();

#endif

#endif  // CHROME_BROWSER_MAC_EXCEPTION_PROCESSOR_H_

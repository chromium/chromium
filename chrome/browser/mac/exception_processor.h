// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_EXCEPTION_PROCESSOR_H_
#define CHROME_BROWSER_MAC_EXCEPTION_PROCESSOR_H_

#include <stddef.h>

@class NSException;

namespace chrome {

// Installs the Objective-C exception preprocessor. This records UMA and crash
// keys for NSException objects. The preprocessor will also make fatal any
// exception that is not handled.
void InstallObjcExceptionPreprocessor();

// The items below are exposed only for testing.
////////////////////////////////////////////////////////////////////////////////

// Removes the exception preprocessor if it is installed.
void UninstallObjcExceptionPreprocessor();

// Bin for unknown exceptions.
extern const size_t kUnknownNSException;

// Returns the histogram bin for |exception| if it is one we track
// specifically, or |kUnknownNSException| if unknown.
size_t BinForException(NSException* exception);

// Use UMA to track exception occurance.
void RecordExceptionWithUma(NSException* exception);

}  // namespace chrome

#endif  // CHROME_BROWSER_MAC_EXCEPTION_PROCESSOR_H_

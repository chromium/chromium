// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_HISTOGRAMS_H_
#define CC_BASE_HISTOGRAMS_H_

#include "cc/base/base_export.h"

namespace cc {

// Supplies a client name to be inserted into histogram names.
// These are known so far: Renderer, Browser
//
// We currently assume that there is only one distinct client per process.
// Not thread-safe. If called multiple times, warns and skips metrics.
CC_BASE_EXPORT void SetClientNameForMetrics(const char* client_name);

// Returns the client name, for use by applicable cc metrics code.
// May return null, in which case no clients, or at least two clients, set the
// client name, and these metrics should be omitted.
//
// This method guarantees that it will never return two distinct non-null
// values over the lifetime of the process.
CC_BASE_EXPORT const char* GetClientNameForMetrics();

}  // namespace cc

#endif  // CC_BASE_HISTOGRAMS_H_

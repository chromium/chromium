// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYNC_FEATURES_H_
#define CHROME_BROWSER_EXTENSIONS_SYNC_FEATURES_H_

#include "base/feature_list.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// Whether synced extensions are re-installed when restricting policies are
// lifted.
BASE_DECLARE_FEATURE(kReinstallSyncedExtensionsOnPolicyChange);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYNC_FEATURES_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/mock_window_controller_list_observer.h"

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
MockWindowControllerListObserver::MockWindowControllerListObserver() = default;
MockWindowControllerListObserver::~MockWindowControllerListObserver() = default;
}  // namespace extensions

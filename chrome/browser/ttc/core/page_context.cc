// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/page_context.h"

namespace ttc {

PageContext::PageContext() = default;
PageContext::PageContext(PageContext&&) = default;
PageContext& PageContext::operator=(PageContext&&) = default;
PageContext::~PageContext() = default;

}  // namespace ttc

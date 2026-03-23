// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_invoke_options.h"

namespace glic {

GlicInvokeOptions::GlicInvokeOptions(
    glic::mojom::InvocationSource invocation_source)
    : invocation_source(invocation_source) {}

GlicInvokeOptions::~GlicInvokeOptions() = default;

GlicInvokeOptions::GlicInvokeOptions(GlicInvokeOptions&&) = default;

GlicInvokeOptions& GlicInvokeOptions::operator=(GlicInvokeOptions&&) = default;

}  // namespace glic

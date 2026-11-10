// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_PREPARATION_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_PREPARATION_H_

#include <optional>

#include "base/types/expected.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"

class Profile;

namespace glic {

// Validates static caller configuration on `options` before resolving surfaces
// or instances. Returns the matching `GlicInvokeError` if invalid, or
// `std::nullopt` if valid.
std::optional<GlicInvokeError> ValidateInvokeOptions(
    const GlicInvokeOptions& options);

// Resolves whether an invocation landing on an instance should stay on or move
// to the floating surface (`true`), open in the target tab's side panel
// (`false`), or fail (`GlicInvokeError::kLiveModeActive`).
base::expected<bool, GlicInvokeError> ResolveFloatyPreservation(
    bool is_showing_live_mode,
    bool is_floating,
    const GlicInvokeOptions& options);

// Returns whether `profile` has not yet completed FRE consent and an inline
// trust-first FRE experiment (`kGlicMessageFirstFre` or `kGlicActionFirstFRE`)
// is enabled.
bool ShouldUseTrustFirstInlineFre(Profile* profile);

// Returns whether `options` uses a special-case web FRE where native FRE must
// be skipped even when `options.fre_override` is `kUnspecified`.
bool HasSpecialCaseWebFre(const GlicInvokeOptions& options);

// Returns whether an invocation should open in Android native FRE mode.
// Eligible profiles get the native FRE when no entry point or experiment asked
// for a specific web FRE.
bool CanUseNativeFre(const GlicInvokeOptions& options, Profile* profile);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_PREPARATION_H_

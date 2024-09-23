// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SAFETY_CHECK_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SAFETY_CHECK_UTILS_H_

#include "base/gtest_prod_util.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/extension_prefs.h"

class Profile;

namespace extensions {

class CWSInfoServiceInterface;
class Extension;

inline constexpr PrefMap kPrefAcknowledgeSafetyCheckWarning = {
    "ack_safety_check_warning", PrefType::kBool, PrefScope::kExtensionSpecific};

// These functions are used as a utility functions for the Extension
// Safety Check.
namespace ExtensionSafetyCheckUtils {
// Returns the Safety Hub warning reason for an extension.
api::developer_private::SafetyCheckWarningReason GetSafetyCheckWarningReason(
    const Extension& extension,
    Profile* profile,
    bool unpublished_only = false);

// A helper function to `GetSafetyCheckWarningReason` to simplify testing.
api::developer_private::SafetyCheckWarningReason
GetSafetyCheckWarningReasonHelper(CWSInfoServiceInterface* cws_info_service,
                                  BitMapBlocklistState blocklist_state,
                                  Profile* profile,
                                  const Extension& extension,
                                  bool unpublished_only = false);

// Returns the display strings related to a Safety Hub Warning reason.
api::developer_private::SafetyCheckStrings GetSafetyCheckWarningStrings(
    api::developer_private::SafetyCheckWarningReason warning_reason,
    api::developer_private::ExtensionState state);

}  // namespace ExtensionSafetyCheckUtils
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SAFETY_CHECK_UTILS_H_

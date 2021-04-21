// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_OMAHA_ATTRIBUTES_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_OMAHA_ATTRIBUTES_HANDLER_H_

namespace base {
class Value;
}

namespace extensions {

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "ExtensionUpdateCheckDataKey" in src/tools/metrics/histograms/enums.xml.
enum class ExtensionUpdateCheckDataKey {
  // No update check data keys were found so no action was taken.
  kNoKey = 0,
  // The update check data keys had a "_malware" key resulting in the extension
  // being disabled.
  kMalware = 1,
  // The update check data keys had a "_potentially_uws" key resulting in the
  // extension being disabled.
  kPotentiallyUWS = 2,
  // The update check data keys had a "_policy_violation" key resulting in the
  // extension being disabled.
  kPolicyViolation = 3,
  kMaxValue = kPolicyViolation
};

// Manages the Omaha attributes blocklist/greylist states in extension pref.
class OmahaAttributesHandler {
 public:
  OmahaAttributesHandler() = default;
  OmahaAttributesHandler(const OmahaAttributesHandler&) = delete;
  OmahaAttributesHandler& operator=(const OmahaAttributesHandler&) = delete;
  ~OmahaAttributesHandler() = default;

  // Logs UMA metrics when an extension is disabled remotely.
  static void ReportExtensionDisabledRemotely(
      bool should_be_remotely_disabled,
      ExtensionUpdateCheckDataKey reason);

  // Logs UMA metrics when the key is not found in Omaha attributes.
  static void ReportNoUpdateCheckKeys();

  // Logs UMA metrics when a remotely disabled extension is re-enabled.
  static void ReportReenableExtensionFromMalware();

  // Performs action based on Omaha attributes for the extension.
  // TODO(crbug.com/1193695): This function currently only handles greylist
  // states. We should move blocklist handling into this class too.
  void PerformActionBasedOnOmahaAttributes(const base::Value& attributes);

 private:
  void ReportPolicyViolationUWSOmahaAttributes(const base::Value& attributes);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_OMAHA_ATTRIBUTES_HANDLER_H_

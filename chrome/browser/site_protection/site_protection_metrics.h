// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_PROTECTION_SITE_PROTECTION_METRICS_H_
#define CHROME_BROWSER_SITE_PROTECTION_SITE_PROTECTION_METRICS_H_

namespace site_protection {

// Denotes the v8-optimizer state of a frame and the topmost frame in the render
// tree. The topmost frame might be an indirect ancestor (ex grandparent).
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(IframeV8OptimizerState)
enum class IframeV8OptimizerState {
  kEnabledForChildAndTopmost,
  kEnabledForChildDisabledForTopmost,
  kDisabledForChildEnabledForTopmost,
  kDisabledForChildAndTopmost,
  kMaxValue = kDisabledForChildAndTopmost,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:V8IframeState)

}  // namespace site_protection

#endif  // CHROME_BROWSER_SITE_PROTECTION_SITE_PROTECTION_METRICS_H_

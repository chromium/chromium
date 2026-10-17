// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TIPS_CORE_TIPS_SIGNAL_KEYS_H_
#define CHROME_BROWSER_TIPS_CORE_TIPS_SIGNAL_KEYS_H_

// Keys for Java-only runtime signals that tips features consume in
// `TipsFeature::IsEligible()`. Java handlers supply these via
// `TipsPromoHandler#getCustomSignals()`, and the values are passed across JNI
// as a single map. Java constants are generated from this file into
// `org.chromium.chrome.browser.tips.TipsSignalKeys` via `java_cpp_strings`.
//
// Keys must be unique across all tips features.

namespace tips {

// Whether the omnibox is currently positioned at the bottom of the screen
// (1.0) or not (0.0).
//
// The value must match `segmentation_platform::kBottomOmniboxStatus`, which
// the legacy (non self-service) ranker uses as an input name for the same
// signal.
// TODO(crbug.com/559296862): Drop the coupling once the legacy ranker is
// removed.
inline constexpr char kBottomOmniboxStatus[] = "bottom_omnibox_status";

}  // namespace tips

#endif  // CHROME_BROWSER_TIPS_CORE_TIPS_SIGNAL_KEYS_H_

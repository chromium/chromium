// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_METRICS_H_
#define CHROME_BROWSER_ASH_GROWTH_METRICS_H_

// Enumeration of campaign button ID. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "CampaignButtonId" in src/tools/metrics/histograms/metadata/enums.xml.
enum class CampaignButtonId {
  kPrimary = 0,
  kSecondary = 1,

  kMaxValue = kSecondary,
};

#endif  // CHROME_BROWSER_ASH_GROWTH_METRICS_H_

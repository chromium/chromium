// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_METRICS_H_
#define CHROME_BROWSER_ASH_GROWTH_METRICS_H_

// Enumeration of campaign button ID. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "CampaignButtonId" in
// src/tools/metrics/histograms/metadata/ash/histograms.xml and
// src/tools/metrics/structured/sync/structured.xml.
enum class CampaignButtonId {
  kPrimary = 0,
  kSecondary = 1,
  // Just in case that the button is neither primary nor secondary. For
  // example, potentially notification can have more than 2 buttons although it
  // is uncommon.
  kOthers = 2,

  kMaxValue = kOthers,
};

// Records how many times a button is pressed in the campaign UI.
void RecordButtonPressed(int campaign_id, CampaignButtonId button_id);

// Records how many times a campaign UI is dismissed.
void RecordDismissed(int campaign_id);

// Records how many times a campaign UI is about to show.
void RecordImpression(int campaign_id);

#endif  // CHROME_BROWSER_ASH_GROWTH_METRICS_H_

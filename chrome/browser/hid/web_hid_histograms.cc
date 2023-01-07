// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/web_hid_histograms.h"

#include "base/metrics/histogram_functions.h"

void RecordWebHidChooserClosure(WebHidChooserClosed disposition) {
  base::UmaHistogramEnumeration("Permissions.WebHid.ChooserClosed",
                                disposition);
}

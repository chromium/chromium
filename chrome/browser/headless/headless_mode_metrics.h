// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_METRICS_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_METRICS_H_

namespace headless {

// Report headless actions like remote debugging, screenshot, print to PDF.
void ReportHeadlessActionMetrics();

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_METRICS_H_

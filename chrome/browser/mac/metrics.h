// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_METRICS_H_
#define CHROME_BROWSER_MAC_METRICS_H_

namespace mac_metrics {

// Records the file system type where the running instance of Chrome is
// installed.
void RecordAppFileSystemType();

}  // namespace mac_metrics

#endif  // CHROME_BROWSER_MAC_METRICS_H_

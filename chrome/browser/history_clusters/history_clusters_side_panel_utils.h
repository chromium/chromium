// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UTILS_H_
#define CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UTILS_H_

class Profile;

namespace history_clusters {

// Returns whether the History Clusters (Journeys) side panel is supported for
// `profile`. When it is not, neither its side panel entry nor its action may
// be registered.
bool IsSidePanelSupported(Profile* profile);

}  // namespace history_clusters

#endif  // CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UTILS_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_TAB_RESTORE_HELPER_H_
#define CHROME_BROWSER_GLIC_GLIC_TAB_RESTORE_HELPER_H_

#include <map>
#include <string>

namespace content {
class WebContents;
}

namespace glic {

// Checks the WebContents for Glic state and populates the extra_data map.
void PopulateGlicExtraData(content::WebContents* web_contents,
                           std::map<std::string, std::string>* extra_data);

// Checks the extra_data map for Glic state and attaches it to the WebContents
// if present, so it can be picked up later by the TabStripModel observer.
void RestoreGlicStateFromExtraData(
    content::WebContents* web_contents,
    const std::map<std::string, std::string>& extra_data);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_TAB_RESTORE_HELPER_H_

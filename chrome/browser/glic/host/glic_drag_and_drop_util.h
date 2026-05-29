// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_DRAG_AND_DROP_UTIL_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_DRAG_AND_DROP_UTIL_H_

#include "content/public/browser/web_contents_view_delegate.h"

namespace content {
class WebContents;
struct DropData;
}  // namespace content

namespace glic {

// Returns true if `drop_data` contains the custom GLIC drag ID.
bool IsGlicWebDrag(const content::DropData& drop_data);

// Validates the drag source, fetches the TabContext, and triggers the GLIC
// Invoke API for a drop.
void StartDragAndDropInvoke(content::WebContents* target_web_contents,
                            const content::DropData& drop_data);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_DRAG_AND_DROP_UTIL_H_

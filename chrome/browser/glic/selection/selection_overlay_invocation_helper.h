// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_INVOCATION_HELPER_H_
#define CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_INVOCATION_HELPER_H_

namespace selection {
enum class EntryPoint;
}  // namespace selection

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// Starts a region capture on `tab`. Hands off to the web client when the Glic
// side panel owns the session, otherwise shows the selection overlay directly,
// pre-selecting the current text selection if there is one.
void InvokeSelectionOverlay(tabs::TabInterface& tab,
                            ::selection::EntryPoint entry_point);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_INVOCATION_HELPER_H_

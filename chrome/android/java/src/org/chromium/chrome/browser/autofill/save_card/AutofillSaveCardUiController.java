// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

/**
 * Interface to control the UI to show the BottomSheetContent. This interface is a subset of the
 * BottomSheetController interface.
 */
@NullMarked
interface AutofillSaveCardUiController {
    boolean requestShowContent(BottomSheetContent content, boolean animate);

    void hideContent(
            @Nullable BottomSheetContent content,
            boolean animate,
            @StateChangeReason int hideReason);

    void addObserver(BottomSheetObserver observer);

    void removeObserver(BottomSheetObserver observer);
}

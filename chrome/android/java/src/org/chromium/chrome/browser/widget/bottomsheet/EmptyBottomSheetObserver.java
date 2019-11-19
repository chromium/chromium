// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.StateChangeReason;

/**
 * An empty base implementation of the {@link BottomSheetObserver} interface.
 */
public class EmptyBottomSheetObserver implements BottomSheetObserver {
    @Override
    public void onSheetOpened(@StateChangeReason int reason) {}

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {}

    @Override
    public void onLoadUrl(String url) {}

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {}

    @Override
    public void onSheetStateChanged(int newState) {}

    @Override
    public void onSheetFullyPeeked() {}

    @Override
    public void onSheetContentChanged(BottomSheetContent newContent) {}
}

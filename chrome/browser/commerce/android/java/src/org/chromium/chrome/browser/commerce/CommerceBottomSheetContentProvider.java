// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

/**
 * The interface to be implemented by the individual feature to show a View in the
 * CommerceBottomSheet.
 */
public interface CommerceBottomSheetContentProvider {

    /**
     * Request the content to show in the CommerceBottomSheetContent.
     *
     * @param listener The listener that will run after the feature is finished gathering all
     *     content.
     */
    void requestShowContent(CommerceBottomSheetContentListener listener);

    /** Called when the BottomSheet hides. */
    void hideContentView();
}

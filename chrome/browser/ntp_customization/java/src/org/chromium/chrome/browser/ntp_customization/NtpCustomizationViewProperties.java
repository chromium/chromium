// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The properties associated with rendering NTP customization bottom sheets. */
public class NtpCustomizationViewProperties {
    /** The click listener to handle back button clicks in the bottom sheet. */
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            BACK_PRESS_HANDLER = new PropertyModel.WritableObjectPropertyKey<>();

    /** The position of the bottom sheet layout in the view flipper. */
    public static final PropertyModel.WritableIntPropertyKey LAYOUT_TO_DISPLAY =
            new PropertyModel.WritableIntPropertyKey();

    /**
     * The delegate provides list content and event handlers to a {@link
     * BottomSheetListContainerView}.
     */
    public static final PropertyModel.WritableObjectPropertyKey<ListContainerViewDelegate>
            LIST_CONTAINER_VIEW_DELEGATE = new PropertyModel.WritableObjectPropertyKey<>();

    /** The keys to bind a view flipper view. */
    public static final PropertyKey[] VIEW_FLIPPER_KEYS = new PropertyKey[] {LAYOUT_TO_DISPLAY};

    /** The keys to bind a {@link BottomSheetListContainerView}. */
    public static final PropertyKey[] LIST_CONTAINER_KEYS =
            new PropertyKey[] {LIST_CONTAINER_VIEW_DELEGATE};

    /** The keys to bind a NTP customization bottom sheet with a back button inside. */
    public static final PropertyKey[] BOTTOM_SHEET_KEYS = new PropertyKey[] {BACK_PRESS_HANDLER};
}

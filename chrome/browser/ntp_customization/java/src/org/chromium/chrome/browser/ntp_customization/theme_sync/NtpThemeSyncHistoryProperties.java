// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the NTP theme sync history. */
@NullMarked
public class NtpThemeSyncHistoryProperties {
    public static final WritableObjectPropertyKey<RecyclerView.Adapter> RECYCLER_VIEW_ADAPTER =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<RecyclerView.LayoutManager>
            RECYCLER_VIEW_LAYOUT_MANAGER = new WritableObjectPropertyKey<>();

    // This index isn't updated when the RECYCLER_VIEW_ADAPTER handles the highlighted items. We
    // have to skip equality check to allow the refreshed value is set.
    public static final WritableObjectPropertyKey<Integer> HIGHLIGHTED_ITEM_INDEX =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);

    public static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<View.OnClickListener>
            MORE_OPTIONS_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                RECYCLER_VIEW_ADAPTER,
                RECYCLER_VIEW_LAYOUT_MANAGER,
                HIGHLIGHTED_ITEM_INDEX,
                IS_VISIBLE,
                MORE_OPTIONS_CLICK_LISTENER
            };
}

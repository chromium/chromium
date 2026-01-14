// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet.TabBottomSheetUtils.TabBottomSheetModes;

/** Base class for the Tab Bottom Sheet toolbar. */
@NullMarked
public class TabBottomSheetToolbarContainer {
    private final Context mContext;
    private final ViewGroup mContainerView;

    private @Nullable TabBottomSheetToolbar mToolbar;

    TabBottomSheetToolbarContainer(Context context, ViewGroup container) {
        mContext = context;
        mContainerView = container;
    }

    void setToolbar(@TabBottomSheetModes int tabBottomSheetMode) {
        if (mToolbar != null) {
            mContainerView.removeView(assumeNonNull(mToolbar.getToolbarView()));
        }
        switch (tabBottomSheetMode) {
            case TabBottomSheetModes.SIMPLE:
                mToolbar = new TabBottomSheetSimpleToolbar(mContext);
                break;
            default:
                assert false : "Unknown tab bottom sheet mode.";
                break;
        }
        mContainerView.addView(assumeNonNull(mToolbar).getToolbarView());
    }

    void destroy() {
        if (mToolbar != null) {
            mContainerView.removeView(assumeNonNull(mToolbar.getToolbarView()));
            mToolbar = null;
        }
    }

    @Nullable TabBottomSheetToolbar getToolbar() {
        return mToolbar;
    }
}

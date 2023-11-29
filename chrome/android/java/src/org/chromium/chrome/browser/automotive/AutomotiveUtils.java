// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotive;

import android.content.Context;
import android.content.res.TypedArray;

import org.chromium.base.BuildInfo;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

public class AutomotiveUtils {

    /** Returns the height of the automotive back button toolbar. */
    public static int getAutomotiveToolbarHeightDp(Context activityContext) {
        if (BuildInfo.getInstance().isAutomotive) {
            final TypedArray styledAttributes =
                    activityContext
                            .getTheme()
                            .obtainStyledAttributes(
                                    new int[] {org.chromium.ui.R.attr.actionBarSize});
            int automotiveToolbarHeightPx = Math.round(styledAttributes.getDimension(0, 0));
            styledAttributes.recycle();
            return DisplayUtil.pxToDp(
                    DisplayAndroid.getNonMultiDisplay(activityContext), automotiveToolbarHeightPx);
        } else {
            return 0;
        }
    }
}

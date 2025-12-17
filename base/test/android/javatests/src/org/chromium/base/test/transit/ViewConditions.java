// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.chromium.base.test.util.ViewPrinter.Options.PRINT_SHALLOW;

import android.view.View;

import org.chromium.base.test.util.ViewPrinter;
import org.chromium.build.annotations.NullMarked;

import java.util.List;

/** {@link Condition}s related to Android {@link View}s. */
@NullMarked
public class ViewConditions {
    public static String writeMatchingViewsStatusMessage(List<ViewAndRoot> viewMatches) {
        // TODO(crbug.com/456770151): Print which root matches are in.
        if (viewMatches.isEmpty()) {
            return "No matching Views";
        } else if (viewMatches.size() == 1) {
            String viewDescription =
                    ViewPrinter.describeView(viewMatches.get(0).view, PRINT_SHALLOW);
            return String.format("1 matching View: %s", viewDescription);
        } else {
            String viewDescription1 =
                    ViewPrinter.describeView(viewMatches.get(0).view, PRINT_SHALLOW);
            String viewDescription2 =
                    ViewPrinter.describeView(viewMatches.get(1).view, PRINT_SHALLOW);
            String moreString = viewMatches.size() > 2 ? " and more" : "";

            return String.format(
                    "%d matching Views: %s, %s%s",
                    viewMatches.size(), viewDescription1, viewDescription2, moreString);
        }
    }
}

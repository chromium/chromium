// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.selection;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.autofill.AutofillSelectionActionMenuDelegate;
import org.chromium.content_public.browser.SelectionPopupController;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The WebView delegate customizing text selection menu items in {@link SelectionPopupController}.
 * It records webview-specific histograms.
 */
public class AwSelectionActionMenuDelegate extends AutofillSelectionActionMenuDelegate {
    static final String TEXT_SELECTION_MENU_ORDERING_HISTOGRAM_NAME =
            "Android.WebView.TextSelectionMenuOrdering";

    // This should be kept in sync with the definition
    // |AndroidWebViewTextSelectionMenuOrdering| in
    // tools/metrics/histograms/metadata/android/enums.xml
    @IntDef({
        TextSelectionMenuOrdering.DEFAULT_MENU_ORDER,
        TextSelectionMenuOrdering.SAMSUNG_MENU_ORDER,
        TextSelectionMenuOrdering.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface TextSelectionMenuOrdering {
        int DEFAULT_MENU_ORDER = 0;
        int SAMSUNG_MENU_ORDER = 1;
        int COUNT = 2;
    }

    public AwSelectionActionMenuDelegate() {
        RecordHistogram.recordEnumeratedHistogram(
                TEXT_SELECTION_MENU_ORDERING_HISTOGRAM_NAME,
                TextSelectionMenuOrdering.DEFAULT_MENU_ORDER,
                TextSelectionMenuOrdering.COUNT);
    }
}

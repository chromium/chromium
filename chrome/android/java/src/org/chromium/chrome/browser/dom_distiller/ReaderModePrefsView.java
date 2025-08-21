// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;

/**
 * A view which displays preferences for reader mode. This allows users to change the theme, font
 * size, etc. when browsing in reader mode. This has the same functionality as
 * DistilledPagePrefsView, but created to support new UI.
 */
@NullMarked
public class ReaderModePrefsView extends LinearLayout {
    // XML layout for View.
    private static final int VIEW_LAYOUT = R.layout.reader_mode_prefs_view;

    /**
     * Creates a ReaderModePrefsView.
     *
     * @param context Context for acquiring resources.
     * @param attrs Attributes from the XML layout inflation.
     */
    public ReaderModePrefsView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Creates a ReaderModePrefsView. This is the method for programmatically creating this view.
     *
     * @param context The {@link Context} to use for inflation.
     * @return A new {@link ReaderModePrefsView}.
     */
    public static ReaderModePrefsView create(Context context) {
        ReaderModePrefsView prefsView =
                (ReaderModePrefsView) LayoutInflater.from(context).inflate(VIEW_LAYOUT, null);
        return prefsView;
    }
}

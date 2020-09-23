// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.os.Build;
import android.util.AttributeSet;
import android.widget.TextView;

/**
 * Provides a TextView that provides safety for ellipsize=start on any version of Android.
 * TODO(donnd): remove this when older versions of Android are no longer supported.
 * See crbug.com/834959.
 */
public class TextViewEllipsizerSafe extends TextView {
    // The maximum length of a string that we return when detecting a crash.
    // This needs to be small enough to not take a long time in the worst case where we crash again
    // and again while shortening the string.
    private static final int STRING_MAXIMUM_LENGTH = 2000;
    // The minimum length of a string that we return when detecting multiple crashes.
    // This needs to be big enough so that we'll always ellipsize on Chrome for Android, e.g. on a
    // big tablet in landscape mode.
    private static final int STRING_MINIMUM_LENGTH = 1000;

    /**
     * Constructs a subclass of TextView for use in the URL Infobar UI.
     */
    public TextViewEllipsizerSafe(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Safely does #setText when ellipsize=start in at TextView on any version of Android.
     * The #setText method is final, so we have to make our own safe variant.
     * @param stringToEllipsize The text to set in the TextView, which can use the dangerous form
     *        of ellipsize=start.  See https://crbug.com/834959 for details.
     */
    void setTextSafely(String stringToEllipsize) {
        assert stringToEllipsize != null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            setText(stringToEllipsize);
            return;
        }

        // First trim the string to some reasonable length limit.
        int beginIndex = Math.max(0, stringToEllipsize.length() - STRING_MAXIMUM_LENGTH);
        String result = stringToEllipsize.substring(beginIndex);
        int measureSpec = android.view.View.MeasureSpec.EXACTLY;
        while (result.length() > STRING_MINIMUM_LENGTH) {
            try {
                // Attempt to set and measure the altered text.  In rare cases this may crash.
                setText(result);
                measure(measureSpec, measureSpec);
                return;

            } catch (Exception e) {
                // Try to prevent a Chrome crash by retrying with a shorter version.
                // NOTE: we must trim at the start of the string, as specified by the UI/Security
                // spec.
                result = result.substring(1);
            }
        }
    }
}

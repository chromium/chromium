// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.support.v7.app.AppCompatActivity;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;

import org.chromium.chrome.browser.AppHooks;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A means of showing highlight for a particular feature as a form of in product help. */
public class FeatureHighlightProvider {
    /**
     * These values determine text alignment and need to match the values in the closed-source
     * library.
     */
    @IntDef({TextAlignment.START, TextAlignment.CENTER, TextAlignment.END})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TextAlignment {
        int START = 0;
        int CENTER = 1;
        int END = 2;
    }

    /** Static handle to the sole highlight provider. */
    private static FeatureHighlightProvider sInstance;

    /**
     * @return A handle to the highlight provider.
     */
    public static FeatureHighlightProvider getInstance() {
        if (sInstance == null) sInstance = AppHooks.get().createFeatureHighlightProvider();
        return sInstance;
    }

    /**
     * Build and show a feature highlight bubble for a particular view.
     * @param activity An activity to attach the highlight to.
     * @param view The view to focus.
     * @param headTextId The text shown in the header section of the bubble.
     * @param headAlignment Alignment of the head text.
     * @param bodyTextId The text shown in the body section of the bubble.
     * @param bodyAlignment Alignment of the body text.
     * @param color The color of the bubble.
     * @param timeoutMs The amount of time in ms before the bubble disappears.
     */
    public void buildForView(AppCompatActivity activity, View view, @StringRes int headTextId,
            @TextAlignment int headAlignment, @StyleRes int headStyle, @StringRes int bodyTextId,
            @TextAlignment int bodyAlignment, @StyleRes int bodyStyle, @ColorInt int color,
            long timeoutMs) {}

    /**
     * Build and show a feature highlight bubble for a particular view.
     * @param activity An activity to attach the highlight to.
     * @param view The view to focus.
     * @param headTextId The text shown in the header section of the bubble.
     * @param headAlignment Alignment of the head text.
     * @param bodyTextId The text shown in the body section of the bubble.
     * @param bodyAlignment Alignment of the body text.
     * @param color The color of the bubble.
     * @param timeoutMs The amount of time in ms before the bubble disappears.
     * @param completeRunnable The Runnable to be called if the user tab on the view.
     */
    public void buildForView(AppCompatActivity activity, View view, @StringRes int headTextId,
            @TextAlignment int headAlignment, @StyleRes int headStyle, @StringRes int bodyTextId,
            @TextAlignment int bodyAlignment, @StyleRes int bodyStyle, @ColorInt int color,
            long timeoutMs, Runnable completeRunnable) {}
}

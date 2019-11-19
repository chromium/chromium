// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.init.SingleWindowKeyboardVisibilityDelegate;

import java.lang.ref.WeakReference;

/**
 * A {@link SingleWindowKeyboardVisibilityDelegate} that considers UI elements of a
 * {@link ChromeActivity} which amend or replace the keyboard.
 */
public class ChromeKeyboardVisibilityDelegate extends SingleWindowKeyboardVisibilityDelegate {
    /**
     * Creates a new visibility delegate.
     * @param activity A {@link WeakReference} to a {@link ChromeActivity}.
     */
    public ChromeKeyboardVisibilityDelegate(WeakReference<Activity> activity) {
        super(activity);
        assert activity.get() instanceof ChromeActivity;
    }

    @Override
    public @Nullable ChromeActivity getActivity() {
        return (ChromeActivity) super.getActivity();
    }

    /**
     * Hide only Android's soft keyboard. Keeps eventual keyboard replacements and extensions
     * untouched. Usually, you will want to call {@link #hideKeyboard(View)}.
     * @param view A focused {@link View}.
     * @return True if the keyboard was visible before this call.
     */
    public boolean hideSoftKeyboardOnly(View view) {
        return hideAndroidSoftKeyboard(view);
    }

    /**
     * Returns whether Android soft keyboard is showing and ignores all extensions/replacements.
     * Usually, you will want to call {@link #isKeyboardShowing(Context, View)}.
     * @param context A {@link Context} instance.
     * @param view    A {@link View}.
     * @return Returns true if Android's soft keyboard is visible. Ignores extensions/replacements.
     */
    public boolean isSoftKeyboardShowing(Context context, View view) {
        return isAndroidSoftKeyboardShowing(context, view);
    }

    @Override
    public boolean hideKeyboard(View view) {
        ChromeActivity activity = getActivity();
        boolean wasManualFillingViewShowing = false;
        if (activity != null) {
            wasManualFillingViewShowing =
                    activity.getManualFillingComponent().isFillingViewShown(view);
            activity.getManualFillingComponent().hide();
        }
        return super.hideKeyboard(view) || wasManualFillingViewShowing;
    }

    @Override
    public boolean isKeyboardShowing(Context context, View view) {
        ChromeActivity activity = getActivity();
        return super.isKeyboardShowing(context, view)
                || (activity != null
                        && activity.getManualFillingComponent().isFillingViewShown(view));
    }
}
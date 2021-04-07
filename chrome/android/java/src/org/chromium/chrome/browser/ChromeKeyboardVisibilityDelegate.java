// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.Px;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.init.SingleWindowKeyboardVisibilityDelegate;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;

import java.lang.ref.WeakReference;

/**
 * A {@link SingleWindowKeyboardVisibilityDelegate} that considers UI elements of a
 * {@link ChromeActivity} which amend or replace the keyboard.
 */
public class ChromeKeyboardVisibilityDelegate extends SingleWindowKeyboardVisibilityDelegate
        implements ManualFillingComponent.SoftKeyboardDelegate {
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

    /**
     * Implementation ignoring the Chrome-specific keyboard logic on top of the system keyboard.
     * @see ManualFillingComponent.SoftKeyboardDelegate#hideSoftKeyboardOnly(View)
     */
    @Override
    public boolean hideSoftKeyboardOnly(View view) {
        return hideAndroidSoftKeyboard(view);
    }

    /**
     * Implementation ignoring the Chrome-specific keyboard logic on top of the system keyboard.
     * @see ManualFillingComponent.SoftKeyboardDelegate#isSoftKeyboardShowing(Context, View)
     */
    @Override
    public boolean isSoftKeyboardShowing(Context context, View view) {
        return isAndroidSoftKeyboardShowing(context, view);
    }

    /**
     * Implementation ignoring the Chrome-specific keyboard logic on top of the system keyboard.
     * @see ManualFillingComponent.SoftKeyboardDelegate#showSoftKeyboard(ViewGroup)
     */
    @Override
    public void showSoftKeyboard(ViewGroup contentView) {
        showKeyboard(contentView);
    }

    /**
     * Implementation ignoring the Chrome-specific keyboard logic on top of the system keyboard.
     * @see ManualFillingComponent.SoftKeyboardDelegate#calculateSoftKeyboardHeight(View)
     */
    @Override
    public @Px int calculateSoftKeyboardHeight(View rootView) {
        return calculateKeyboardHeight(rootView);
    }
}
// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Px;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.ui.KeyboardUtils;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;

import java.lang.ref.WeakReference;

/**
 * A {@link ActivityKeyboardVisibilityDelegate} that considers UI elements of an {@link Activity}
 * which amend or replace the keyboard.
 */
public class ChromeKeyboardVisibilityDelegate extends ActivityKeyboardVisibilityDelegate
        implements ManualFillingComponent.SoftKeyboardDelegate {
    private final Supplier<ManualFillingComponent> mManualFillingComponentSupplier;

    /**
     * Creates a new visibility delegate.
     * @param activity A {@link WeakReference} to an {@link Activity}.
     */
    public ChromeKeyboardVisibilityDelegate(
            WeakReference<Activity> activity,
            @NonNull Supplier<ManualFillingComponent> manualFillingComponentSupplier) {
        super(activity);
        mManualFillingComponentSupplier = manualFillingComponentSupplier;
    }

    @Override
    public boolean hideKeyboard(View view) {
        boolean wasManualFillingViewShowing = false;
        if (mManualFillingComponentSupplier.hasValue()) {
            wasManualFillingViewShowing =
                    mManualFillingComponentSupplier.get().isFillingViewShown(view);
            mManualFillingComponentSupplier.get().hide();
        }
        return hideSoftKeyboardOnly(view) || wasManualFillingViewShowing;
    }

    @Override
    public boolean isKeyboardShowing(Context context, View view) {
        return isSoftKeyboardShowing(context, view)
                || (mManualFillingComponentSupplier.hasValue()
                        && mManualFillingComponentSupplier.get().isFillingViewShown(view));
    }

    @Override
    public int calculateTotalKeyboardHeight(View rootView) {
        int accessoryHeight = 0;
        if (mManualFillingComponentSupplier.hasValue()) {
            accessoryHeight = mManualFillingComponentSupplier.get().getKeyboardExtensionHeight();
        }
        return calculateSoftKeyboardHeight(rootView) + accessoryHeight;
    }

    // Implements ManualFillingComponent.SoftKeyboardDelegate

    /**
     * Implementation ignoring the Chrome-specific keyboard logic on top of the system keyboard.
     *
     * @see ManualFillingComponent.SoftKeyboardDelegate#hideSoftKeyboardOnly(View)
     */
    @Override
    public boolean hideSoftKeyboardOnly(View view) {
        return KeyboardUtils.hideAndroidSoftKeyboard(view);
    }

    /**
     * Implementation ignoring the Chrome-specific keyboard logic on top of the system keyboard.
     *
     * @see ManualFillingComponent.SoftKeyboardDelegate#isSoftKeyboardShowing(Context, View)
     */
    @Override
    public boolean isSoftKeyboardShowing(Context context, View view) {
        return KeyboardUtils.isAndroidSoftKeyboardShowing(view);
    }

    /**
     * Implementation ignoring the Chrome-specific keyboard logic on top of the system keyboard.
     *
     * @see ManualFillingComponent.SoftKeyboardDelegate#showSoftKeyboard(ViewGroup)
     */
    @Override
    public void showSoftKeyboard(ViewGroup contentView) {
        showKeyboard(contentView);
    }

    /**
     * Implementation ignoring the Chrome-specific keyboard logic on top of the system keyboard.
     *
     * @see ManualFillingComponent.SoftKeyboardDelegate#calculateSoftKeyboardHeight(View)
     */
    @Override
    public @Px int calculateSoftKeyboardHeight(View rootView) {
        return KeyboardUtils.calculateKeyboardHeightFromWindowInsets(rootView);
    }
}

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Px;

import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;

import java.lang.ref.WeakReference;
import java.util.function.Supplier;

/**
 * A {@link ActivityKeyboardVisibilityDelegate} that considers UI elements of an {@link Activity}
 * which amend or replace the keyboard.
 */
@NullMarked
public class ChromeKeyboardVisibilityDelegate extends ActivityKeyboardVisibilityDelegate
        implements ManualFillingComponent.SoftKeyboardDelegate {
    private final Supplier<ManualFillingComponent> mManualFillingComponentSupplier;

    /**
     * Creates a new visibility delegate.
     * @param activity A {@link WeakReference} to an {@link Activity}.
     */
    public ChromeKeyboardVisibilityDelegate(
            WeakReference<Activity> activity,
            Supplier<ManualFillingComponent> manualFillingComponentSupplier) {
        super(activity);
        mManualFillingComponentSupplier = manualFillingComponentSupplier;
    }

    @Override
    public boolean hideKeyboard(View view) {
        boolean wasManualFillingViewShowing = false;
        var manualFillingComponent = mManualFillingComponentSupplier.get();
        if (manualFillingComponent != null) {
            wasManualFillingViewShowing = manualFillingComponent.isFillingViewShown(view);
            manualFillingComponent.hide();
        }
        return hideSoftKeyboardOnly(view) || wasManualFillingViewShowing;
    }

    @Override
    public boolean isKeyboardShowing(View view) {
        ManualFillingComponent manualFillingComponent = mManualFillingComponentSupplier.get();
        return isSoftKeyboardShowing(view)
                || (manualFillingComponent != null
                        && manualFillingComponent.isFillingViewShown(view));
    }

    @Override
    public int calculateTotalKeyboardHeight(View rootView) {
        int accessoryHeight = 0;
        var manualFillingComponent = mManualFillingComponentSupplier.get();
        if (manualFillingComponent != null) {
            accessoryHeight = manualFillingComponent.getKeyboardExtensionHeight();
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
     * @see ManualFillingComponent.SoftKeyboardDelegate#isSoftKeyboardShowing(View)
     */
    @Override
    public boolean isSoftKeyboardShowing(View view) {
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

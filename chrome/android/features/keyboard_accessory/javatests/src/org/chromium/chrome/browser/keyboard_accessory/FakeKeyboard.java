// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeKeyboardVisibilityDelegate;
import org.chromium.chrome.browser.ChromeWindow;

import java.lang.ref.WeakReference;

/**
 * This class allows to mock the {@link org.chromium.ui.KeyboardVisibilityDelegate} in any given
 * {@link org.chromium.chrome.test.ChromeActivityTestRule} which allows to write tests relying on
 * keyboard without having to deal with the soft keyboard. To use it, inject its constructor as
 * factory into the {@link org.chromium.chrome.browser.ChromeWindow} before launching an activity.
 * To reset, call {@link ChromeWindow#resetKeyboardVisibilityDelegateFactory()}.
 * <pre>E.g.{@code
 *    // To force a keyboard open.
 *    ChromeWindow.setKeyboardVisibilityDelegateFactory(FakeKeyboard::new);
 *    aTestRule.startMainActivityOnBlankPage();
 *    ChromeWindow.resetKeyboardVisibilityDelegateFactory();
 *    aTestRule.getKeyboardDelegate().showKeyboard();  // No delay/waiting necessary.
 *  }</pre>
 */
public class FakeKeyboard extends ChromeKeyboardVisibilityDelegate {
    private static final int KEYBOARD_HEIGHT_DP = 234;
    private boolean mIsShowing;

    public FakeKeyboard(
            WeakReference<Activity> activity,
            @NonNull Supplier<ManualFillingComponent> manualFillingComponentSupplier) {
        super(activity, manualFillingComponentSupplier);
    }

    protected int getStaticKeyboardHeight() {
        return (int)
                (getActivity().getResources().getDisplayMetrics().density * KEYBOARD_HEIGHT_DP);
    }

    @Override
    public boolean isSoftKeyboardShowing(Context context, View view) {
        return mIsShowing;
    }

    @Override
    public void showKeyboard(View view) {
        boolean keyboardWasVisible = mIsShowing;
        mIsShowing = true;
        runOnUiThreadBlocking(
                () -> {
                    // Fake a layout change for components listening to the activity directly ...
                    if (getStaticKeyboardHeight() <= 0) {
                        return; // ... unless the keyboard didn't affect it.
                    }
                    if (!keyboardWasVisible) {
                        notifyListeners(isKeyboardShowing(getActivity(), view));
                    }
                    // Pretend a layout change for components listening to the activity directly:
                    View contentView = getActivity().findViewById(android.R.id.content);
                    ViewGroup.LayoutParams p = contentView.getLayoutParams();
                    p.height = p.height - getStaticKeyboardHeight();
                    contentView.setLayoutParams(p);
                });
    }

    @Override
    public boolean hideSoftKeyboardOnly(View view) {
        boolean keyboardWasVisible = mIsShowing;
        mIsShowing = false;
        runOnUiThreadBlocking(
                () -> {
                    // Fake a layout change for components listening to the activity directly ...
                    if (getStaticKeyboardHeight() <= 0) {
                        return; // ... unless the keyboard didn't affect it.
                    }
                    if (keyboardWasVisible) notifyListeners(isKeyboardShowing(getActivity(), view));
                    View contentView = getActivity().findViewById(android.R.id.content);
                    ViewGroup.LayoutParams p = contentView.getLayoutParams();
                    p.height = p.height + getStaticKeyboardHeight();
                    contentView.setLayoutParams(p);
                });
        return keyboardWasVisible;
    }

    @Override
    public int calculateTotalKeyboardHeight(View rootView) {
        return mIsShowing ? getStaticKeyboardHeight() : 0;
    }
}

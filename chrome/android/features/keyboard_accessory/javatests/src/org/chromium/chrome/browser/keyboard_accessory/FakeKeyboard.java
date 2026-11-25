// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeKeyboardVisibilityDelegate;
import org.chromium.chrome.browser.ChromeWindow;

import java.lang.ref.WeakReference;
import java.util.function.Supplier;

/**
 * This class allows to mock the {@link org.chromium.ui.KeyboardVisibilityDelegate} in any given
 * {@link org.chromium.chrome.test.ChromeActivityTestRule} which allows to write tests relying on
 * keyboard without having to deal with the soft keyboard. To use it, inject its constructor as
 * factory into the {@link org.chromium.chrome.browser.ChromeWindow} before launching an activity.
 * To reset, call {@link ChromeWindow#resetKeyboardVisibilityDelegateFactory()}.
 *
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
    private @Nullable Integer mSavedContentLayoutHeight;

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
    public boolean isSoftKeyboardShowing(View view) {
        return mIsShowing;
    }

    @Override
    public boolean isKeyboardVisible(View view) {
        return isSoftKeyboardShowing(view);
    }

    @Override
    public void showKeyboard(View view) {
        runOnUiThreadBlocking(
                () -> {
                    if (mIsShowing) return;
                    Activity activity = getActivity();
                    if (activity == null) return;
                    mIsShowing = true;
                    // Fake a layout change for components listening to the activity directly ...
                    int keyboardHeight = getStaticKeyboardHeight();
                    if (keyboardHeight <= 0) {
                        return; // ... unless the keyboard didn't affect it.
                    }
                    View contentView = activity.findViewById(android.R.id.content);
                    if (contentView != null) {
                        ViewGroup.LayoutParams p = contentView.getLayoutParams();
                        int currentHeight = p.height > 0 ? p.height : contentView.getHeight();
                        if (currentHeight > keyboardHeight) {
                            mSavedContentLayoutHeight = p.height;
                            p.height = currentHeight - keyboardHeight;
                            contentView.setLayoutParams(p);
                        }
                    }
                    notifyListeners(isKeyboardObscuring(view));
                    if (mIsShowing) {
                        notifyVisibilityListeners(isKeyboardVisible(view));
                    }
                });
    }

    @Override
    public boolean hideSoftKeyboardOnly(View view) {
        return runOnUiThreadBlocking(
                () -> {
                    if (!mIsShowing) return false;
                    mIsShowing = false;
                    Integer savedHeight = mSavedContentLayoutHeight;
                    mSavedContentLayoutHeight = null;
                    Activity activity = getActivity();
                    if (savedHeight != null && activity != null) {
                        View contentView = activity.findViewById(android.R.id.content);
                        if (contentView != null) {
                            ViewGroup.LayoutParams p = contentView.getLayoutParams();
                            p.height = savedHeight;
                            contentView.setLayoutParams(p);
                        }
                    }
                    // Fake a layout change for components listening to the activity directly ...
                    if (savedHeight == null
                            && (activity == null || getStaticKeyboardHeight() <= 0)) {
                        return true; // ... unless the keyboard didn't affect it.
                    }
                    notifyListeners(isKeyboardObscuring(view));
                    if (!mIsShowing) {
                        notifyVisibilityListeners(isKeyboardVisible(view));
                    }
                    return true;
                });
    }

    @Override
    public int calculateTotalKeyboardHeight(View rootView) {
        return mIsShowing ? getStaticKeyboardHeight() : 0;
    }
}

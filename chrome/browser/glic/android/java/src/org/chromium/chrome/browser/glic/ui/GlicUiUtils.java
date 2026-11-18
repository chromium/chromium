// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.content.res.AppCompatResources;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.glic.R;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;

/** Utility class for Glic UI components. */
@JNINamespace("glic")
@NullMarked
public final class GlicUiUtils {
    private GlicUiUtils() {}

    /**
     * Parks view focus on {@code containerView} itself (without auto-focusing a descendant such as
     * the loading placeholder's close button).
     */
    @CalledByNative
    public static boolean requestContainerFocus(@Nullable View containerView) {
        if (containerView == null || !containerView.isAttachedToWindow()) {
            return false;
        }
        if (containerView.isFocused()) {
            return true;
        }
        if (containerView instanceof ViewGroup viewGroup) {
            int previousFocusability = viewGroup.getDescendantFocusability();
            try {
                viewGroup.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
                return containerView.requestFocus();
            } finally {
                viewGroup.setDescendantFocusability(previousFocusability);
            }
        }
        return containerView.requestFocus();
    }

    /** Returns whether {@code containerView} or any descendant within it currently holds focus. */
    @CalledByNative
    public static boolean hasContainerFocus(@Nullable View containerView) {
        return containerView != null
                && containerView.isAttachedToWindow()
                && containerView.hasFocus();
    }

    /**
     * Clears focus when parked on {@code containerView} itself without corrupting focused
     * descendants or triggering root-view refocus side effects.
     */
    @CalledByNative
    public static void clearContainerFocus(@Nullable View containerView) {
        if (containerView == null || !containerView.isFocused()) {
            return;
        }
        // View.clearFocus() invokes rootViewRequestFocus() on Android P+, which otherwise
        // immediately re-focuses containerView or jumps focus onto another focusable browser view.
        if (containerView.getRootView() instanceof ViewGroup rootGroup
                && rootGroup != containerView) {
            int previousRootFocusability = rootGroup.getDescendantFocusability();
            try {
                rootGroup.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
                containerView.clearFocus();
            } finally {
                rootGroup.setDescendantFocusability(previousRootFocusability);
            }
            return;
        }
        containerView.clearFocus();
    }

    /**
     * Configures the Glic inactive placeholder view.
     *
     * @param placeholder The placeholder view.
     */
    public static void setupPlaceholderView(TextViewWithCompoundDrawables placeholder) {
        placeholder.setText(R.string.glic_inactive_view_card_text);
        placeholder.setCompoundDrawablesRelativeWithIntrinsicBounds(
                0, R.drawable.ic_spark_filled_24dp, 0, 0);
        placeholder.setDrawableTintColor(
                AppCompatResources.getColorStateList(
                        placeholder.getContext(), R.color.default_icon_color_tint_list));
    }

    /**
     * Opens the Android "App info" settings screen for Chrome, where app permissions such as the
     * microphone can be managed.
     *
     * @param context The Android {@link Context} used to start the activity.
     */
    public static void openAppDetailsSettings(Context context) {
        Intent intent =
                new Intent(
                        Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                        Uri.parse("package:" + context.getPackageName()));
        // Starting an activity from a non-activity context requires it to be in its own task.
        if (!(context instanceof Activity)) {
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        }
        IntentUtils.safeStartActivity(context, intent);
    }
}

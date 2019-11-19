// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.app.Activity;
import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.StrictMode;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.support.v7.widget.ActionMenuView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver.OnScrollChangedListener;
import android.widget.ImageView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.XmlRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

/**
 * A helper class for Preferences.
 */
public class PreferenceUtils {
    /**
     * A helper that is used to load preferences from XML resources without causing a
     * StrictModeViolation. See http://crbug.com/692125.
     *
     * @param preferenceFragment A Support Library {@link PreferenceFragmentCompat}.
     * @param preferencesResId   The id of the XML resource to add to the PreferenceFragment.
     */
    public static void addPreferencesFromResource(
            PreferenceFragmentCompat preferenceFragment, @XmlRes int preferencesResId) {
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            preferenceFragment.addPreferencesFromResource(preferencesResId);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Returns a view tree observer to show the shadow if and only if the view is scrolled.
     * @param view   The view whose scroll will be detected to determine the shadow's visibility.
     * @param shadow The shadow to show/hide.
     * @return An OnScrollChangedListener that detects scrolling and shows the passed in shadow
     *         when a scroll is detected and hides the shadow otherwise.
     */
    public static OnScrollChangedListener getShowShadowOnScrollListener(View view, View shadow) {
        return new OnScrollChangedListener() {
            @Override
            public void onScrollChanged() {
                if (!view.canScrollVertically(-1)) {
                    shadow.setVisibility(View.GONE);
                } else {
                    shadow.setVisibility(View.VISIBLE);
                }
            }
        };
    }

    /**
     * Creates a {@link Drawable} for the given resource id with the default icon color applied.
     */
    public static Drawable getTintedIcon(Context context, @DrawableRes int resId) {
        Drawable icon = AppCompatResources.getDrawable(context, resId);
        // DrawableCompat.setTint() doesn't work well on BitmapDrawables on older versions.
        icon.setColorFilter(
                ApiCompatibilityUtils.getColor(context.getResources(), R.color.default_icon_color),
                PorterDuff.Mode.SRC_IN);
        return icon;
    }

    /**
     * A helper that is used to set the visibility of the overflow menu view in a given activity.
     *
     * @param activity The Activity containing the action bar with the menu.
     * @param visibility The new visibility of the overflow menu view.
     * @return True if the visibility could be set, false otherwise (e.g. because no menu exists).
     */
    public static boolean setOverflowMenuVisibility(@Nullable Activity activity, int visibility) {
        if (activity == null) return false;
        ViewGroup actionBar = activity.findViewById(org.chromium.chrome.R.id.action_bar);
        int i = actionBar.getChildCount();
        ActionMenuView menuView = null;
        while (i-- > 0) {
            if (actionBar.getChildAt(i) instanceof ActionMenuView) {
                menuView = (ActionMenuView) actionBar.getChildAt(i);
                break;
            }
        }
        if (menuView == null) return false;
        View overflowButton = menuView.getChildAt(menuView.getChildCount() - 1);
        if (!isOverflowMenuButton(overflowButton, menuView)) return false;
        overflowButton.setVisibility(visibility);
        return true;
    }

    /**
     * There is no regular way to access the overflow button of an {@link ActionMenuView}.
     * Checking whether a given view is an {@link ImageView} with the correct icon is an
     * approximation to this issue as the exact icon that the parent menu will set is always known.
     *
     * @param button A view in the |parentMenu| that might be the overflow menu.
     * @param parentMenu The menu that created the overflow button.
     * @return True, if the given button can belong to the overflow menu. False otherwise.
     */
    private static boolean isOverflowMenuButton(View button, ActionMenuView parentMenu) {
        if (button == null) return false;
        if (!(button instanceof ImageView))
            return false; // Normal items are usually TextView or LinearLayouts.
        ImageView imageButton = (ImageView) button;
        return imageButton.getDrawable() == parentMenu.getOverflowIcon();
    }
}

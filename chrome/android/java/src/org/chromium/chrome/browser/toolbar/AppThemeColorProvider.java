// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** A ThemeColorProvider for the app theme (incognito or standard theming). */
public class AppThemeColorProvider extends ThemeColorProvider
        implements IncognitoStateObserver, TopResumedActivityChangedObserver {
    /** Primary color for standard mode. */
    private final int mStandardPrimaryColor;

    /** Primary color for incognito mode. */
    private final int mIncognitoPrimaryColor;

    /** Used to know when incognito mode is entered or exited. */
    private IncognitoStateProvider mIncognitoStateProvider;

    /** Used to know the Layout state. */
    private LayoutStateProvider mLayoutStateProvider;

    /** Observer to know when Layout state is changed, e.g show/hide. */
    private final LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;

    /** Whether app is in incognito mode. */
    private boolean mIsIncognito;

    /** The activity {@link Context}. */
    private final Context mActivityContext;

    /**
     * The {@link ActivityLifecycleDispatcher} instance associated with the current activity, if
     * available.
     */
    @Nullable private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    /**
     * Whether the current activity is the top resumed activity. This is only relevant for use in
     * the desktop windowing mode, to determine the tint for the toolbar icons.
     */
    private boolean mIsTopResumedActivity;

    /** Provider for desktop windowing mode state. */
    @Nullable private final DesktopWindowStateProvider mDesktopWindowStateProvider;

    /**
     * @param context The {@link Context} that is used to retrieve color related resources.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} instance
     *     associated with the current activity. {@code null} if activity lifecycle observation is
     *     not required.
     * @param desktopWindowStateProvider The {@link DesktopWindowStateProvider} for the current
     *     activity. {@code null} if desktop window state observation is not required.
     */
    AppThemeColorProvider(
            Context context,
            @Nullable ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider) {
        super(context);

        mActivityContext = context;
        mStandardPrimaryColor = ChromeColors.getDefaultThemeColor(context, false);
        mIncognitoPrimaryColor = ChromeColors.getDefaultThemeColor(context, true);

        mLayoutStateObserver =
                new LayoutStateProvider.LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            updateTheme();
                        }
                    }

                    @Override
                    public void onStartedHiding(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            updateTheme();
                        }
                    }
                };

        mDesktopWindowStateProvider = desktopWindowStateProvider;
        mIsTopResumedActivity =
                mDesktopWindowStateProvider == null
                        || !mDesktopWindowStateProvider.isInUnfocusedDesktopWindow();

        // Activity lifecycle observation for activity focus change.
        if (activityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher = activityLifecycleDispatcher;
            mActivityLifecycleDispatcher.register(this);
        }
    }

    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mIncognitoStateProvider = provider;
        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(this);
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        mIsIncognito = isIncognito;
        updateTheme();
    }

    void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
    }

    private void updateTheme() {
        updatePrimaryColor(mIsIncognito ? mIncognitoPrimaryColor : mStandardPrimaryColor, false);
        final @BrandedColorScheme int brandedColorScheme =
                mIsIncognito ? BrandedColorScheme.INCOGNITO : BrandedColorScheme.APP_DEFAULT;
        final ColorStateList iconTint =
                ThemeUtils.getThemedToolbarIconTint(mActivityContext, brandedColorScheme);

        final ColorStateList activityFocusTint =
                calculateActivityFocusTint(mActivityContext, brandedColorScheme);
        updateTint(iconTint, activityFocusTint, brandedColorScheme);
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mIncognitoStateProvider != null) {
            mIncognitoStateProvider.removeObserver(this);
            mIncognitoStateProvider = null;
        }
        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
            mLayoutStateProvider = null;
        }
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
        }
    }

    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        // TODO (crbug/328055199): Check if losing focus to a non-Chrome task.
        mIsTopResumedActivity = isTopResumedActivity;
        updateTheme();
    }

    private ColorStateList calculateActivityFocusTint(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        var iconTint = ThemeUtils.getThemedToolbarIconTint(context, brandedColorScheme);
        return mActivityLifecycleDispatcher == null
                        || !AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateProvider)
                ? iconTint
                : ThemeUtils.getThemedToolbarIconTintForActivityState(
                        context, brandedColorScheme, mIsTopResumedActivity);
    }
}

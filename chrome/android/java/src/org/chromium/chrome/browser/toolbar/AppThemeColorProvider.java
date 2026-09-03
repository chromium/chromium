// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher.MultiWindowModeObserver;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** A ThemeColorProvider for the app theme (incognito or standard theming). */
@NullMarked
public class AppThemeColorProvider extends ThemeColorProvider
        implements IncognitoStateObserver,
                TopResumedActivityChangedObserver,
                MultiWindowModeObserver {
    /** Primary color for standard mode. */
    private final int mStandardPrimaryColor;

    /** Primary color for incognito mode. */
    private final int mIncognitoPrimaryColor;

    /** Used to know when incognito mode is entered or exited. */
    private @Nullable IncognitoStateProvider mIncognitoStateProvider;

    /** Used to know the Layout state. */
    private @Nullable LayoutStateProvider mLayoutStateProvider;

    /** Observer to know when Layout state is changed, e.g show/hide. */
    private final LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;

    /** Whether app is in incognito mode. */
    private boolean mIsIncognito;

    /** The activity {@link Context}. */
    private final Context mActivityContext;

    /** The {@link ActivityLifecycleDispatcher} instance associated with the current activity. */
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    /**
     * Whether the current activity is the top resumed activity. This is relevant in multi-window
     * mode to determine the tint for the toolbar icons.
     */
    private boolean mIsTopResumedActivity;

    private @Nullable MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;

    /**
     * @param context The {@link Context} that is used to retrieve color related resources.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} instance
     *     associated with the current activity.
     * @param multiWindowModeStateDispatcher The {@link MultiWindowModeStateDispatcher} instance for
     *     the current activity. {@code null} if multi-window observation is not required.
     */
    public AppThemeColorProvider(
            Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @Nullable MultiWindowModeStateDispatcher multiWindowModeStateDispatcher) {
        super(context);

        mActivityContext = context;
        mStandardPrimaryColor =
                ChromeColors.getDefaultThemeColor(context, /* isIncognito= */ false);
        mIncognitoPrimaryColor =
                ChromeColors.getDefaultThemeColor(context, /* isIncognito= */ true);

        mLayoutStateObserver =
                new LayoutStateProvider.LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.HUB) {
                            updateTheme();
                        }
                    }

                    @Override
                    public void onStartedHiding(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.HUB) {
                            updateTheme();
                        }
                    }
                };

        mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;
        if (mMultiWindowModeStateDispatcher != null) {
            mMultiWindowModeStateDispatcher.addObserver(this);
        }

        // Fetch the active top-resumed status dynamically on start-up.
        mIsTopResumedActivity =
                AppHeaderUtils.isActivityFocusedAtStartup(activityLifecycleDispatcher);

        // Activity lifecycle observation for activity focus change.
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
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

    @Override
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode) {
        updateTheme();
    }

    private void updateTheme() {
        updatePrimaryColor(mIsIncognito ? mIncognitoPrimaryColor : mStandardPrimaryColor, false);
        final @BrandedColorScheme int brandedColorScheme =
                mIsIncognito ? BrandedColorScheme.INCOGNITO : BrandedColorScheme.APP_DEFAULT;
        final ColorStateList iconTint =
                ThemeUtils.getThemedToolbarIconTint(mActivityContext, brandedColorScheme);

        final boolean isInMultiWindowMode =
                mMultiWindowModeStateDispatcher != null
                        && mMultiWindowModeStateDispatcher.isInMultiWindowMode();

        final ColorStateList activityFocusTint =
                !isInMultiWindowMode
                        ? iconTint
                        : ThemeColorProvider.calculateActivityFocusTint(
                                mActivityContext, brandedColorScheme, mIsTopResumedActivity);
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
        mActivityLifecycleDispatcher.unregister(this);
        if (mMultiWindowModeStateDispatcher != null) {
            mMultiWindowModeStateDispatcher.removeObserver(this);
            mMultiWindowModeStateDispatcher = null;
        }
    }

    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        // TODO (crbug/328055199): Check if losing focus to a non-Chrome task.
        mIsTopResumedActivity = isTopResumedActivity;
        updateTheme();
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.ui.styles.ChromeColors;

/** A ThemeColorProvider for the app theme (incognito or standard theming). */
public class AppThemeColorProvider extends ThemeColorProvider implements IncognitoStateObserver {
    /** Primary color for standard mode. */
    private final int mStandardPrimaryColor;

    /** Primary color for incognito mode. */
    private final int mIncognitoPrimaryColor;

    /** Used to know when incognito mode is entered or exited. */
    private IncognitoStateProvider mIncognitoStateProvider;

    /** The overview mode manager. */
    private OverviewModeBehavior mOverviewModeBehavior;

    /** Observer to know when overview mode is entered/exited. */
    private final OverviewModeObserver mOverviewModeObserver;

    /** Whether app is in incognito mode. */
    private boolean mIsIncognito;

    /** Whether app is in overview mode. */
    private boolean mIsOverviewVisible;

    /** The activity {@link Context}. */
    private final Context mActivityContext;

    AppThemeColorProvider(Context context) {
        super(context);

        mActivityContext = context;
        mStandardPrimaryColor = ChromeColors.getDefaultThemeColor(context.getResources(), false);
        mIncognitoPrimaryColor = ChromeColors.getDefaultThemeColor(context.getResources(), true);

        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                mIsOverviewVisible = true;
                updateTheme();
            }

            @Override
            public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
                mIsOverviewVisible = false;
                updateTheme();
            }
        };
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

    void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        mOverviewModeBehavior = overviewModeBehavior;
        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
    }

    private void updateTheme() {
        final boolean isAccessibilityEnabled = DeviceClassManager.enableAccessibilityLayout();
        final boolean isHorizontalTabSwitcherEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID);
        final boolean isTabGridEnabled = FeatureUtilities.isGridTabSwitcherEnabled();
        final boolean shouldUseIncognitoBackground = mIsIncognito
                && (isAccessibilityEnabled || isHorizontalTabSwitcherEnabled || isTabGridEnabled
                        || !mIsOverviewVisible);

        updatePrimaryColor(
                shouldUseIncognitoBackground ? mIncognitoPrimaryColor : mStandardPrimaryColor,
                false);
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mIncognitoStateProvider != null) {
            mIncognitoStateProvider.removeObserver(this);
            mIncognitoStateProvider = null;
        }
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
            mOverviewModeBehavior = null;
        }
    }
}

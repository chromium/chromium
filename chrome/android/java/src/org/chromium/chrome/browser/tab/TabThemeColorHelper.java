// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.graphics.Color;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.previews.Previews;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.NetError;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.ColorUtils;

/**
 * Manages theme color used for {@link Tab}. Destroyed together with the tab.
 */
public class TabThemeColorHelper extends EmptyTabObserver implements UserData {
    private static final Class<TabThemeColorHelper> USER_DATA_KEY = TabThemeColorHelper.class;
    private final TabImpl mTab;

    private int mDefaultColor;
    private int mColor;

    /**
     * The default background color used for {@link #mTab} if the associate web content doesn't
     * specify a background color.
     */
    private int mDefaultBackgroundColor;

    /** Whether or not the default color is used. */
    private boolean mIsDefaultColorUsed;

    /**
     * Whether or not the color provided by the web page is used. False if the web page does not
     * provide a custom theme color.
     */
    private boolean mIsUsingColorFromTabContents;

    public static void createForTab(Tab tab) {
        assert get(tab) == null;
        tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabThemeColorHelper(tab));
    }

    @Nullable
    public static TabThemeColorHelper get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /** Convenience method that returns theme color of {@link Tab}. */
    public static int getColor(Tab tab) {
        return get(tab).getColor();
    }

    /** Convenience method that returns default theme color of {@link Tab}. */
    public static int getDefaultColor(Tab tab) {
        return get(tab).getDefaultColor();
    }

    /** @return Whether default theme color is used for the specified {@link Tab}. */
    public static boolean isDefaultColorUsed(Tab tab) {
        return get(tab).mIsDefaultColorUsed;
    }

    /** @return Whether the color provided by the web page is used for the specified {@link Tab}. */
    public static boolean isUsingColorFromTabContents(Tab tab) {
        return get(tab).mIsUsingColorFromTabContents;
    }

    /** @return Whether background color of the specified {@link Tab}. */
    public static int getBackgroundColor(Tab tab) {
        return get(tab).getBackgroundColor();
    }

    private TabThemeColorHelper(Tab tab) {
        mTab = (TabImpl) tab;
        mDefaultColor = calculateDefaultColor();
        mIsDefaultColorUsed = true;
        mIsUsingColorFromTabContents = false;
        mColor = mDefaultColor;
        updateDefaultBackgroundColor();
        tab.addObserver(this);

        updateThemeColor(false);
    }

    private void updateDefaultColor() {
        mDefaultColor = calculateDefaultColor();
        updateIfNeeded(false);
    }

    private int calculateDefaultColor() {
        return ChromeColors.getDefaultThemeColor(
                mTab.getContext().getResources(), mTab.isIncognito());
    }

    private void updateDefaultBackgroundColor() {
        mDefaultBackgroundColor =
                ChromeColors.getPrimaryBackgroundColor(mTab.getContext().getResources(), false);
    }

    /**
     * Updates the theme color based on if the page is native, the theme color changed, etc.
     * @param didWebContentsThemeColorChange If the theme color of the web contents is known to have
     *                                       changed.
     */
    private void updateThemeColor(boolean didWebContentsThemeColorChange) {
        // Start by assuming the current theme color is the one that should be used. This will
        // either be transparent, the last theme color, or the color restored from TabState.
        int themeColor = mColor;

        // Only use the web contents for the theme color if it is known to have changed. This
        // corresponds to the didChangeThemeColor in WebContentsObserver.
        if (mTab.getWebContents() != null && didWebContentsThemeColorChange) {
            themeColor = mTab.getWebContents().getThemeColor();
            mIsUsingColorFromTabContents = themeColor != TabState.UNSPECIFIED_THEME_COLOR
                    && ColorUtils.isValidThemeColor(themeColor);
            if (mIsUsingColorFromTabContents) {
                mIsDefaultColorUsed = false;
            }
        }

        boolean isThemingAllowed = checkThemingAllowed();
        if (!isThemingAllowed) {
            mIsUsingColorFromTabContents = false;
        }
        if (!mIsUsingColorFromTabContents) {
            themeColor = mDefaultColor;
            mIsDefaultColorUsed = true;
            if (mTab.getActivity() != null && isThemingAllowed) {
                int customThemeColor = mTab.getActivity().getActivityThemeColor();
                if (customThemeColor != TabState.UNSPECIFIED_THEME_COLOR) {
                    themeColor = customThemeColor;
                    mIsDefaultColorUsed = false;
                }
            }
        }

        // Ensure there is no alpha component to the theme color as that is not supported in the
        // dependent UI.
        mColor = themeColor | 0xFF000000;
    }

    /**
     * Returns whether theming the activity is allowed (either by the web contents or by the
     * activity).
     */
    private boolean checkThemingAllowed() {
        // Do not apply the theme color if there are any security issues on the page.
        final int securityLevel =
                SecurityStateModel.getSecurityLevelForWebContents(mTab.getWebContents());
        return securityLevel != ConnectionSecurityLevel.DANGEROUS
                && securityLevel != ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT
                && (mTab.getActivity() == null || !mTab.getActivity().isTablet())
                && (mTab.getActivity() == null
                        || !mTab.getActivity().getNightModeStateProvider().isInNightMode())
                && !mTab.isNativePage() && !mTab.isIncognito() && !Previews.isPreview(mTab);
    }

    /**
     * Determines if the theme color has changed and notifies the listeners if it has.
     * @param didWebContentsThemeColorChange If the theme color of the web contents is known to have
     *                                       changed.
     */
    public void updateIfNeeded(boolean didWebContentsThemeColorChange) {
        int oldThemeColor = mColor;
        updateThemeColor(didWebContentsThemeColorChange);
        if (oldThemeColor == mColor) return;
        mTab.notifyThemeColorChanged(mColor);
    }

    /**
     * @return The default theme color for this tab.
     */
    @VisibleForTesting
    public int getDefaultColor() {
        return mDefaultColor;
    }

    /**
     * @return The current theme color based on the value passed from the web contents and the
     *         security state.
     */
    public int getColor() {
        return mColor;
    }

    /**
     * Returns the background color of the associate web content of {@link #mTab}, or the default
     * background color if the web content background color is not specified (i.e. transparent).
     * See native WebContentsAndroid#GetBackgroundColor.
     * @return The background color of {@link #mTab}.
     */
    public int getBackgroundColor() {
        if (mTab.isNativePage()) return mTab.getNativePage().getBackgroundColor();

        WebContents tabWebContents = mTab.getWebContents();
        RenderWidgetHostView rwhv =
                tabWebContents == null ? null : tabWebContents.getRenderWidgetHostView();
        final int backgroundColor = rwhv != null ? rwhv.getBackgroundColor() : Color.TRANSPARENT;
        return backgroundColor == Color.TRANSPARENT ? mDefaultBackgroundColor : backgroundColor;
    }

    // TabObserver

    @Override
    public void onInitialized(
            Tab tab, String appId, @Nullable Boolean hasThemeColor, int themeColor) {
        if (hasThemeColor == null) return;

        // Update from TabState.
        mIsUsingColorFromTabContents = hasThemeColor;
        mIsDefaultColorUsed = !mIsUsingColorFromTabContents;
        mColor = mIsDefaultColorUsed ? getDefaultColor() : themeColor;
        updateIfNeeded(false);
    }

    @Override
    public void onSSLStateUpdated(Tab tab) {
        updateIfNeeded(false);
    }

    @Override
    public void onUrlUpdated(Tab tab) {
        updateIfNeeded(false);
    }

    @Override
    public void onDidFailLoad(Tab tab, boolean isMainFrame, int errorCode, String failingUrl) {
        updateIfNeeded(true);
    }

    @Override
    public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
        if (navigation.errorCode() != NetError.OK) updateIfNeeded(true);
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        updateDefaultColor();
        updateDefaultBackgroundColor();
        updateIfNeeded(/* didWebContentsThemeColorChange= */ true);
    }

    @Override
    public void onDestroyed(Tab tab) {
        tab.removeObserver(this);
    }
}

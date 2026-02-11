// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.security_state.ConnectionMaliciousContentStatus;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.url.GURL;

/** Implementing {@link LocationBarDataProvider} to provide state to the fusebox. */
@NullMarked
public class TabBottomSheetFuseboxDataProvider implements LocationBarDataProvider {
    private final NonNullObservableSupplier<@ControlsPosition Integer> mToolbarPosition =
            ObservableSuppliers.createNonNull(ControlsPosition.TOP);
    private final UserDataHost mUserDataHost = new UserDataHost();
    private @ColorInt int mPrimaryColor;
    private boolean mIsIncognito;

    void initialize(Context context, boolean isIncognito) {
        mPrimaryColor = ChromeColors.getPrimaryBackgroundColor(context, isIncognito);
        mIsIncognito = isIncognito;
    }

    void destroy() {
        mUserDataHost.destroy();
    }

    @Override
    public boolean isUsingBrandColor() {
        return false;
    }

    @Override
    public boolean isIncognito() {
        return mIsIncognito;
    }

    @Override
    public boolean isIncognitoBranded() {
        return mIsIncognito;
    }

    @Override
    public boolean isOffTheRecord() {
        return false;
    }

    @Override
    public UrlBarData getUrlBarData() {
        return UrlBarData.EMPTY;
    }

    @Override
    public String getTitle() {
        return "";
    }

    @Override
    public @Nullable Tab getTab() {
        return null;
    }

    @Override
    public boolean hasTab() {
        return false;
    }

    @Override
    public UserDataHost getUserDataHost() {
        return mUserDataHost;
    }

    @Override
    public int getPrimaryColor() {
        return mPrimaryColor;
    }

    @Override
    public NewTabPageDelegate getNewTabPageDelegate() {
        return new NewTabPageDelegate() {
            @Override
            public boolean isCurrentlyVisible() {
                // Required to show the G on the fusebox.
                return true;
            }
        };
    }

    @Override
    public boolean isLoading() {
        return false;
    }

    @Override
    public void addObserver(Observer observer) {}

    @Override
    public void removeObserver(Observer observer) {}

    @Override
    public GURL getCurrentGurl() {
        return GURL.emptyGURL();
    }

    @Override
    public boolean isOfflinePage() {
        return false;
    }

    @Override
    public @ConnectionSecurityLevel int getSecurityLevel() {
        return ConnectionSecurityLevel.NONE;
    }

    @Override
    public @ConnectionMaliciousContentStatus int getMaliciousContentStatus() {
        return ConnectionMaliciousContentStatus.NONE;
    }

    @Override
    public int getPageClassification(boolean prefetch) {
        return PageClassification.CO_BROWSING_COMPOSEBOX_VALUE;
    }

    @Override
    public @DrawableRes int getSecurityIconResource(boolean isTablet) {
        return 0;
    }

    @Override
    public @ColorRes int getSecurityIconColorStateList() {
        return 0;
    }

    @Override
    public @StringRes int getSecurityIconContentDescriptionResourceId() {
        return Resources.ID_NULL;
    }

    @Override
    public NonNullObservableSupplier<@ControlsPosition Integer> getToolbarPositionSupplier() {
        return mToolbarPosition;
    }
}

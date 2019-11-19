// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ssl.SecurityStateModel;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.WebContents;

/**
 * Provides a trusted CDN publisher URL for the current web contents in a Tab.
 */
public class TrustedCdn extends TabWebContentsUserData {
    private static final Class<TrustedCdn> USER_DATA_KEY = TrustedCdn.class;

    private final Tab mTab;
    private final long mNativeTrustedCdn;

    /**
     * The publisher URL for pages hosted on a trusted CDN, or null otherwise.
     */
    private String mPublisherUrl;

    /**
     *  @return The publisher URL if the current page is hosted on a trusted CDN, or null otherwise
     */
    @Nullable
    public static String getPublisherUrl(Tab tab) {
        TrustedCdn cdn = get(tab);
        return cdn != null ? cdn.getPublisherUrl() : null;
    }

    static TrustedCdn from(Tab tab) {
        TrustedCdn trustedCdn = get(tab);
        if (trustedCdn == null) {
            trustedCdn = tab.getUserDataHost().setUserData(USER_DATA_KEY, new TrustedCdn(tab));
        }
        return trustedCdn;
    }

    private static TrustedCdn get(Tab tab) {
        return tab != null ? tab.getUserDataHost().getUserData(USER_DATA_KEY) : null;
    }

    private TrustedCdn(Tab tab) {
        super(tab);
        mTab = tab;
        mNativeTrustedCdn = TrustedCdnJni.get().init(TrustedCdn.this);
    }

    @Override
    public void initWebContents(WebContents webContents) {
        TrustedCdnJni.get().setWebContents(mNativeTrustedCdn, TrustedCdn.this, webContents);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {
        TrustedCdnJni.get().resetWebContents(mNativeTrustedCdn, TrustedCdn.this);
        mPublisherUrl = null;
    }

    @Override
    public void destroyInternal() {
        TrustedCdnJni.get().onDestroyed(mNativeTrustedCdn, TrustedCdn.this);
    }

    @Nullable
    private String getPublisherUrl() {
        ChromeActivity activity = mTab.getActivity();
        if (activity == null) return null;
        if (!activity.canShowTrustedCdnPublisherUrl()) return null;
        if (getSecurityLevel() == ConnectionSecurityLevel.DANGEROUS) return null;
        return mPublisherUrl;
    }

    private int getSecurityLevel() {
        return SecurityStateModel.getSecurityLevelForWebContents(mTab.getWebContents());
    }

    @CalledByNative
    private void setPublisherUrl(@Nullable String url) {
        mPublisherUrl = url;
    }

    @NativeMethods
    interface Natives {
        long init(TrustedCdn caller);
        void onDestroyed(long nativeTrustedCdn, TrustedCdn caller);
        void setWebContents(long nativeTrustedCdn, TrustedCdn caller, WebContents webContents);
        void resetWebContents(long nativeTrustedCdn, TrustedCdn caller);
    }
}

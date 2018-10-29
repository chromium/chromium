// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.CustomTabsSessionToken;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * A model class that parses intent from third-party apps for data related with various browser
 * services related Intent types.
 */
public class BrowserSessionDataProvider {
    private static final String TAG = "BrowserSessionData";

    /**
     * Extra used to keep the caller alive. Its value is an Intent.
     */
    public static final String EXTRA_KEEP_ALIVE = "android.support.customtabs.extra.KEEP_ALIVE";

    private static final String ANIMATION_BUNDLE_PREFIX =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ? "android:activity." : "android:";
    private static final String BUNDLE_PACKAGE_NAME = ANIMATION_BUNDLE_PREFIX + "packageName";
    private static final String BUNDLE_ENTER_ANIMATION_RESOURCE =
            ANIMATION_BUNDLE_PREFIX + "animEnterRes";
    private static final String BUNDLE_EXIT_ANIMATION_RESOURCE =
            ANIMATION_BUNDLE_PREFIX + "animExitRes";

    private final CustomTabsSessionToken mSession;
    private final boolean mIsTrustedIntent;
    private final Intent mKeepAliveServiceIntent;
    private Bundle mAnimationBundle;

    /**
     * Constructs a {@link BrowserSessionDataProvider}.
     */
    public BrowserSessionDataProvider(Intent intent) {
        if (intent == null) assert false;
        mSession = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        mIsTrustedIntent = IntentHandler.notSecureIsIntentChromeOrFirstParty(intent);

        mAnimationBundle = IntentUtils.safeGetBundleExtra(
                intent, CustomTabsIntent.EXTRA_EXIT_ANIMATION_BUNDLE);

        mKeepAliveServiceIntent = IntentUtils.safeGetParcelableExtra(intent, EXTRA_KEEP_ALIVE);
    }

    /**
     * @return The session specified in the intent, or null.
     */
    public CustomTabsSessionToken getSession() {
        return mSession;
    }

    /**
     * @return The keep alive service intent specified in the intent, or null.
     */
    public Intent getKeepAliveServiceIntent() {
        return mKeepAliveServiceIntent;
    }

    /**
     * @return Whether chrome should animate when it finishes. We show animations only if the client
     *         app has supplied the correct animation resources via intent extra.
     */
    public boolean shouldAnimateOnFinish() {
        return mAnimationBundle != null && getClientPackageName() != null;
    }

    /**
     * @return The package name of the client app. This is used for a workaround in order to
     *         retrieve the client's animation resources.
     */
    public String getClientPackageName() {
        if (mAnimationBundle == null) return null;
        return mAnimationBundle.getString(BUNDLE_PACKAGE_NAME);
    }

    /**
     * @return The resource id for enter animation, which is used in
     *         {@link Activity#overridePendingTransition(int, int)}.
     */
    public int getAnimationEnterRes() {
        return shouldAnimateOnFinish() ? mAnimationBundle.getInt(BUNDLE_ENTER_ANIMATION_RESOURCE)
                                       : 0;
    }

    /**
     * @return The resource id for exit animation, which is used in
     *         {@link Activity#overridePendingTransition(int, int)}.
     */
    public int getAnimationExitRes() {
        return shouldAnimateOnFinish() ? mAnimationBundle.getInt(BUNDLE_EXIT_ANIMATION_RESOURCE)
                                       : 0;
    }

    /**
     * Checks whether or not the Intent is from Chrome or other trusted first party.
     *
     * @deprecated This method is not reliable, see https://crbug.com/832124
     */
    @Deprecated
    public boolean isTrustedIntent() {
        return mIsTrustedIntent;
    }
}

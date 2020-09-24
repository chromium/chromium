// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.BUNDLE_ENTER_ANIMATION_RESOURCE;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.BUNDLE_EXIT_ANIMATION_RESOURCE;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.BUNDLE_PACKAGE_NAME;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_CHROME;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.EXTRA_UI_TYPE;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.isTrustedCustomTab;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.TintedDrawable;

import java.util.ArrayList;
import java.util.List;

/**
 * A model class that parses the incoming intent for incognito Custom Tabs specific customization
 * data.
 *
 * Lifecycle: is activity-scoped, i.e. one instance per CustomTabActivity instance. Must be
 * re-created when color scheme changes, which happens automatically since color scheme change leads
 * to activity re-creation.
 */
public class IncognitoCustomTabIntentDataProvider extends BrowserServicesIntentDataProvider {
    private static final int MAX_CUSTOM_MENU_ITEMS = 5;

    private final Intent mIntent;
    private final CustomTabsSessionToken mSession;
    private final boolean mIsTrustedIntent;
    private final Bundle mAnimationBundle;
    private final int mToolbarColor;
    private final int mBottomBarColor;
    private final Drawable mCloseButtonIcon;
    private final boolean mShowShareItem;
    private final List<Pair<String, PendingIntent>> mMenuEntries = new ArrayList<>();

    @Nullable
    private final String mUrlToLoad;

    /** Whether this CustomTabActivity was explicitly started by another Chrome Activity. */
    private final boolean mIsOpenedByChrome;

    private final @CustomTabsUiType int mUiType;

    /**
     * Constructs a {@link IncognitoCustomTabIntentDataProvider}.
     * Incognito CCT would have a fix color scheme.
     */
    public IncognitoCustomTabIntentDataProvider(Intent intent, Context context, int colorScheme) {
        assert intent != null;
        mIntent = intent;
        mUrlToLoad = resolveUrlToLoad(intent);
        mSession = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        mIsTrustedIntent = isTrustedCustomTab(intent, mSession);
        mAnimationBundle = IntentUtils.safeGetBundleExtra(
                intent, CustomTabsIntent.EXTRA_EXIT_ANIMATION_BUNDLE);
        mIsOpenedByChrome =
                IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OPENED_BY_CHROME, false);
        mToolbarColor = ChromeColors.getDefaultThemeColor(
                context.getResources(), /*forceDarkBgColor*/ true);
        mBottomBarColor = ChromeColors.getDefaultThemeColor(
                context.getResources(), /*forceDarkBgColor*/ true);
        mCloseButtonIcon = TintedDrawable.constructTintedDrawable(context, R.drawable.btn_close);
        mShowShareItem = IntentUtils.safeGetBooleanExtra(
                intent, CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM, false);

        mUiType = getUiType(intent);
        updateExtraMenuItemsIfNecessary(intent);
    }

    private static @CustomTabsUiType int getUiType(Intent intent) {
        if (isForPaymentsFlow(intent)) return CustomTabsUiType.PAYMENT_REQUEST;
        if (isForReaderMode(intent)) return CustomTabsUiType.READER_MODE;

        return CustomTabsUiType.DEFAULT;
    }

    private static boolean isIncognitoRequested(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(
                intent, IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);
    }

    private static boolean isForPaymentsFlow(Intent intent) {
        final int requestedUiType =
                IntentUtils.safeGetIntExtra(intent, EXTRA_UI_TYPE, CustomTabsUiType.DEFAULT);
        return (isTrustedIntent(intent) && (requestedUiType == CustomTabsUiType.PAYMENT_REQUEST));
    }

    private static boolean isForReaderMode(Intent intent) {
        final int requestedUiType =
                IntentUtils.safeGetIntExtra(intent, EXTRA_UI_TYPE, CustomTabsUiType.DEFAULT);
        return (isTrustedIntent(intent) && (requestedUiType == CustomTabsUiType.READER_MODE));
    }

    private static boolean isTrustedIntent(Intent intent) {
        CustomTabsSessionToken session = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        boolean isOpenedByChrome =
                IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OPENED_BY_CHROME, false);
        return isTrustedCustomTab(intent, session) && isOpenedByChrome;
    }

    private static boolean isAllowedToAddCustomMenuItem(Intent intent) {
        // Only READER_MODE is supported for now.
        return isForReaderMode(intent);
    }

    private void updateExtraMenuItemsIfNecessary(Intent intent) {
        if (!isAllowedToAddCustomMenuItem(intent)) return;

        List<Bundle> menuItems =
                IntentUtils.getParcelableArrayListExtra(intent, CustomTabsIntent.EXTRA_MENU_ITEMS);
        if (menuItems == null) return;

        for (int i = 0; i < Math.min(MAX_CUSTOM_MENU_ITEMS, menuItems.size()); i++) {
            Bundle bundle = menuItems.get(i);
            String title = IntentUtils.safeGetString(bundle, CustomTabsIntent.KEY_MENU_ITEM_TITLE);
            PendingIntent pendingIntent =
                    IntentUtils.safeGetParcelable(bundle, CustomTabsIntent.KEY_PENDING_INTENT);
            if (TextUtils.isEmpty(title) || pendingIntent == null) continue;
            mMenuEntries.add(new Pair<String, PendingIntent>(title, pendingIntent));
        }
    }

    // TODO(https://crbug.com/1023759): Remove this function and enable
    // incognito CCT request for all apps.
    public static boolean isValidIncognitoIntent(Intent intent) {
        if (!isIncognitoRequested(intent)) return false;
        // Incognito requests for payments flow are supported without
        // INCOGNITO_CCT flag as an exceptional case that can use Chrome
        // incognito profile.
        if (isForPaymentsFlow(intent)) return true;
        assert ChromeFeatureList.isInitialized();
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_INCOGNITO);
    }

    private String resolveUrlToLoad(Intent intent) {
        return IntentHandler.getUrlFromIntent(intent);
    }

    @Override
    public @ActivityType int getActivityType() {
        return ActivityType.CUSTOM_TAB;
    }

    @Override
    @Nullable
    public Intent getIntent() {
        return mIntent;
    }

    @Override
    @Nullable
    public CustomTabsSessionToken getSession() {
        return mSession;
    }

    @Override
    public boolean shouldAnimateOnFinish() {
        return mAnimationBundle != null && getClientPackageName() != null;
    }

    @Override
    public String getClientPackageName() {
        if (mAnimationBundle == null) return null;
        return mAnimationBundle.getString(BUNDLE_PACKAGE_NAME);
    }

    @Override
    public int getAnimationEnterRes() {
        return shouldAnimateOnFinish() ? mAnimationBundle.getInt(BUNDLE_ENTER_ANIMATION_RESOURCE)
                                       : 0;
    }

    @Override
    public int getAnimationExitRes() {
        return shouldAnimateOnFinish() ? mAnimationBundle.getInt(BUNDLE_EXIT_ANIMATION_RESOURCE)
                                       : 0;
    }

    @Deprecated
    @Override
    public boolean isTrustedIntent() {
        return mIsTrustedIntent;
    }

    @Override
    @Nullable
    public String getUrlToLoad() {
        return mUrlToLoad;
    }

    @Override
    public boolean shouldEnableUrlBarHiding() {
        return false;
    }

    @Override
    public int getToolbarColor() {
        return mToolbarColor;
    }

    @Override
    @Nullable
    public Drawable getCloseButtonDrawable() {
        return mCloseButtonIcon;
    }

    @Override
    public boolean shouldShowShareMenuItem() {
        return mShowShareItem;
    }

    @Override
    public int getBottomBarColor() {
        return mBottomBarColor;
    }

    @Override
    public boolean isOpenedByChrome() {
        return mIsOpenedByChrome;
    }

    @Override
    public boolean shouldShowStarButton() {
        return true;
    }

    @Override
    public boolean shouldShowDownloadButton() {
        return false;
    }

    @Override
    public boolean isIncognito() {
        return true;
    }

    @Override
    @CustomTabsUiType
    public int getUiType() {
        return mUiType;
    }

    @Override
    public List<String> getMenuTitles() {
        ArrayList<String> list = new ArrayList<>();
        for (Pair<String, PendingIntent> pair : mMenuEntries) {
            list.add(pair.first);
        }
        return list;
    }
}

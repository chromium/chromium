// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_DEFAULT;

import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.BUNDLE_ENTER_ANIMATION_RESOURCE;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.BUNDLE_EXIT_ANIMATION_RESOURCE;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.BUNDLE_PACKAGE_NAME;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.EXTRA_UI_TYPE;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.getClientPackageNameFromSessionOrCallingActivity;
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
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsFeatureUsage.CustomTabsFeature;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.TintedDrawable;

import java.util.ArrayList;
import java.util.List;

/**
 * A model class that parses the incoming intent for incognito Custom Tabs specific customization
 * data.
 *
 * <p>Lifecycle: is activity-scoped, i.e. one instance per CustomTabActivity instance. Must be
 * re-created when color scheme changes, which happens automatically since color scheme change leads
 * to activity re-creation.
 */
public class IncognitoCustomTabIntentDataProvider extends BrowserServicesIntentDataProvider {
    private static final int MAX_CUSTOM_MENU_ITEMS = 7;
    private final Intent mIntent;
    private final CustomTabsSessionToken mSession;
    private final boolean mIsTrustedIntent;
    private final Bundle mAnimationBundle;
    private final ColorProvider mColorProvider;
    private final int mTitleVisibilityState;
    private final Drawable mCloseButtonIcon;
    private final boolean mShowShareItem;
    private final List<Pair<String, PendingIntent>> mMenuEntries = new ArrayList<>();

    @Nullable private final String mUrlToLoad;
    private final String mSendersPackageName;

    /** Whether this CustomTabActivity was explicitly started by another Chrome Activity. */
    private final boolean mIsOpenedByChrome;

    private final @CustomTabsUiType int mUiType;

    /** Constructs a {@link IncognitoCustomTabIntentDataProvider}. */
    public IncognitoCustomTabIntentDataProvider(Intent intent, Context context, int colorScheme) {
        assert intent != null;
        mIntent = intent;
        mUrlToLoad = resolveUrlToLoad(intent);
        mSession = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        mSendersPackageName = getClientPackageNameFromSessionOrCallingActivity(intent, mSession);
        mIsTrustedIntent = isTrustedCustomTab(intent, mSession);
        assert isOffTheRecord();
        mAnimationBundle =
                IntentUtils.safeGetBundleExtra(
                        intent, CustomTabsIntent.EXTRA_EXIT_ANIMATION_BUNDLE);
        mIsOpenedByChrome = IntentHandler.wasIntentSenderChrome(intent);
        mColorProvider = new IncognitoCustomTabColorProvider(context);

        mCloseButtonIcon = TintedDrawable.constructTintedDrawable(context, R.drawable.btn_close);
        mShowShareItem =
                IntentUtils.safeGetBooleanExtra(
                        intent, CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM, false);
        mTitleVisibilityState =
                IntentUtils.safeGetIntExtra(
                        intent,
                        CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE,
                        CustomTabsIntent.NO_TITLE);

        mUiType = getUiType(intent);
        updateExtraMenuItemsIfNecessary(intent);

        logFeatureUsage(intent);
    }

    private static @CustomTabsUiType int getUiType(Intent intent) {
        if (isForReaderMode(intent)) return CustomTabsUiType.READER_MODE;

        return CustomTabsUiType.DEFAULT;
    }

    private static boolean isIncognitoRequested(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(
                intent, IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);
    }

    private static boolean isForReaderMode(Intent intent) {
        final int requestedUiType =
                IntentUtils.safeGetIntExtra(intent, EXTRA_UI_TYPE, CustomTabsUiType.DEFAULT);
        return (isIntentFromChrome(intent) && (requestedUiType == CustomTabsUiType.READER_MODE));
    }

    private static boolean isIntentFromThirdPartyAllowed() {
        return ChromeFeatureList.sCctIncognitoAvailableToThirdParty.isEnabled();
    }

    private static boolean isIntentFromChrome(Intent intent) {
        return IntentHandler.wasIntentSenderChrome(intent);
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

    /**
     * Logs the usage of intents of all CCT features to a large enum histogram in order to track
     * usage by apps.
     *
     * @param intent The intent used to launch the CCT.
     */
    private void logFeatureUsage(Intent intent) {
        if (!CustomTabsFeatureUsage.isEnabled()) return;
        CustomTabsFeatureUsage featureUsage = new CustomTabsFeatureUsage();

        // Ordering: Log all the features ordered by enum, when they apply.
        if (mCloseButtonIcon != null) featureUsage.log(CustomTabsFeature.EXTRA_CLOSE_BUTTON_ICON);
        if (getCloseButtonPosition() != CLOSE_BUTTON_POSITION_DEFAULT) {
            featureUsage.log(CustomTabsFeature.EXTRA_CLOSE_BUTTON_POSITION);
        }
        if (mAnimationBundle != null) {
            featureUsage.log(CustomTabsFeature.EXTRA_EXIT_ANIMATION_BUNDLE);
        }
        featureUsage.log(CustomTabsFeature.EXTRA_OPEN_NEW_INCOGNITO_TAB);
        if (mMenuEntries != null) featureUsage.log(CustomTabsFeature.EXTRA_MENU_ITEMS);
        if (getClientPackageName() != null) featureUsage.log(CustomTabsFeature.CTF_PACKAGE_NAME);
        if (IntentUtils.safeHasExtra(intent, IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE)) {
            featureUsage.log(CustomTabsFeature.EXTRA_CALLING_ACTIVITY_PACKAGE);
        }
        if (isPartialHeightCustomTab()) featureUsage.log(CustomTabsFeature.CTF_PARTIAL);
        if (isForReaderMode(intent)) featureUsage.log(CustomTabsFeature.CTF_READER_MODE);
        if (mIsOpenedByChrome) featureUsage.log(CustomTabsFeature.CTF_SENT_BY_CHROME);
        if (mShowShareItem) featureUsage.log(CustomTabsFeature.EXTRA_DEFAULT_SHARE_MENU_ITEM);
        if (mTitleVisibilityState != CustomTabsIntent.NO_TITLE) {
            featureUsage.log(CustomTabsFeature.EXTRA_TITLE_VISIBILITY_STATE);
        }
    }

    public static void addIncognitoExtrasForChromeFeatures(
            Intent intent, @IntentHandler.IncognitoCCTCallerId int chromeCallerId) {
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        intent.putExtra(IntentHandler.EXTRA_INCOGNITO_CCT_CALLER_ID, chromeCallerId);
    }

    public @IntentHandler.IncognitoCCTCallerId int getFeatureIdForMetricsCollection() {
        if (isIntentFromChrome(mIntent)) {
            assert mIntent.hasExtra(IntentHandler.EXTRA_INCOGNITO_CCT_CALLER_ID)
                    : "Intent coming from Chrome features should add the extra "
                            + "IntentHandler.EXTRA_INCOGNITO_CCT_CALLER_ID.";

            @IntentHandler.IncognitoCCTCallerId
            int incognitoCCTChromeClientId =
                    IntentUtils.safeGetIntExtra(
                            mIntent,
                            IntentHandler.EXTRA_INCOGNITO_CCT_CALLER_ID,
                            IntentHandler.IncognitoCCTCallerId.OTHER_CHROME_FEATURES);

            boolean isValidEntry =
                    (incognitoCCTChromeClientId
                                    > IntentHandler.IncognitoCCTCallerId.OTHER_CHROME_FEATURES
                            && incognitoCCTChromeClientId
                                    < IntentHandler.IncognitoCCTCallerId.NUM_ENTRIES);
            assert isValidEntry : "Invalid EXTRA_INCOGNITO_CCT_CALLER_ID value!";
            if (!isValidEntry) {
                incognitoCCTChromeClientId =
                        IntentHandler.IncognitoCCTCallerId.OTHER_CHROME_FEATURES;
            }
            return incognitoCCTChromeClientId;
        } else if (mIsTrustedIntent) {
            return IntentHandler.IncognitoCCTCallerId.GOOGLE_APPS;
        } else {
            return IntentHandler.IncognitoCCTCallerId.OTHER_APPS;
        }
    }

    public static boolean isValidIncognitoIntent(Intent intent) {
        if (!isIncognitoRequested(intent)) return false;
        var session = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        if (isIntentFromThirdPartyAllowed()
                && getClientPackageNameFromSessionOrCallingActivity(intent, session) != null) {
            return true;
        }
        boolean isTrusted = isTrustedCustomTab(intent, session);
        RecordHistogram.recordBooleanHistogram("CustomTabs.IncognitoCCTCallerIsTrusted", isTrusted);
        return isTrusted;
    }

    private String resolveUrlToLoad(Intent intent) {
        return IntentHandler.getUrlFromIntent(intent);
    }

    public String getSendersPackageName() {
        return mSendersPackageName;
    }

    @Override
    public @ActivityType int getActivityType() {
        return ActivityType.CUSTOM_TAB;
    }

    @Override
    public @Nullable Intent getIntent() {
        return mIntent;
    }

    @Override
    public @Nullable CustomTabsSessionToken getSession() {
        return mSession;
    }

    @Override
    public boolean shouldAnimateOnFinish() {
        return mAnimationBundle != null && mAnimationBundle.getString(BUNDLE_PACKAGE_NAME) != null;
    }

    @Override
    public String getClientPackageName() {
        return mSendersPackageName;
    }

    @Override
    public int getAnimationEnterRes() {
        return shouldAnimateOnFinish()
                ? mAnimationBundle.getInt(BUNDLE_ENTER_ANIMATION_RESOURCE)
                : 0;
    }

    @Override
    public int getAnimationExitRes() {
        return shouldAnimateOnFinish()
                ? mAnimationBundle.getInt(BUNDLE_EXIT_ANIMATION_RESOURCE)
                : 0;
    }

    @Override
    public boolean isTrustedIntent() {
        return mIsTrustedIntent;
    }

    @Override
    public @Nullable String getUrlToLoad() {
        return mUrlToLoad;
    }

    @Override
    public boolean shouldEnableUrlBarHiding() {
        return false;
    }

    @Override
    public ColorProvider getColorProvider() {
        return mColorProvider;
    }

    @Override
    public @Nullable Drawable getCloseButtonDrawable() {
        return mCloseButtonIcon;
    }

    @Override
    public boolean shouldShowShareMenuItem() {
        return mShowShareItem;
    }

    @Override
    public int getTitleVisibilityState() {
        return mTitleVisibilityState;
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
    public @CustomTabProfileType int getCustomTabMode() {
        return CustomTabProfileType.INCOGNITO;
    }

    @Override
    public @CustomTabsUiType int getUiType() {
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

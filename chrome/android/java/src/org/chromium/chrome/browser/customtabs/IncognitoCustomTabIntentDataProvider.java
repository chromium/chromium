// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_DEFAULT;

import static org.chromium.chrome.browser.app.tab_activity_glue.PopupCreator.EXTRA_REQUESTED_WINDOW_FEATURES;
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

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.customtabs.CustomTabsFeatureUsage.CustomTabsFeature;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.WindowFeatures;
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
@NullMarked
public class IncognitoCustomTabIntentDataProvider extends BrowserServicesIntentDataProvider {
    private static final int MAX_CUSTOM_MENU_ITEMS = 7;
    private final Intent mIntent;
    private final @Nullable SessionHolder<CustomTabsSessionToken> mSession;
    private final boolean mIsTrustedIntent;
    private final @Nullable Bundle mAnimationBundle;
    private final ColorProvider mColorProvider;
    private final int mTitleVisibilityState;
    private final Drawable mCloseButtonIcon;
    private final boolean mShowShareItem;
    private final List<Pair<String, PendingIntent>> mMenuEntries = new ArrayList<>();

    private final @Nullable String mUrlToLoad;
    private final @Nullable String mSendersPackageName;

    /** Whether this CustomTabActivity was explicitly started by another Chrome Activity. */
    private final boolean mIsOpenedByChrome;

    private final @CustomTabsUiType int mUiType;

    /** Constructs a {@link IncognitoCustomTabIntentDataProvider}. */
    public IncognitoCustomTabIntentDataProvider(Intent intent, Context context, int colorScheme) {
        assert intent != null;
        mIntent = intent;
        mUrlToLoad = IntentHandler.getUrlFromIntent(intent);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        mSession = token != null ? new SessionHolder<>(token) : null;
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
        int intentVisibilityState =
                IntentUtils.safeGetIntExtra(
                        intent,
                        CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE,
                        CustomTabsIntent.NO_TITLE);
        mTitleVisibilityState =
                BrowserServicesIntentDataProvider.customTabIntentTitleBarVisibility(
                        intentVisibilityState, false);

        mUiType = getUiType(intent);
        updateExtraMenuItemsIfNecessary(intent);

        logFeatureUsage(intent);
    }

    private static @CustomTabsUiType int getUiType(Intent intent) {
        if (isForReaderMode(intent)) return CustomTabsUiType.READER_MODE;
        if (isForPopup(intent)) return CustomTabsUiType.POPUP;

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

    private static boolean isForPopup(Intent intent) {
        final int requestedUiType =
                IntentUtils.safeGetIntExtra(intent, EXTRA_UI_TYPE, CustomTabsUiType.DEFAULT);
        return (isIntentFromChrome(intent) && (requestedUiType == CustomTabsUiType.POPUP));
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
            mMenuEntries.add(new Pair<>(title, pendingIntent));
        }
    }

    /**
     * Logs the usage of intents of all CCT features to a large enum histogram in order to track
     * usage by apps.
     *
     * @param intent The intent used to launch the CCT.
     */
    private void logFeatureUsage(Intent intent) {
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
            Intent intent, @IncognitoCctCallerId int chromeCallerId) {
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        intent.putExtra(IntentHandler.EXTRA_INCOGNITO_CCT_CALLER_ID, chromeCallerId);
    }

    @Override
    public @IncognitoCctCallerId int getFeatureIdForMetricsCollection() {
        if (isIntentFromChrome(mIntent)) {
            assert mIntent.hasExtra(IntentHandler.EXTRA_INCOGNITO_CCT_CALLER_ID)
                    : "Intent coming from Chrome features should add the extra "
                            + "IntentHandler.EXTRA_INCOGNITO_CCT_CALLER_ID.";

            @IncognitoCctCallerId
            int incognitoCctChromeClientId =
                    IntentUtils.safeGetIntExtra(
                            mIntent,
                            IntentHandler.EXTRA_INCOGNITO_CCT_CALLER_ID,
                            IncognitoCctCallerId.OTHER_CHROME_FEATURES);

            boolean isValidEntry =
                    (incognitoCctChromeClientId > IncognitoCctCallerId.OTHER_CHROME_FEATURES
                            && incognitoCctChromeClientId < IncognitoCctCallerId.NUM_ENTRIES);
            assert isValidEntry : "Invalid EXTRA_INCOGNITO_CCT_CALLER_ID value!";
            if (!isValidEntry) {
                incognitoCctChromeClientId = IncognitoCctCallerId.OTHER_CHROME_FEATURES;
            }
            return incognitoCctChromeClientId;
        } else if (mIsTrustedIntent) {
            return IncognitoCctCallerId.GOOGLE_APPS;
        } else {
            return IncognitoCctCallerId.OTHER_APPS;
        }
    }

    public static boolean isValidIncognitoIntent(Intent intent, boolean recordMetrics) {
        if (!isIncognitoRequested(intent)) return false;
        var session = SessionHolder.getSessionHolderFromIntent(intent);
        if (isIntentFromThirdPartyAllowed()
                && getClientPackageNameFromSessionOrCallingActivity(intent, session) != null) {
            return true;
        }
        boolean isTrusted = isTrustedCustomTab(intent, session);
        if (recordMetrics) {
            RecordHistogram.recordBooleanHistogram(
                    "CustomTabs.IncognitoCCTCallerIsTrusted", isTrusted);
        }
        return isTrusted;
    }

    public @Nullable String getSendersPackageName() {
        return mSendersPackageName;
    }

    @Override
    public @ActivityType int getActivityType() {
        return ActivityType.CUSTOM_TAB;
    }

    @Override
    public Intent getIntent() {
        return mIntent;
    }

    @Override
    public @Nullable SessionHolder<?> getSession() {
        return mSession;
    }

    @EnsuresNonNullIf("mAnimationBundle")
    @Override
    public boolean shouldAnimateOnFinish() {
        return mAnimationBundle != null && mAnimationBundle.getString(BUNDLE_PACKAGE_NAME) != null;
    }

    @Override
    public @Nullable String getClientPackageName() {
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
    public @TitleVisibility int getTitleVisibilityState() {
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

    @Override
    public boolean isCloseButtonEnabled() {
        return getUiType() != CustomTabsUiType.POPUP;
    }

    @Override
    public @Nullable WindowFeatures getRequestedWindowFeatures() {
        if (getUiType() != CustomTabsUiType.POPUP) {
            return null;
        }
        final Bundle bundle =
                IntentUtils.safeGetBundleExtra(getIntent(), EXTRA_REQUESTED_WINDOW_FEATURES);
        if (bundle == null) {
            return new WindowFeatures();
        }
        return new WindowFeatures(bundle);
    }
}

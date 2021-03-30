// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Pair;
import android.widget.RemoteViews;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.TrustedWebUtils;
import androidx.browser.trusted.ScreenOrientation;
import androidx.browser.trusted.TrustedWebActivityDisplayMode;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.browser.trusted.sharing.ShareData;
import androidx.browser.trusted.sharing.ShareTarget;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.version.ChromeVersionInfo;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.device.mojom.ScreenOrientationLockType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * A model class that parses the incoming intent for Custom Tabs specific customization data.
 *
 * Lifecycle: is activity-scoped, i.e. one instance per CustomTabActivity instance. Must be
 * re-created when color scheme changes, which happens automatically since color scheme change leads
 * to activity re-creation.
 */
public class CustomTabIntentDataProvider extends BrowserServicesIntentDataProvider {
    private static final String TAG = "CustomTabIntentData";

    @IntDef({LaunchSourceType.OTHER, LaunchSourceType.MEDIA_LAUNCHER_ACTIVITY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LaunchSourceType {
        int OTHER = -1;
        int MEDIA_LAUNCHER_ACTIVITY = 3;
    }

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({ShareOptionLocation.TOOLBAR, ShareOptionLocation.MENU,
            ShareOptionLocation.TOOLBAR_FULL_MENU_FALLBACK, ShareOptionLocation.NO_SPACE,
            ShareOptionLocation.SHARE_DISABLED, ShareOptionLocation.NUM_ENTRIES})
    private @interface ShareOptionLocation {
        int TOOLBAR = 0;
        int MENU = 1;
        int TOOLBAR_FULL_MENU_FALLBACK = 2;
        int NO_SPACE = 3;
        int SHARE_DISABLED = 4;

        // Must be the last one.
        int NUM_ENTRIES = 5;
    }

    /**
     * Extra used to keep the caller alive. Its value is an Intent.
     */
    public static final String EXTRA_KEEP_ALIVE = "android.support.customtabs.extra.KEEP_ALIVE";

    public static final String ANIMATION_BUNDLE_PREFIX =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ? "android:activity." : "android:";
    public static final String BUNDLE_PACKAGE_NAME = ANIMATION_BUNDLE_PREFIX + "packageName";
    public static final String BUNDLE_ENTER_ANIMATION_RESOURCE =
            ANIMATION_BUNDLE_PREFIX + "animEnterRes";
    public static final String BUNDLE_EXIT_ANIMATION_RESOURCE =
            ANIMATION_BUNDLE_PREFIX + "animExitRes";

    /**
     * Extra that indicates whether or not the Custom Tab is being launched by an Intent fired by
     * Chrome itself.
     */
    public static final String EXTRA_IS_OPENED_BY_CHROME =
            "org.chromium.chrome.browser.customtabs.IS_OPENED_BY_CHROME";

    /** URL that should be loaded in place of the URL passed along in the data. */
    public static final String EXTRA_MEDIA_VIEWER_URL =
            "org.chromium.chrome.browser.customtabs.MEDIA_VIEWER_URL";

    /** Extra that enables embedded media experience. */
    public static final String EXTRA_ENABLE_EMBEDDED_MEDIA_EXPERIENCE =
            "org.chromium.chrome.browser.customtabs.EXTRA_ENABLE_EMBEDDED_MEDIA_EXPERIENCE";

    /** Indicates the type of UI Custom Tab should use. */
    public static final String EXTRA_UI_TYPE =
            "org.chromium.chrome.browser.customtabs.EXTRA_UI_TYPE";

    /** Extra that defines the initial background color (RGB color stored as an integer). */
    public static final String EXTRA_INITIAL_BACKGROUND_COLOR =
            "org.chromium.chrome.browser.customtabs.EXTRA_INITIAL_BACKGROUND_COLOR";

    /** Extra that enables the client to disable the star button in menu. */
    public static final String EXTRA_DISABLE_STAR_BUTTON =
            "org.chromium.chrome.browser.customtabs.EXTRA_DISABLE_STAR_BUTTON";

    /** Extra that enables the client to disable the download button in menu. */
    public static final String EXTRA_DISABLE_DOWNLOAD_BUTTON =
            "org.chromium.chrome.browser.customtabs.EXTRA_DISABLE_DOWNLOAD_BUTTON";

    /**
     * Indicates the source where the Custom Tab is launched. The value is defined as
     * {@link LaunchSourceType}.
     */
    public static final String EXTRA_BROWSER_LAUNCH_SOURCE =
            "org.chromium.chrome.browser.customtabs.EXTRA_BROWSER_LAUNCH_SOURCE";

    // TODO(yusufo): Move this to CustomTabsIntent.
    /** Signals custom tabs to favor sending initial urls to external handler apps if possible. */
    public static final String EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER =
            "android.support.customtabs.extra.SEND_TO_EXTERNAL_HANDLER";

    // TODO(amalova): Move this to CustomTabsIntent.
    /**
     * Extra that, if set, specifies Translate UI should be triggered with
     * specified target language.
     */
    private static final String EXTRA_TRANSLATE_LANGUAGE =
            "androidx.browser.customtabs.extra.TRANSLATE_LANGUAGE";

    /**
     * Extra that, if set, results in blocking new notification requests while in CCT.
     */
    public static final String EXTRA_BLOCK_NEW_NOTIFICATION_REQUESTS_IN_CCT =
            "androidx.browser.customtabs.extra.BLOCK_NEW_NOTIFICATION_REQUESTS_IN_CCT";

    /**
     * Extra that, if set, results in hiding omnibox suggestions for visits from cct. The value is
     * a boolean, and is only considered if the feature kSuggestVisitsWithPageTransitionFromApi2 is
     * enabled.
     */
    public static final String EXTRA_HIDE_OMNIBOX_SUGGESTIONS_FROM_CCT =
            "androidx.browser.customtabs.extra.HIDE_OMNIBOX_SUGGESTIONS_FROM_CCT";

    /**
     * Extra that determines whether the 'open in chrome' menu item should be shown in the context
     * menu. The value is a boolean. Default value is false, meaning the item is shown.
     */
    public static final String EXTRA_HIDE_OPEN_IN_CHROME_MENU_ITEM_IN_CONTEXT_MENU =
            "androidx.browser.customtabs.extra.HIDE_OPEN_IN_CHROME_MENU_ITEM_IN_CONTEXT_MENU";

    /**
     * Extra that determines whether the 'open in chrome' menu item should be shown in the menu. The
     * value is a boolean. Default value is false, meaning the item is shown.
     */
    public static final String EXTRA_HIDE_OPEN_IN_CHROME_MENU_ITEM =
            "androidx.browser.customtabs.extra.HIDE_OPEN_IN_CHROME_MENU_ITEM";

    /**
     * Extra that, if set, results in marking visits from cct as hidden. The value is
     * a boolean, and is only considered if the feature kCCTHideVisits is enabled.
     */
    public static final String EXTRA_HIDE_VISITS_FROM_CCT =
            "androidx.browser.customtabs.extra.HIDE_VISITS_FROM_CCT";

    private static final String EXTRA_TWA_DISCLOSURE_UI =
            "androidx.browser.trusted.extra.DISCLOSURE_VERSION";

    private static final int MAX_CUSTOM_MENU_ITEMS = 5;

    private static final int MAX_CUSTOM_TOOLBAR_ITEMS = 2;

    private static final String FIRST_PARTY_PITFALL_MSG =
            "The intent contains a non-default UI type, but it is not from a first-party app. "
            + "To make locally-built Chrome a first-party app, sign with release-test "
            + "signing keys and run on userdebug devices. See use_signing_keys GN arg.";

    // Extra whose value is an array of ints that is supplied to
    // SyntheticTrialRegistry::RegisterExternalExperiments().
    public static final String EXPERIMENT_IDS =
            "org.chromium.chrome.browser.customtabs.AGA_EXPERIMENT_IDS";

    private final Intent mIntent;
    private final CustomTabsSessionToken mSession;
    private final boolean mIsTrustedIntent;
    private final Intent mKeepAliveServiceIntent;
    private Bundle mAnimationBundle;

    private final int mUiType;
    private final int mTitleVisibilityState;
    private final String mMediaViewerUrl;
    private final boolean mEnableEmbeddedMediaExperience;
    private final boolean mIsFromMediaLauncherActivity;
    private final boolean mDisableStar;
    private final boolean mDisableDownload;
    private final @ActivityType int mActivityType;
    @Nullable
    private final List<String> mTrustedWebActivityAdditionalOrigins;
    @Nullable
    private final TrustedWebActivityDisplayMode mTrustedWebActivityDisplayMode;
    @Nullable
    private String mUrlToLoad;

    private boolean mEnableUrlBarHiding;
    private List<CustomButtonParams> mCustomButtonParams;
    private Drawable mCloseButtonIcon;
    private List<Pair<String, PendingIntent>> mMenuEntries = new ArrayList<>();
    private boolean mShowShareItemInMenu;
    private List<CustomButtonParams> mToolbarButtons = new ArrayList<>(1);
    private List<CustomButtonParams> mBottombarButtons = new ArrayList<>(2);
    private RemoteViews mRemoteViews;
    private int[] mClickableViewIds;
    private PendingIntent mRemoteViewsPendingIntent;
    // OnFinished listener for PendingIntents. Used for testing only.
    private PendingIntent.OnFinished mOnFinished;

    /** Whether this CustomTabActivity was explicitly started by another Chrome Activity. */
    private final boolean mIsOpenedByChrome;

    /** ISO 639 language code */
    @Nullable
    private final String mTranslateLanguage;
    private final int mDefaultOrientation;

    @Nullable
    private final int[] mGsaExperimentIds;

    @NonNull
    private final CustomTabColorProvider mColorProvider;

    /**
     * Add extras to customize menu items for opening Reader Mode UI custom tab from Chrome.
     */
    public static void addReaderModeUIExtras(Intent intent) {
        intent.putExtra(EXTRA_UI_TYPE, CustomTabsUiType.READER_MODE);
        intent.putExtra(EXTRA_IS_OPENED_BY_CHROME, true);
        IntentHandler.addTrustedIntentExtras(intent);
    }

    /**
     * Evaluates whether the passed Intent and/or CustomTabsSessionToken are
     * from a trusted source. Trusted in this case means from the app itself or
     * via a first-party application.
     *
     * @param intent The Intent used to start the custom tabs activity, or null.
     * @param session The connected session for the custom tabs activity, or null.
     * @return True if the intent or session are trusted.
     */
    public static boolean isTrustedCustomTab(Intent intent, CustomTabsSessionToken session) {
        return IntentHandler.wasIntentSenderChrome(intent)
                || CustomTabsConnection.getInstance().isSessionFirstParty(session);
    }

    /**
     * Constructs a {@link CustomTabIntentDataProvider}.
     *
     * The colorScheme parameter specifies which color scheme the Custom Tab should use.
     * It can currently be either {@link CustomTabsIntent#COLOR_SCHEME_LIGHT} or
     * {@link CustomTabsIntent#COLOR_SCHEME_DARK}.
     * If Custom Tab was launched with {@link CustomTabsIntent#COLOR_SCHEME_SYSTEM}, colorScheme
     * must reflect the current system setting. When the system setting changes, a new
     * CustomTabIntentDataProvider object must be created.
     */
    public CustomTabIntentDataProvider(Intent intent, Context context, int colorScheme) {
        if (intent == null) assert false;
        mIntent = intent;

        mSession = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        mIsTrustedIntent = isTrustedCustomTab(intent, mSession);

        mAnimationBundle = IntentUtils.safeGetBundleExtra(
                intent, CustomTabsIntent.EXTRA_EXIT_ANIMATION_BUNDLE);

        mKeepAliveServiceIntent = IntentUtils.safeGetParcelableExtra(intent, EXTRA_KEEP_ALIVE);

        mIsOpenedByChrome = IntentUtils.safeGetBooleanExtra(
                intent, EXTRA_IS_OPENED_BY_CHROME, false);

        final int requestedUiType =
                IntentUtils.safeGetIntExtra(intent, EXTRA_UI_TYPE, CustomTabsUiType.DEFAULT);
        mUiType = verifiedUiType(requestedUiType);

        mColorProvider = new CustomTabColorProviderImpl(intent, context, colorScheme);

        retrieveCustomButtons(intent, context);

        mEnableUrlBarHiding = IntentUtils.safeGetBooleanExtra(
                intent, CustomTabsIntent.EXTRA_ENABLE_URLBAR_HIDING, true);

        Bitmap bitmap = IntentUtils.safeGetParcelableExtra(
                intent, CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON);
        if (bitmap != null && !checkCloseButtonSize(context, bitmap)) {
            IntentUtils.safeRemoveExtra(intent, CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON);
            bitmap.recycle();
            bitmap = null;
        }
        if (bitmap == null) {
            mCloseButtonIcon =
                    TintedDrawable.constructTintedDrawable(context, R.drawable.btn_close);
        } else {
            mCloseButtonIcon = new BitmapDrawable(context.getResources(), bitmap);
        }

        List<Bundle> menuItems =
                IntentUtils.getParcelableArrayListExtra(intent, CustomTabsIntent.EXTRA_MENU_ITEMS);
        updateExtraMenuItems(menuItems);
        addShareOption(intent, context);

        mActivityType = IntentUtils.safeGetBooleanExtra(
                                intent, TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false)
                ? ActivityType.TRUSTED_WEB_ACTIVITY
                : ActivityType.CUSTOM_TAB;
        mTrustedWebActivityAdditionalOrigins = IntentUtils.safeGetStringArrayListExtra(intent,
                TrustedWebActivityIntentBuilder.EXTRA_ADDITIONAL_TRUSTED_ORIGINS);
        mTrustedWebActivityDisplayMode = resolveTwaDisplayMode();
        mTitleVisibilityState = IntentUtils.safeGetIntExtra(
                intent, CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, CustomTabsIntent.NO_TITLE);
        mRemoteViews =
                IntentUtils.safeGetParcelableExtra(intent, CustomTabsIntent.EXTRA_REMOTEVIEWS);
        mClickableViewIds = IntentUtils.safeGetIntArrayExtra(
                intent, CustomTabsIntent.EXTRA_REMOTEVIEWS_VIEW_IDS);
        mRemoteViewsPendingIntent = IntentUtils.safeGetParcelableExtra(
                intent, CustomTabsIntent.EXTRA_REMOTEVIEWS_PENDINGINTENT);
        mMediaViewerUrl = isMediaViewer()
                ? IntentUtils.safeGetStringExtra(intent, EXTRA_MEDIA_VIEWER_URL)
                : null;
        mEnableEmbeddedMediaExperience = isTrustedIntent()
                && IntentUtils.safeGetBooleanExtra(
                           intent, EXTRA_ENABLE_EMBEDDED_MEDIA_EXPERIENCE, false);
        mIsFromMediaLauncherActivity = isTrustedIntent()
                && (IntentUtils.safeGetIntExtra(
                            intent, EXTRA_BROWSER_LAUNCH_SOURCE, LaunchSourceType.OTHER)
                           == LaunchSourceType.MEDIA_LAUNCHER_ACTIVITY);
        mDisableStar = IntentUtils.safeGetBooleanExtra(intent, EXTRA_DISABLE_STAR_BUTTON, false);
        mDisableDownload =
                IntentUtils.safeGetBooleanExtra(intent, EXTRA_DISABLE_DOWNLOAD_BUTTON, false);

        mTranslateLanguage = IntentUtils.safeGetStringExtra(intent, EXTRA_TRANSLATE_LANGUAGE);
        // Import the {@link ScreenOrientation}.
        mDefaultOrientation = convertOrientationType(IntentUtils.safeGetIntExtra(intent,
                TrustedWebActivityIntentBuilder.EXTRA_SCREEN_ORIENTATION,
                ScreenOrientation.DEFAULT));

        mGsaExperimentIds = IntentUtils.safeGetIntArrayExtra(intent, EXPERIMENT_IDS);
    }

    private void updateExtraMenuItems(List<Bundle> menuItems) {
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
     * Triggers the client-defined action when the user clicks a custom menu item.
     * @param activity The {@link Activity} to use for sending the {@link PendingIntent}.
     * @param menuIndex The index that the menu item is shown in the result of
     *                  {@link #getMenuTitles()}.
     * @param url The URL to attach as additional data to the {@link PendingIntent}.
     * @param title The title to attach as additional data to the {@link PendingIntent}.
     */
    public void clickMenuItemWithUrlAndTitle(
            Activity activity, int menuIndex, String url, String title) {
        Intent addedIntent = new Intent();
        addedIntent.setData(Uri.parse(url));
        addedIntent.putExtra(Intent.EXTRA_SUBJECT, title);
        try {
            // Media viewers pass in PendingIntents that contain CHOOSER Intents.  Setting the data
            // in these cases prevents the Intent from firing correctly.
            String menuTitle = mMenuEntries.get(menuIndex).first;
            PendingIntent pendingIntent = mMenuEntries.get(menuIndex).second;
            pendingIntent.send(
                    activity, 0, isMediaViewer() ? null : addedIntent, mOnFinished, null);
            if (shouldEnableEmbeddedMediaExperience()
                    && TextUtils.equals(
                            menuTitle, activity.getString(R.string.download_manager_open_with))) {
                RecordUserAction.record("CustomTabsMenuCustomMenuItem.DownloadsUI.OpenWith");
            }
        } catch (CanceledException e) {
            Log.e(TAG, "Custom tab in Chrome failed to send pending intent.");
        }
    }

    private boolean checkCloseButtonSize(Context context, Bitmap bitmap) {
        int size = context.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        if (bitmap.getHeight() == size && bitmap.getWidth() == size) return true;
        return false;
    }

    /**
     * Get the verified UI type, according to the intent extras, and whether the intent is trusted.
     * @param requestedUiType requested UI type in the intent, unqualified
     * @return verified UI type
     */
    private int verifiedUiType(int requestedUiType) {
        if (!isTrustedIntent()) {
            if (ChromeVersionInfo.isLocalBuild()) Log.w(TAG, FIRST_PARTY_PITFALL_MSG);
            return CustomTabsUiType.DEFAULT;
        }

        return requestedUiType;
    }

    /**
     * Gets custom buttons from the intent and updates {@link #mCustomButtonParams},
     * {@link #mBottombarButtons} and {@link #mToolbarButtons}.
     */
    private void retrieveCustomButtons(Intent intent, Context context) {
        assert mCustomButtonParams == null;
        mCustomButtonParams = CustomButtonParamsImpl.fromIntent(context, intent);
        for (CustomButtonParams params : mCustomButtonParams) {
            if (!params.showOnToolbar()) {
                mBottombarButtons.add(params);
            } else if (mToolbarButtons.size() < getMaxCustomToolbarItems()) {
                mToolbarButtons.add(params);
            } else {
                Log.w(TAG, "Only %d items are allowed in the toolbar", getMaxCustomToolbarItems());
            }
        }
    }

    private int getMaxCustomToolbarItems() {
        return MAX_CUSTOM_TOOLBAR_ITEMS;
    }

    /**
     * Adds a share option to the custom tab according to the {@link
     * CustomTabsIntent#EXTRA_SHARE_STATE} stored in the intent.
     *
     * <p>Shows share options according to the following rules:
     *
     * <ul>
     *   <li>If {@link CustomTabsIntent#SHARE_STATE_ON} or
     *   {@link CustomTabsIntent#SHARE_STATE_DEFAULT}, add to the top toolbar if empty, otherwise
     *   add to the overflow menu if it is not customized.
     *   <li>If {@link CustomTabsIntent#SHARE_STATE_OFF}, add to the overflow menu depending on
     *   {@link CustomTabsIntent#EXTRA_DEFAULT_SHARE_MENU_ITEM}.
     * </ul>
     */
    private void addShareOption(Intent intent, Context context) {
        int shareState = IntentUtils.safeGetIntExtra(
                intent, CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_DEFAULT);
        if (shareState == CustomTabsIntent.SHARE_STATE_ON
                || shareState == CustomTabsIntent.SHARE_STATE_DEFAULT) {
            if (mToolbarButtons.isEmpty()) {
                mToolbarButtons.add(
                        CustomButtonParamsImpl.createShareButton(context, getToolbarColor()));
                logShareOptionLocation(ShareOptionLocation.TOOLBAR);
            } else if (mMenuEntries.isEmpty()) {
                mShowShareItemInMenu = true;
                logShareOptionLocation(ShareOptionLocation.TOOLBAR_FULL_MENU_FALLBACK);
            } else {
                logShareOptionLocation(ShareOptionLocation.NO_SPACE);
            }
        } else {
            mShowShareItemInMenu = IntentUtils.safeGetBooleanExtra(intent,
                    CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM,
                    mIsOpenedByChrome && mUiType == CustomTabsUiType.DEFAULT);
            if (mShowShareItemInMenu) {
                logShareOptionLocation(ShareOptionLocation.MENU);
            } else {
                logShareOptionLocation(ShareOptionLocation.SHARE_DISABLED);
            }
        }
    }

    private static void logShareOptionLocation(@ShareOptionLocation int shareOptionLocation) {
        RecordHistogram.recordEnumeratedHistogram("CustomTabs.ShareOptionLocation",
                shareOptionLocation, ShareOptionLocation.NUM_ENTRIES);
    }

    private String resolveUrlToLoad(Intent intent) {
        String url = IntentHandler.getUrlFromIntent(intent);

        // Intents fired for media viewers have an additional file:// URI passed along so that the
        // tab can display the actual filename to the user when it is loaded.
        if (isMediaViewer()) {
            String mediaViewerUrl = getMediaViewerUrl();
            if (!TextUtils.isEmpty(mediaViewerUrl)) {
                Uri mediaViewerUri = Uri.parse(mediaViewerUrl);
                if (UrlConstants.FILE_SCHEME.equals(mediaViewerUri.getScheme())) {
                    url = mediaViewerUrl;
                }
            }
        }
        return url;
    }

    @Nullable
    private TrustedWebActivityDisplayMode resolveTwaDisplayMode() {
        Bundle bundle = IntentUtils.safeGetBundleExtra(mIntent,
                TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_MODE);
        if (bundle == null) {
            return null;
        }
        try {
            return TrustedWebActivityDisplayMode.fromBundle(bundle);
        } catch (Throwable e) {
            return null;
        }
    }

    /**
     * Returns the {@link ScreenOrientationLockType} which matches {@link ScreenOrientation}.
     * @param orientation {@link ScreenOrientation}
     * @return The matching ScreenOrientationLockType. {@link ScreenOrientationLockType#DEFAULT} if
     *         there is no match.
     */
    private static int convertOrientationType(@ScreenOrientation.LockType int orientation) {
        switch (orientation) {
            case ScreenOrientation.DEFAULT:
                return ScreenOrientationLockType.DEFAULT;
            case ScreenOrientation.PORTRAIT_PRIMARY:
                return ScreenOrientationLockType.PORTRAIT_PRIMARY;
            case ScreenOrientation.PORTRAIT_SECONDARY:
                return ScreenOrientationLockType.PORTRAIT_SECONDARY;
            case ScreenOrientation.LANDSCAPE_PRIMARY:
                return ScreenOrientationLockType.LANDSCAPE_PRIMARY;
            case ScreenOrientation.LANDSCAPE_SECONDARY:
                return ScreenOrientationLockType.LANDSCAPE_SECONDARY;
            case ScreenOrientation.ANY:
                return ScreenOrientationLockType.ANY;
            case ScreenOrientation.LANDSCAPE:
                return ScreenOrientationLockType.LANDSCAPE;
            case ScreenOrientation.PORTRAIT:
                return ScreenOrientationLockType.PORTRAIT;
            case ScreenOrientation.NATURAL:
                return ScreenOrientationLockType.NATURAL;
            default:
                Log.w(TAG, "The provided orientaton is not supported, orientation = %d",
                        orientation);
                return ScreenOrientationLockType.DEFAULT;
        }
    }

    @Override
    public int getDefaultOrientation() {
        return mDefaultOrientation;
    }

    @Override
    public @ActivityType int getActivityType() {
        return mActivityType;
    }

    @Override
    public Intent getIntent() {
        return mIntent;
    }

    @Override
    @Nullable
    public CustomTabsSessionToken getSession() {
        return mSession;
    }

    @Override
    @Nullable
    public Intent getKeepAliveServiceIntent() {
        return mKeepAliveServiceIntent;
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
        if (mUrlToLoad == null) {
            mUrlToLoad = resolveUrlToLoad(getIntent());
        }
        return mUrlToLoad;
    }

    @Override
    public boolean shouldEnableUrlBarHiding() {
        return mEnableUrlBarHiding;
    }

    @Override
    public int getToolbarColor() {
        return mColorProvider.getToolbarColor();
    }

    @Override
    public boolean hasCustomToolbarColor() {
        return mColorProvider.hasCustomToolbarColor();
    }

    @Override
    @Nullable
    public Integer getNavigationBarColor() {
        return mColorProvider.getNavigationBarColor();
    }

    @Override
    @Nullable
    public Integer getNavigationBarDividerColor() {
        return mColorProvider.getNavigationBarDividerColor();
    }

    @Override
    @Nullable
    public Drawable getCloseButtonDrawable() {
        return mCloseButtonIcon;
    }

    @Override
    public int getTitleVisibilityState() {
        return mTitleVisibilityState;
    }

    @Override
    public boolean shouldShowShareMenuItem() {
        return mShowShareItemInMenu;
    }

    @Override
    public List<CustomButtonParams> getCustomButtonsOnToolbar() {
        return mToolbarButtons;
    }

    @Override
    public List<CustomButtonParams> getCustomButtonsOnBottombar() {
        return mBottombarButtons;
    }

    @Override
    public int getBottomBarColor() {
        return mColorProvider.getBottomBarColor();
    }

    @Override
    @Nullable
    public RemoteViews getBottomBarRemoteViews() {
        return mRemoteViews;
    }

    @Override
    @Nullable
    public int[] getClickableViewIDs() {
        if (mClickableViewIds == null) return null;
        return mClickableViewIds.clone();
    }

    @Override
    @Nullable
    public PendingIntent getRemoteViewsPendingIntent() {
        return mRemoteViewsPendingIntent;
    }

    @Override
    public List<CustomButtonParams> getAllCustomButtons() {
        return mCustomButtonParams;
    }

    @Override
    public List<String> getMenuTitles() {
        ArrayList<String> list = new ArrayList<>();
        for (Pair<String, PendingIntent> pair : mMenuEntries) {
            list.add(pair.first);
        }
        return list;
    }

    /**
     * Set the callback object for {@link PendingIntent}s that are sent in this class. For testing
     * purpose only.
     */
    @VisibleForTesting
    void setPendingIntentOnFinishedForTesting(PendingIntent.OnFinished onFinished) {
        mOnFinished = onFinished;
    }

    @Override
    public boolean isOpenedByChrome() {
        return mIsOpenedByChrome;
    }

    @Override
    @BrowserServicesIntentDataProvider.CustomTabsUiType
    public int getUiType() {
        return mUiType;
    }

    /**
     * @return See {@link #EXTRA_MEDIA_VIEWER_URL}.
     */
    @Override
    @Nullable
    public String getMediaViewerUrl() {
        return mMediaViewerUrl;
    }

    @Override
    public boolean shouldEnableEmbeddedMediaExperience() {
        return mEnableEmbeddedMediaExperience;
    }

    @Override
    public boolean isFromMediaLauncherActivity() {
        return mIsFromMediaLauncherActivity;
    }

    @Override
    public int getInitialBackgroundColor() {
        return mColorProvider.getInitialBackgroundColor();
    }

    @Override
    public boolean shouldShowStarButton() {
        return !mDisableStar;
    }

    @Override
    public boolean shouldShowDownloadButton() {
        return !mDisableDownload;
    }

    @Override
    public boolean isIncognito() {
        return false;
    }

    @Nullable
    @Override
    public TrustedWebActivityDisplayMode getTwaDisplayMode() {
        return mTrustedWebActivityDisplayMode;
    }

    @Override
    @Nullable
    public List<String> getTrustedWebActivityAdditionalOrigins() {
        return mTrustedWebActivityAdditionalOrigins;
    }

    @Override
    @Nullable
    public String getTranslateLanguage() {
        return mTranslateLanguage;
    }

    @Override
    @Nullable
    public ShareTarget getShareTarget() {
        Bundle bundle = IntentUtils.safeGetBundleExtra(
                getIntent(), TrustedWebActivityIntentBuilder.EXTRA_SHARE_TARGET);
        if (bundle == null) return null;
        try {
            return ShareTarget.fromBundle(bundle);
        } catch (Throwable e) {
            // Catch unparcelling errors.
            return null;
        }
    }

    @Override
    @Nullable
    public ShareData getShareData() {
        Bundle bundle = IntentUtils.safeGetParcelableExtra(
                getIntent(), TrustedWebActivityIntentBuilder.EXTRA_SHARE_DATA);
        if (bundle == null) return null;
        try {
            return ShareData.fromBundle(bundle);
        } catch (Throwable e) {
            // Catch unparcelling errors.
            return null;
        }
    }

    @TwaDisclosureUi
    @Override
    public int getTwaDisclosureUi() {
        int version = mIntent.getIntExtra(EXTRA_TWA_DISCLOSURE_UI, TwaDisclosureUi.DEFAULT);

        if (version != TwaDisclosureUi.V1_INFOBAR
                && version != TwaDisclosureUi.V2_NOTIFICATION_OR_SNACKBAR) {
            return TwaDisclosureUi.DEFAULT;
        }

        return version;
    }

    @Override
    @Nullable
    public int[] getGsaExperimentIds() {
        return mGsaExperimentIds;
    }

    public static boolean shouldHideOmniboxSuggestionsForCctVisits(Intent intent) {
        return intent.getBooleanExtra(EXTRA_HIDE_OMNIBOX_SUGGESTIONS_FROM_CCT, false)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_HIDE_VISITS_FROM_CCT);
    }

    @Override
    public boolean shouldHideOmniboxSuggestionsForCctVisits() {
        return shouldHideOmniboxSuggestionsForCctVisits(mIntent);
    }

    @Override
    public boolean shouldHideCctVisits() {
        if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.HIDE_FROM_API_3_TRANSITIONS_FROM_HISTORY)) {
            return false;
        }

        // Only 1p apps are allowed to hide visits.
        String clientPackageName =
                CustomTabsConnection.getInstance().getClientPackageNameForSession(getSession());
        if (!GSAState.isGsaPackageName(clientPackageName)) return false;
        return IntentUtils.safeGetBooleanExtra(mIntent, EXTRA_HIDE_VISITS_FROM_CCT, false);
    }

    @Override
    public boolean shouldBlockNewNotificationRequests() {
        // Only 1p apps are allowed to hide visits.
        String clientPackageName =
                CustomTabsConnection.getInstance().getClientPackageNameForSession(getSession());
        if (!GSAState.isGsaPackageName(clientPackageName)) return false;
        return IntentUtils.safeGetBooleanExtra(
                mIntent, EXTRA_BLOCK_NEW_NOTIFICATION_REQUESTS_IN_CCT, false);
    }

    @Override
    public boolean shouldShowOpenInChromeMenuItemInContextMenu() {
        // Only 1p apps are allowed to hide visits.
        String clientPackageName =
                CustomTabsConnection.getInstance().getClientPackageNameForSession(getSession());
        if (!GSAState.isGsaPackageName(clientPackageName)) return true;
        return !IntentUtils.safeGetBooleanExtra(
                mIntent, EXTRA_HIDE_OPEN_IN_CHROME_MENU_ITEM_IN_CONTEXT_MENU, false);
    }

    @Override
    public boolean shouldShowOpenInChromeMenuItem() {
        // Only 1p apps are allowed to hide visits.
        String clientPackageName =
                CustomTabsConnection.getInstance().getClientPackageNameForSession(getSession());
        if (!GSAState.isGsaPackageName(clientPackageName)) return true;
        return !IntentUtils.safeGetBooleanExtra(
                mIntent, EXTRA_HIDE_OPEN_IN_CHROME_MENU_ITEM, false);
    }
}

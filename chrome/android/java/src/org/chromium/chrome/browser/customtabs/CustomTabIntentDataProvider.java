// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_HEIGHT_DEFAULT;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_HEIGHT_FIXED;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DEFAULT;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_POSITION_DEFAULT;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_POSITION_END;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_DEFAULT;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_TOP;
import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_DEFAULT;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_ACTIVITY_HEIGHT_RESIZE_BEHAVIOR;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_BREAKPOINT_DP;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_DECORATION_TYPE;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_ENABLE_MAXIMIZATION;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_POSITION;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_CLOSE_BUTTON_ENABLED;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_CLOSE_BUTTON_POSITION;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_INITIAL_ACTIVITY_HEIGHT_PX;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_INITIAL_ACTIVITY_WIDTH_PX;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_NETWORK;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_TOOLBAR_CORNER_RADIUS_DP;
import static androidx.browser.trusted.LaunchHandlerClientMode.FOCUS_EXISTING;
import static androidx.browser.trusted.LaunchHandlerClientMode.NAVIGATE_EXISTING;
import static androidx.browser.trusted.LaunchHandlerClientMode.NAVIGATE_NEW;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.app.tab_activity_glue.PopupCreator.EXTRA_REQUESTED_WINDOW_FEATURES;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.net.Network;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Pair;
import android.widget.RemoteViews;

import androidx.annotation.IntDef;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomContentAction;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsIntent.ActivityHeightResizeBehavior;
import androidx.browser.customtabs.CustomTabsIntent.ActivitySideSheetDecorationType;
import androidx.browser.customtabs.CustomTabsIntent.ActivitySideSheetRoundedCornersPosition;
import androidx.browser.customtabs.CustomTabsIntent.CloseButtonPosition;
import androidx.browser.customtabs.CustomTabsIntent.OpenInBrowserState;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.ExperimentalCustomContentAction;
import androidx.browser.customtabs.ExperimentalOpenInBrowser;
import androidx.browser.customtabs.TrustedWebUtils;
import androidx.browser.trusted.FileHandlingData;
import androidx.browser.trusted.LaunchHandlerClientMode;
import androidx.browser.trusted.ScreenOrientation;
import androidx.browser.trusted.TrustedWebActivityDisplayMode;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.browser.trusted.sharing.ShareData;
import androidx.browser.trusted.sharing.ShareTarget;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.IntentUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.customtabs.CustomTabsFeatureUsage.CustomTabsFeature;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams;
import org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderUtils;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.net.NetId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * A model class that parses the incoming intent for Custom Tabs specific customization data.
 *
 * <p>Lifecycle: is activity-scoped, i.e. one instance per CustomTabActivity instance. Must be
 * re-created when color scheme changes, which happens automatically since color scheme change leads
 * to activity re-creation.
 */
@NullMarked
public class CustomTabIntentDataProvider extends BrowserServicesIntentDataProvider {
    private static final String TAG = "CustomTabIntentData";

    @IntDef({LaunchSourceType.OTHER, LaunchSourceType.MEDIA_LAUNCHER_ACTIVITY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LaunchSourceType {
        int OTHER = -1;
        int MEDIA_LAUNCHER_ACTIVITY = 3;
    }

    @IntDef({
        BackgroundInteractBehavior.DEFAULT,
        BackgroundInteractBehavior.ON,
        BackgroundInteractBehavior.OFF
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BackgroundInteractBehavior {
        int DEFAULT = 0;
        int ON = 1;
        int OFF = 2;

        // Must be the last one.
        int NUM_ENTRIES = 3;
    }

    /** Extra used to keep the caller alive. Its value is an Intent. */
    public static final String EXTRA_KEEP_ALIVE = "android.support.customtabs.extra.KEEP_ALIVE";

    public static final String ANIMATION_BUNDLE_PREFIX = "android:activity.";
    public static final String BUNDLE_PACKAGE_NAME = ANIMATION_BUNDLE_PREFIX + "packageName";
    public static final String BUNDLE_ENTER_ANIMATION_RESOURCE =
            ANIMATION_BUNDLE_PREFIX + "animEnterRes";
    public static final String BUNDLE_EXIT_ANIMATION_RESOURCE =
            ANIMATION_BUNDLE_PREFIX + "animExitRes";

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

    /**
     * Indicates the source where the Custom Tab is launched. The value is defined as
     * {@link LaunchSourceType}.
     */
    public static final String EXTRA_BROWSER_LAUNCH_SOURCE =
            "org.chromium.chrome.browser.customtabs.EXTRA_BROWSER_LAUNCH_SOURCE";

    // Deprecated. Use CustomTabsIntent#EXTRA_TRANSLATE_LANGUAGE_TAG
    /**
     * Extra that, if set, specifies Translate UI should be triggered with
     * specified target language.
     */
    @Deprecated @VisibleForTesting
    static final String EXTRA_TRANSLATE_LANGUAGE =
            "androidx.browser.customtabs.extra.TRANSLATE_LANGUAGE";

    /**
     * Extra that, if set, specifies that the loaded page should be automatically translated once it
     * loads with the specified target language. This overrides EXTRA_TRANSLATE_LANGUAGE.
     */
    @VisibleForTesting
    static final String EXTRA_AUTO_TRANSLATE_LANGUAGE =
            "androidx.browser.customtabs.extra.AUTO_TRANSLATE_LANGUAGE";

    public static final String EXTRA_OPEN_IN_BROWSER_STATE =
            "androidx.browser.customtabs.extra.OPEN_IN_BROWSER_STATE";

    @VisibleForTesting
    static final String EXTRA_OPEN_IN_BROWSER_BUTTON_ALLOWED =
            "androidx.browser.customtabs.extra.OPEN_IN_BROWSER_BUTTON_ALLOWED";

    static final String EXTRA_CUSTOM_CONTENT_ACTIONS =
            "androidx.browser.customtabs.extra.CUSTOM_CONTENT_ACTIONS";

    @IntDef({
        CustomTabsButtonState.BUTTON_STATE_OFF,
        CustomTabsButtonState.BUTTON_STATE_ON,
        CustomTabsButtonState.BUTTON_STATE_DEFAULT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CustomTabsButtonState {
        int BUTTON_STATE_OFF = CustomTabsIntent.SHARE_STATE_OFF;
        int BUTTON_STATE_ON = CustomTabsIntent.SHARE_STATE_ON;
        int BUTTON_STATE_DEFAULT = CustomTabsIntent.SHARE_STATE_DEFAULT;
    }

    private static final String EXTRA_TWA_DISCLOSURE_UI =
            "androidx.browser.trusted.extra.DISCLOSURE_VERSION";

    private static final int DEFAULT_BREAKPOINT_DP = 840;

    private static final int MAX_CUSTOM_MENU_ITEMS = 7;

    private static final int MAX_CUSTOM_TOOLBAR_ITEMS = 2;

    private static final String FIRST_PARTY_PITFALL_MSG =
            "The intent contains a non-default UI type, but it is not from a first-party app. "
                    + "To make locally-built Chrome a first-party app, sign with release-test "
                    + "signing keys and run on userdebug devices. See use_signing_keys GN arg.";

    // Extra whose value is an array of ints that is supplied to
    // SyntheticTrialRegistry::RegisterExternalExperiments().
    public static final String EXPERIMENT_IDS =
            "org.chromium.chrome.browser.customtabs.AGA_EXPERIMENT_IDS";

    // These Extra Intent parameters allow an Intent to enable or disable a set of Features.
    // The set of Features that may be enabled or disabled is restricted by the code,
    // and initially only two Features may be enabled together, or disabled together.
    public static final String EXPERIMENTS_ENABLE =
            "org.chromium.chrome.browser.customtabs.EXPERIMENTS_ENABLE";
    public static final String EXPERIMENTS_DISABLE =
            "org.chromium.chrome.browser.customtabs.EXPERIMENTS_DISABLE";

    /**
     * Extra that, if set, makes the Custom Tab Activity's height to be x pixels, the Custom Tab
     * will behave as a bottom sheet. x will be clamped between 50% and 100% of screen height.
     * TODO(jinsukkim): Deprecate this.
     */
    public static final String EXTRA_INITIAL_ACTIVITY_HEIGHT_IN_PIXEL_LEGACY =
            "androidx.browser.customtabs.extra.INITIAL_ACTIVITY_HEIGHT_IN_PIXEL";

    /**
     * Extra that, if set, corresponds to the timestamp (in SystemClock.uptimeMillis()) when the TWA
     * launcher Activity was created. Used to measure the full startup duration.
     */
    public static final String EXTRA_TWA_STARTUP_UPTIME_MS =
            "org.chromium.chrome.browser.customtabs.trusted.STARTUP_UPTIME_MILLIS";

    /** Extra that, if set, corresponds to the integer version of android_browser_helper. */
    public static final String EXTRA_ANDROID_BROWSER_HELPER_VERSION =
            "org.chromium.chrome.browser.ANDROID_BROWSER_HELPER_VERSION";

    /**
     * Extra that, if set, allows you to interact with the background app when a PCCT is launched.
     * Note: Deprecated. Use {@link CustomTabsIntent#isBackgroundInteractionEnabled(Intent)}.
     */
    @Deprecated
    public static final String EXTRA_ENABLE_BACKGROUND_INTERACTION =
            "androidx.browser.customtabs.extra.ENABLE_BACKGROUND_INTERACTION";

    /**
     * Extra that, if set, makes the toolbar's top corner radii to be x pixels. This will only have
     * effect if the custom tab is behaving as a bottom sheet. Currently, this is capped at 16dp.
     * TODO(jinsukkim): Deprecate this.
     */
    public static final String EXTRA_TOOLBAR_CORNER_RADIUS_IN_PIXEL_LEGACY =
            "androidx.browser.customtabs.extra.TOOLBAR_CORNER_RADIUS_IN_PIXEL";

    private static final String DEFAULT_POLICY_USE_DENYLIST = "use-denylist";
    private static final String DEFAULT_POLICY_USE_ALLOWLIST = "use-allowlist";

    /**
     * Extra that specifies the {@link PendingIntent} to be sent when the user swipes up from the
     * secondary (bottom) toolbar.
     *
     * <p>Use {@link CustomTabsIntent.Builder#setSecondaryToolbarSwipeUpGesture(PendingIntent)} or
     * {@link CustomTabsIntent#EXTRA_SECONDARY_TOOLBAR_SWIPE_UP_GESTURE} as this is deprecated.
     */
    @Deprecated
    public static final String EXTRA_SECONDARY_TOOLBAR_SWIPE_UP_ACTION =
            "androidx.browser.customtabs.extra.SECONDARY_TOOLBAR_SWIPE_UP_ACTION";

    /**
     * Allow user gestures on content area to be used not only for scrolling contents but also for
     * resizing CCT. Used for Partial Custom Tab Bottom Sheet only.
     */
    public static final String EXTRA_ACTIVITY_SCROLL_CONTENT_RESIZE =
            "androidx.browser.customtabs.extra.ACTIVITY_SCROLL_CONTENT_RESIZE";

    @IntDef({
        OpenInBrowserButtonState.OPEN_IN_BROWSER_STATE_OFF,
        OpenInBrowserButtonState.OPEN_IN_BROWSER_STATE_ON,
        OpenInBrowserButtonState.OPEN_IN_BROWSER_STATE_DEFAULT
    })
    @Retention(RetentionPolicy.SOURCE)
    @ExperimentalOpenInBrowser
    public @interface OpenInBrowserButtonState {
        int OPEN_IN_BROWSER_STATE_OFF = CustomTabsIntent.OPEN_IN_BROWSER_STATE_OFF;
        int OPEN_IN_BROWSER_STATE_ON = CustomTabsIntent.OPEN_IN_BROWSER_STATE_ON;
        int OPEN_IN_BROWSER_STATE_DEFAULT = CustomTabsIntent.OPEN_IN_BROWSER_STATE_DEFAULT;
    }

    private final Intent mIntent;
    private final @Nullable SessionHolder<CustomTabsSessionToken> mSession;
    private final boolean mIsTrustedIntent;
    private final @Nullable Intent mKeepAliveServiceIntent;
    private final @Nullable Bundle mAnimationBundle;

    private int mUiType;
    private final int mTitleVisibilityState;
    private final @Nullable String mMediaViewerUrl;
    private final boolean mEnableEmbeddedMediaExperience;
    private final boolean mIsFromMediaLauncherActivity;
    private final boolean mDisableStar;
    private final boolean mDisableDownload;
    private final @ActivityType int mActivityType;
    private final @Nullable List<String> mTrustedWebActivityAdditionalOrigins;
    private @Nullable Set<Origin> mAllTrustedWebActivityOrigins;
    private final @Nullable TrustedWebActivityDisplayMode mTrustedWebActivityDisplayMode;
    private final List<TrustedWebActivityDisplayMode> mTrustedWebActivityDisplayOverrideMode;
    private @Nullable String mUrlToLoad;

    private final boolean mEnableUrlBarHiding;
    private boolean mInteractWithBackground;
    private List<CustomButtonParams> mCustomButtonParams;
    private @Nullable Drawable mCloseButtonIcon;
    private final boolean mIsCloseButtonEnabled;
    private final List<Pair<String, PendingIntent>> mMenuEntries = new ArrayList<>();
    private boolean mShowShareItemInMenu;
    private final List<CustomButtonParams> mToolbarButtons = new ArrayList<>(1);
    private final List<CustomButtonParams> mBottombarButtons = new ArrayList<>(2);
    private final List<CustomButtonParams> mGoogleBottomBarButtons = new ArrayList<>();
    private final @Nullable RemoteViews mRemoteViews;
    @ActivitySideSheetDecorationType private final int mSideSheetDecorationType;
    @ActivitySideSheetRoundedCornersPosition private final int mSideSheetRoundedCornersPosition;
    private final int @Nullable [] mClickableViewIds;
    private final @Nullable PendingIntent mRemoteViewsPendingIntent;
    private final @Nullable PendingIntent mSecondaryToolbarSwipeUpPendingIntent;
    private PendingIntent.@Nullable OnFinished mOnFinishedForTesting;
    private @DisplayMode.EnumType int mResolvedDisplayMode = DisplayMode.UNDEFINED;
    private final @OpenInBrowserState int mOpenInBrowserState;
    private final int mShareState;

    /** Whether this CustomTabActivity was explicitly started by another Chrome Activity. */
    private final boolean mIsOpenedByChrome;

    /** ISO 639 language code */
    private final @Nullable String mTranslateLanguage;

    /** ISO 639 language code, overrides {@link mTranslateLanguage} if non-null. */
    private final @Nullable String mAutoTranslateLanguage;

    private final int mDefaultOrientation;

    private final int @Nullable [] mGsaExperimentIds;

    private final ColorProvider mColorProvider;

    private final int mBreakPointDp;
    private final @Px int mInitialActivityHeight;
    private final @Px int mInitialActivityWidth;
    private final @Px int mPartialTabToolbarCornerRadius;

    private final boolean mIsPartialCustomTabFixedHeight;
    private final boolean mContentScrollMayResizeTab;

    /**
     * {@link Network} to be bound when launching a custom tab or tabs that have been pre-created.
     */
    private final @Nullable Network mNetwork;

    /** Add extras to customize menu items for opening Reader Mode UI custom tab from Chrome. */
    public static void addReaderModeUiExtras(Intent intent) {
        intent.putExtra(EXTRA_UI_TYPE, CustomTabsUiType.READER_MODE);
        IntentUtils.addTrustedIntentExtras(intent);
    }

    /**
     * Evaluates whether the passed Intent and/or CustomTabsSessionToken are from a trusted source.
     * Trusted in this case means from the app itself or via a first-party application.
     *
     * @param intent The Intent used to start the custom tabs activity, or null.
     * @param session The connected session for the custom tabs activity, or null.
     * @return True if the intent or session are trusted.
     */
    public static boolean isTrustedCustomTab(Intent intent, @Nullable SessionHolder<?> session) {
        if (IntentHandler.wasIntentSenderChrome(intent)) return true;
        String packageName = getClientPackageNameFromSessionOrCallingActivity(intent, session);
        return CustomTabsConnection.getInstance().isFirstParty(packageName);
    }

    static @Nullable String getClientPackageNameFromSessionOrCallingActivity(
            Intent intent, @Nullable SessionHolder<?> session) {
        String packageNameFromSession =
                CustomTabsConnection.getInstance().getClientPackageNameForSession(session);
        if (!TextUtils.isEmpty(packageNameFromSession)) return packageNameFromSession;

        String packageNameFromIntent =
                IntentUtils.safeGetStringExtra(
                        intent, IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE);
        if (!TextUtils.isEmpty(packageNameFromIntent)) return packageNameFromIntent;

        return null;
    }

    public static void configureIntentForResizableCustomTab(Context context, Intent intent) {
        SessionHolder<?> session = SessionHolder.getSessionHolderFromIntent(intent);
        boolean isTrustedCustomTab = isTrustedCustomTab(intent, session);
        String packageName = getClientPackageNameFromSessionOrCallingActivity(intent, session);
        @Px
        int initialActivityHeight =
                getInitialActivityHeight(
                        isTrustedCustomTab,
                        getInitialActivityHeightFromIntent(intent),
                        packageName);
        @Px
        int initialActivityWidth =
                getInitialActivityWidth(
                        isTrustedCustomTab, getInitialActivityWidthFromIntent(intent), packageName);
        if (initialActivityHeight <= 0 && initialActivityWidth <= 0) {
            // fallback to normal Custom Tab.
            return;
        }
        intent.setClassName(context, TranslucentCustomTabActivity.class.getName());
        // When scrolling up the web content, we don't want to hide the URL bar.
        intent.putExtra(CustomTabsIntent.EXTRA_ENABLE_URLBAR_HIDING, false);
    }

    private static @Px int getInitialActivityHeight(
            boolean isTrustedIntent, @Px int initialActivityHeight, @Nullable String packageName) {
        boolean enabledDueToThirdParty =
                ChromeFeatureList.sCctResizableForThirdParties.isEnabled()
                        && isAllowedThirdParty(packageName);
        return (isTrustedIntent || enabledDueToThirdParty) ? initialActivityHeight : 0;
    }

    private static @Px int getInitialActivityWidth(
            boolean isTrustedIntent, @Px int initialActivityWidth, @Nullable String packageName) {
        boolean enabledDueToThirdParty = isAllowedThirdParty(packageName);
        return (isTrustedIntent || enabledDueToThirdParty) ? initialActivityWidth : 0;
    }

    /** Returns the initial activity height in px. */
    private static int getInitialActivityHeightFromIntent(Intent intent) {
        int heightPx1 =
                IntentUtils.safeGetIntExtra(
                        intent,
                        CustomTabIntentDataProvider.EXTRA_INITIAL_ACTIVITY_HEIGHT_IN_PIXEL_LEGACY,
                        0);
        if (heightPx1 > 0) return heightPx1;
        int heightPx2 = IntentUtils.safeGetIntExtra(intent, EXTRA_INITIAL_ACTIVITY_HEIGHT_PX, 0);
        return heightPx2 > 0 ? heightPx2 : 0;
    }

    private static int getInitialActivityWidthFromIntent(Intent intent) {
        int widthPx = IntentUtils.safeGetIntExtra(intent, EXTRA_INITIAL_ACTIVITY_WIDTH_PX, 0);
        return widthPx > 0 ? widthPx : 0;
    }

    private static int getActivityBreakPointFromIntent(Intent intent) {
        int breakPointDp =
                IntentUtils.safeGetIntExtra(
                        intent, EXTRA_ACTIVITY_SIDE_SHEET_BREAKPOINT_DP, DEFAULT_BREAKPOINT_DP);
        return breakPointDp < 0 ? DEFAULT_BREAKPOINT_DP : breakPointDp;
    }

    private static int getActivitySideSheetDecorationTypeFromIntent(Intent intent) {
        int decorationType = CustomTabsIntent.getActivitySideSheetDecorationType(intent);
        return decorationType == ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DEFAULT
                        || decorationType < 0
                        || decorationType > ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER
                ? ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW
                : decorationType;
    }

    private static int getActivitySideSheetRoundedCornersPositionFromIntent(Intent intent) {
        int roundedCornersPosition =
                CustomTabsIntent.getActivitySideSheetRoundedCornersPosition(intent);
        return roundedCornersPosition == ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_DEFAULT
                        || roundedCornersPosition < 0
                        || roundedCornersPosition > ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_TOP
                ? ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE
                : roundedCornersPosition;
    }

    private static boolean getIsCloseButtonEnabled(Intent intent, int uiType) {
        return IntentUtils.safeGetBooleanExtra(intent, EXTRA_CLOSE_BUTTON_ENABLED, true)
                && uiType != CustomTabsUiType.POPUP;
    }

    /**
     * Extracts the name that identifies the embedding app from the referrer.
     *
     * @return Host name as an id if the referrer is of a well-formed URI with app intent scheme. If
     *     not, just the whole referrer string. TODO(crbug.com/40234088): Move this to
     *     IntentHandler.
     */
    static String getAppIdFromReferrer(Activity activity) {
        String referrer =
                assumeNonNull(CustomTabActivityLifecycleUmaTracker.getReferrerUriString(activity))
                        .toLowerCase(Locale.US);
        if (TextUtils.isEmpty(referrer)) return "";

        Uri uri = Uri.parse(referrer);
        boolean isUrl = TextUtils.equals(UrlConstants.APP_INTENT_SCHEME, uri.getScheme());
        if (isUrl) {
            String host = uri.getHost();
            if (!TextUtils.isEmpty(host)) return host;
        }
        return referrer;
    }

    /**
     * Constructs a {@link CustomTabIntentDataProvider}.
     *
     * @param intent The intent to launch the CCT.
     * @param colorScheme The colorScheme parameter specifies which color scheme the Custom Tab
     * should use.
     * It can currently be either {@link CustomTabsIntent#COLOR_SCHEME_LIGHT} or
     * {@link CustomTabsIntent#COLOR_SCHEME_DARK}.
     * If Custom Tab was launched with {@link CustomTabsIntent#COLOR_SCHEME_SYSTEM}, colorScheme
     * must reflect the current system setting. When the system setting changes, a new
     * CustomTabIntentDataProvider object must be created.
     */
    public CustomTabIntentDataProvider(Intent intent, Context context, int colorScheme) {
        if (intent == null) assert false;
        mIntent = intent;

        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        mSession = token != null ? new SessionHolder<>(token) : null;
        mIsTrustedIntent = isTrustedCustomTab(intent, mSession);

        mAnimationBundle =
                IntentUtils.safeGetBundleExtra(
                        intent, CustomTabsIntent.EXTRA_EXIT_ANIMATION_BUNDLE);

        mKeepAliveServiceIntent = IntentUtils.safeGetParcelableExtra(intent, EXTRA_KEEP_ALIVE);

        mNetwork = IntentUtils.safeGetParcelableExtra(intent, EXTRA_NETWORK);

        mIsOpenedByChrome = IntentHandler.wasIntentSenderChrome(intent);

        final int requestedUiType =
                IntentUtils.safeGetIntExtra(intent, EXTRA_UI_TYPE, CustomTabsUiType.DEFAULT);
        mUiType = getCustomTabsUiType(requestedUiType);

        mColorProvider = new CustomTabColorProviderImpl(intent, context, colorScheme);

        retrieveCustomButtons(intent, context);

        mEnableUrlBarHiding =
                IntentUtils.safeGetBooleanExtra(
                        intent, CustomTabsIntent.EXTRA_ENABLE_URLBAR_HIDING, true);
        mContentScrollMayResizeTab =
                IntentUtils.safeGetBooleanExtra(
                        intent, EXTRA_ACTIVITY_SCROLL_CONTENT_RESIZE, false);

        mIsCloseButtonEnabled = getIsCloseButtonEnabled(intent, mUiType);
        if (mIsCloseButtonEnabled) {
            // TODO(crbug.com/393437143): Potentially reuse the close button code from Auth Tab.
            Bitmap bitmap =
                    IntentUtils.safeGetParcelableExtra(
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
                mCloseButtonIcon = new TintedDrawable(context, bitmap);
            }
        }

        if (mUiType == CustomTabsUiType.POPUP) {
            mShareState = CustomTabsIntent.SHARE_STATE_OFF;
        } else {
            boolean usingInteractiveOmnibox =
                    CustomTabsConnection.getInstance().shouldEnableOmniboxForIntent(this);
            int defState =
                    usingInteractiveOmnibox
                            ? CustomTabsIntent.SHARE_STATE_OFF
                            : CustomTabsIntent.SHARE_STATE_DEFAULT;
            mShareState =
                    IntentUtils.safeGetIntExtra(
                            intent, CustomTabsIntent.EXTRA_SHARE_STATE, defState);
        }

        List<Bundle> menuItems =
                IntentUtils.getParcelableArrayListExtra(intent, CustomTabsIntent.EXTRA_MENU_ITEMS);

        int openInBrowserState =
                IntentUtils.safeGetIntExtra(
                        intent,
                        EXTRA_OPEN_IN_BROWSER_STATE,
                        OpenInBrowserButtonState.OPEN_IN_BROWSER_STATE_DEFAULT);
        if (isOpenInBrowserDisallowed(
                getUiType(), getCustomTabMode() == CustomTabProfileType.INCOGNITO)) {
            openInBrowserState = CustomTabsButtonState.BUTTON_STATE_OFF;
        }
        mOpenInBrowserState = openInBrowserState;

        int oibState = adjustOpenInBrowserOption(intent);
        maybeAddOpenInBrowserOption(context, oibState);
        updateExtraMenuItems(menuItems);
        maybeAddShareOption(intent, context);

        boolean isTwa =
                mSession != null
                        && IntentUtils.safeGetBooleanExtra(
                                intent,
                                TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY,
                                false);

        mActivityType =
                isTwa
                        ? ActivityType.TRUSTED_WEB_ACTIVITY
                        : isAuthTab() ? ActivityType.AUTH_TAB : ActivityType.CUSTOM_TAB;
        mTrustedWebActivityAdditionalOrigins =
                IntentUtils.safeGetStringArrayListExtra(
                        intent, TrustedWebActivityIntentBuilder.EXTRA_ADDITIONAL_TRUSTED_ORIGINS);

        // Do not fill in `mAllTrustedWebActivityOrigins` yet, because we cannot `getUrlToLoad()`
        // until native is loaded.

        mTrustedWebActivityDisplayMode = resolveTwaDisplayMode();
        mTrustedWebActivityDisplayOverrideMode = resolveTwaDisplayOverrideMode();

        // After TWA checks, update custom tabs ui types. Order seems to matter
        // here.
        mUiType = getCustomTabsUiType(requestedUiType);
        int intentVisibilityState =
                IntentUtils.safeGetIntExtra(
                        intent,
                        CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE,
                        CustomTabsIntent.NO_TITLE);
        mTitleVisibilityState =
                BrowserServicesIntentDataProvider.customTabIntentTitleBarVisibility(
                        intentVisibilityState, isTwa);

        mRemoteViews =
                IntentUtils.safeGetParcelableExtra(intent, CustomTabsIntent.EXTRA_REMOTEVIEWS);
        mClickableViewIds =
                IntentUtils.safeGetIntArrayExtra(
                        intent, CustomTabsIntent.EXTRA_REMOTEVIEWS_VIEW_IDS);
        mRemoteViewsPendingIntent =
                IntentUtils.safeGetParcelableExtra(
                        intent, CustomTabsIntent.EXTRA_REMOTEVIEWS_PENDINGINTENT);
        mSecondaryToolbarSwipeUpPendingIntent = getSecondaryToolbarSwipeUpGesture(intent);
        mMediaViewerUrl =
                isMediaViewer()
                        ? IntentUtils.safeGetStringExtra(intent, EXTRA_MEDIA_VIEWER_URL)
                        : null;
        mEnableEmbeddedMediaExperience =
                isTrustedIntent()
                        && IntentUtils.safeGetBooleanExtra(
                                intent, EXTRA_ENABLE_EMBEDDED_MEDIA_EXPERIENCE, false);
        mIsFromMediaLauncherActivity =
                isTrustedIntent()
                        && (IntentUtils.safeGetIntExtra(
                                        intent, EXTRA_BROWSER_LAUNCH_SOURCE, LaunchSourceType.OTHER)
                                == LaunchSourceType.MEDIA_LAUNCHER_ACTIVITY);
        mDisableStar = !CustomTabsIntent.isBookmarksButtonEnabled(intent);
        mDisableDownload = !CustomTabsIntent.isDownloadButtonEnabled(intent);

        mTranslateLanguage = getTranslateLanguage(intent);

        mAutoTranslateLanguage =
                IntentUtils.safeGetStringExtra(intent, EXTRA_AUTO_TRANSLATE_LANGUAGE);

        // Import the {@link ScreenOrientation}.
        mDefaultOrientation =
                convertOrientationType(
                        IntentUtils.safeGetIntExtra(
                                intent,
                                TrustedWebActivityIntentBuilder.EXTRA_SCREEN_ORIENTATION,
                                ScreenOrientation.DEFAULT));

        mGsaExperimentIds = IntentUtils.safeGetIntArrayExtra(intent, EXPERIMENT_IDS);

        mBreakPointDp = getActivityBreakPointFromIntent(intent);
        mInitialActivityHeight = getInitialActivityHeightFromIntent(intent);
        mInitialActivityWidth = getInitialActivityWidthFromIntent(intent);
        mPartialTabToolbarCornerRadius = getToolbarCornerRadiusFromIntent(context, intent);
        // The default behavior is that the PCCT's height is resizable.
        @ActivityHeightResizeBehavior
        int activityHeightResizeBehavior =
                IntentUtils.safeGetIntExtra(
                        intent, EXTRA_ACTIVITY_HEIGHT_RESIZE_BEHAVIOR, ACTIVITY_HEIGHT_DEFAULT);
        mIsPartialCustomTabFixedHeight = activityHeightResizeBehavior == ACTIVITY_HEIGHT_FIXED;

        mInteractWithBackground = CustomTabsIntent.isBackgroundInteractionEnabled(intent);
        if (IntentUtils.safeHasExtra(intent, EXTRA_ENABLE_BACKGROUND_INTERACTION)) {
            @BackgroundInteractBehavior
            int backgroundInteractBehavior =
                    IntentUtils.safeGetIntExtra(
                            intent,
                            EXTRA_ENABLE_BACKGROUND_INTERACTION,
                            BackgroundInteractBehavior.DEFAULT);
            mInteractWithBackground = backgroundInteractBehavior != BackgroundInteractBehavior.OFF;
        }
        mSideSheetDecorationType = getActivitySideSheetDecorationTypeFromIntent(intent);
        mSideSheetRoundedCornersPosition =
                getActivitySideSheetRoundedCornersPositionFromIntent(intent);

        logCustomTabFeatures(intent, colorScheme);
        String packageName = getClientPackageNameFromSessionOrCallingActivity(mIntent, mSession);
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.HasNonSpoofablePackageName", !TextUtils.isEmpty(packageName));
    }

    /** Returns the toolbar corner radius in px. */
    private static int getToolbarCornerRadiusFromIntent(Context context, Intent intent) {
        int defaultRadius =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.custom_tabs_default_corner_radius);
        int radiusPx =
                IntentUtils.safeGetIntExtra(
                        intent, EXTRA_TOOLBAR_CORNER_RADIUS_IN_PIXEL_LEGACY, -1);
        if (radiusPx >= 0) return radiusPx;

        int radiusDp = IntentUtils.safeGetIntExtra(intent, EXTRA_TOOLBAR_CORNER_RADIUS_DP, -1);
        if (radiusDp >= 0) {
            return Math.round(radiusDp * context.getResources().getDisplayMetrics().density);
        }

        return defaultRadius;
    }

    private static @Nullable PendingIntent getSecondaryToolbarSwipeUpGesture(Intent intent) {
        PendingIntent pendingIntent = CustomTabsIntent.getSecondaryToolbarSwipeUpGesture(intent);
        if (pendingIntent == null) {
            pendingIntent =
                    IntentUtils.safeGetParcelableExtra(
                            intent,
                            CustomTabIntentDataProvider.EXTRA_SECONDARY_TOOLBAR_SWIPE_UP_ACTION);
        }
        return pendingIntent;
    }

    private static @Nullable String getTranslateLanguage(Intent intent) {
        String translateLanguage = null;
        Locale locale = CustomTabsIntent.getTranslateLocale(intent);
        if (locale != null) translateLanguage = LocaleUtils.toLanguageTag(locale);
        if (TextUtils.isEmpty(translateLanguage)) {
            translateLanguage = IntentUtils.safeGetStringExtra(intent, EXTRA_TRANSLATE_LANGUAGE);
        }
        return translateLanguage;
    }

    private void updateExtraMenuItems(@Nullable List<Bundle> menuItems) {
        if (menuItems == null) return;
        for (int i = 0; i < Math.min(MAX_CUSTOM_MENU_ITEMS, menuItems.size()); i++) {
            Bundle bundle = menuItems.get(i);
            String title = IntentUtils.safeGetString(bundle, CustomTabsIntent.KEY_MENU_ITEM_TITLE);
            PendingIntent pendingIntent =
                    IntentUtils.safeGetParcelable(bundle, CustomTabsIntent.KEY_PENDING_INTENT);
            if (TextUtils.isEmpty(title) || pendingIntent == null) {
                continue;
            }
            mMenuEntries.add(new Pair<>(title, pendingIntent));
        }
    }

    /**
     * Triggers the client-defined action when the user clicks a custom menu item.
     *
     * @param activity The {@link Activity} to use for sending the {@link PendingIntent}.
     * @param menuIndex The index that the menu item is shown in the result of {@link
     *     #getMenuTitles()}.
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
            ActivityOptions options = ActivityOptions.makeBasic();
            ApiCompatibilityUtils.setActivityOptionsBackgroundActivityStartAllowAlways(options);
            pendingIntent.send(
                    activity,
                    0,
                    isMediaViewer() ? null : addedIntent,
                    mOnFinishedForTesting,
                    null,
                    null,
                    options.toBundle());
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
     * Get the verified custom tabs UI type, according to the intent extras, and whether the intent
     * is trusted.
     *
     * <p>If the intent extras include a valid EXTRA_NETWORK, consider that the custom tab is used
     * for captive portal scenarios especially and the UI hides the "Open in Chrome browser" menu
     * item accordingly.
     *
     * @param requestedUiType requested UI type in the intent, unqualified
     * @return verified UI type
     */
    @BrowserServicesIntentDataProvider.CustomTabsUiType
    private int getCustomTabsUiType(int requestedUiType) {
        if (mNetwork != null) return CustomTabsUiType.NETWORK_BOUND_TAB;
        if (isTrustedWebActivity() && resolveDisplayMode() == DisplayMode.MINIMAL_UI) {
            return CustomTabsUiType.TRUSTED_WEB_ACTIVITY;
        }
        if (!isTrustedIntent()) {
            if (VersionInfo.isLocalBuild()) Log.w(TAG, FIRST_PARTY_PITFALL_MSG);
            return CustomTabsUiType.DEFAULT;
        }

        return requestedUiType;
    }

    /**
     * Gets custom buttons from the intent and updates {@link #mCustomButtonParams}, {@link
     * #mBottombarButtons} and {@link #mToolbarButtons}.
     */
    private void retrieveCustomButtons(Intent intent, Context context) {
        assert mCustomButtonParams == null;
        mCustomButtonParams = CustomButtonParamsImpl.fromIntent(context, intent);
        Set<Integer> googleBottomBarSupportedCustomButtonParamIds =
                getGoogleBottomBarSupportedCustomButtonParamIds();
        for (CustomButtonParams params : mCustomButtonParams) {
            if (googleBottomBarSupportedCustomButtonParamIds.contains(params.getId())) {
                mGoogleBottomBarButtons.add(params);
                params.updateShowOnToolbar(false);
            } else if (!params.showOnToolbar()) {
                mBottombarButtons.add(params);
            } else if (canAddMoreToolbarItems()) {
                mToolbarButtons.add(params);
            } else {
                Log.w(TAG, "Only %d items are allowed in the toolbar", getMaxCustomToolbarItems());
            }
        }

        for (CustomButtonParams params :
                getAdditionalSupportedGoogleBottomBarCustomButtonParams(context)) {
            params.updateShowOnToolbar(false);
            mCustomButtonParams.add(params);
            mGoogleBottomBarButtons.add(params);
        }
    }

    private boolean canAddMoreToolbarItems() {
        return mToolbarButtons.size() < getMaxCustomToolbarItems();
    }

    /**
     * Determines which buttons should be displayed in the Google Bottom Bar.
     *
     * @return A set of integers representing the customButtonParamIds of the buttons that should be
     *     displayed in the Google Bottom Bar. If the Google Bottom Bar is not enabled, an empty set
     *     is returned.
     */
    private Set<Integer> getGoogleBottomBarSupportedCustomButtonParamIds() {
        if (!isGoogleBottomBarEnabled(this)) {
            return Set.of();
        }
        GoogleBottomBarIntentParams intentParams =
                CustomTabsConnection.getInstance().getGoogleBottomBarIntentParams(this);
        return GoogleBottomBarCoordinator.getSupportedCustomButtonParamIds(intentParams);
    }

    /**
     * Retrieves a list of additional CustomButtonParams that are only supported for the Google
     * Bottom Bar and should never be shown on the CCT toolbar.
     *
     * @param context Current context.
     * @return A list of {@link CustomButtonParams}. If the Google Bottom Bar is not enabled, or no
     *     supported buttons are found, an empty list is returned.
     */
    private List<CustomButtonParams> getAdditionalSupportedGoogleBottomBarCustomButtonParams(
            Context context) {
        List<CustomButtonParams> supportedGoogleBottomBarCustomButtonParams = new ArrayList<>();
        if (!isGoogleBottomBarEnabled(this)) {
            return supportedGoogleBottomBarCustomButtonParams;
        }
        List<Bundle> googleBottomBarButtonBundles =
                CustomTabsConnection.getInstance().getGoogleBottomBarButtons(this);
        List<CustomButtonParams> googleBottomBarButtons =
                CustomButtonParamsImpl.fromBundleList(
                        context, googleBottomBarButtonBundles, /* tinted= */ false);
        for (CustomButtonParams params : googleBottomBarButtons) {
            if (GoogleBottomBarCoordinator.isSupported(params.getId())) {
                supportedGoogleBottomBarCustomButtonParams.add(params);
            } else {
                Log.w(TAG, "Unused GoogleBottomBarButton id: %s", params.getId());
            }
        }
        return supportedGoogleBottomBarCustomButtonParams;
    }

    private static boolean isGoogleBottomBarEnabled(BrowserServicesIntentDataProvider provider) {
        return GoogleBottomBarCoordinator.isFeatureEnabled()
                && CustomTabsConnection.getInstance()
                        .shouldEnableGoogleBottomBarForIntent(provider);
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
     *   <li>If {@link CustomTabsIntent#SHARE_STATE_ON} or {@link
     *       CustomTabsIntent#SHARE_STATE_DEFAULT}, add to the top toolbar if empty, otherwise add
     *       to the overflow menu if it is not customized.
     *   <li>If {@link CustomTabsIntent#SHARE_STATE_OFF}, add to the overflow menu depending on
     *       {@link CustomTabsIntent#EXTRA_DEFAULT_SHARE_MENU_ITEM}.
     * </ul>
     */
    private void maybeAddShareOption(Intent intent, Context context) {
        // Disable CCT share options for automotive. See crbug.com/300292495.
        if (!ShareUtils.enableShareForAutomotive(true)) return;

        if (mShareState == CustomTabsIntent.SHARE_STATE_DEFAULT) {
            if (canAddShareAction()) {
                mToolbarButtons.add(
                        CustomButtonParamsImpl.createShareButton(
                                context, getColorProvider().getToolbarColor()));
            } else if (mMenuEntries.isEmpty()) {
                mShowShareItemInMenu = true;
            }
        } else if (mShareState == CustomTabsIntent.SHARE_STATE_ON) {
            if (canAddShareAction()) {
                mToolbarButtons.add(
                        CustomButtonParamsImpl.createShareButton(
                                context, getColorProvider().getToolbarColor()));
            } else {
                mShowShareItemInMenu = true;
            }
        } else {
            mShowShareItemInMenu =
                    IntentUtils.safeGetBooleanExtra(
                            intent,
                            CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM,
                            mIsOpenedByChrome && mUiType == CustomTabsUiType.DEFAULT);
        }
    }

    private boolean canAddShareAction() {
        if (!ChromeFeatureList.sCctAdaptiveButton.isEnabled()) return mToolbarButtons.isEmpty();

        if (!canAddMoreToolbarItems()) return false;

        if (mShareState == CustomTabsIntent.SHARE_STATE_OFF) {
            return false;
        } else if (mShareState == CustomTabsIntent.SHARE_STATE_ON) {
            return true;
        } else { // mShareState == CustomTabsIntent.SHARE_STATE_DEFAULT
            return mToolbarButtons.isEmpty();
        }
    }

    private int adjustOpenInBrowserOption(Intent intent) {
        boolean usingInteractiveOmnibox =
                CustomTabsConnection.getInstance().shouldEnableOmniboxForIntent(this);

        int openInBrowserState = mOpenInBrowserState;

        if (openInBrowserState == CustomTabsButtonState.BUTTON_STATE_ON
                && !ChromeFeatureList.sCctOpenInBrowserButtonIfEnabledByEmbedder.isEnabled()) {
            openInBrowserState = CustomTabsButtonState.BUTTON_STATE_DEFAULT;
        }

        if (openInBrowserState == CustomTabsButtonState.BUTTON_STATE_DEFAULT) {
            if (usingInteractiveOmnibox) {
                openInBrowserState = CustomTabsButtonState.BUTTON_STATE_ON;
            } else if (ChromeFeatureList.sCctOpenInBrowserButtonIfAllowedByEmbedder.isEnabled()
                    && IntentUtils.safeGetBooleanExtra(
                            intent, EXTRA_OPEN_IN_BROWSER_BUTTON_ALLOWED, false)) {
                openInBrowserState = CustomTabsButtonState.BUTTON_STATE_ON;
            } else if (!isCpaOnlyOpenInBrowserDefault()) {
                openInBrowserState = CustomTabsButtonState.BUTTON_STATE_OFF;
            }
        }

        return openInBrowserState;
    }

    private void maybeAddOpenInBrowserOption(Context context, int oibState) {
        if (canAddOpenInBrowserAction(oibState)) {
            mToolbarButtons.add(
                    CustomButtonParamsImpl.createOpenInBrowserButton(
                            context, getColorProvider().getToolbarColor()));
        }
    }

    private boolean canAddOpenInBrowserAction(int oibState) {
        if (!ChromeFeatureList.sCctAdaptiveButton.isEnabled()
                && oibState == CustomTabsButtonState.BUTTON_STATE_ON) {
            return mToolbarButtons.isEmpty();
        }

        if (!canAddMoreToolbarItems()) return false;

        if (oibState == CustomTabsButtonState.BUTTON_STATE_OFF) {
            return false;
        } else if (oibState == CustomTabsButtonState.BUTTON_STATE_ON) {
            return mToolbarButtons.isEmpty() || mShareState != CustomTabsIntent.SHARE_STATE_ON;
        } else { // oibState == CustomTabsButtonState.BUTTON_STATE_DEFAULT
            // Give SHARE a higher precedence than OIB. OIB is visible only in CPA+OIB
            // experiment arm where SHARE is explicitly off.
            return mToolbarButtons.isEmpty()
                    && isCpaOnlyOpenInBrowserDefault()
                    && mShareState == CustomTabsIntent.SHARE_STATE_OFF;
        }
    }

    /**
     * Returns {@code true} if open-in-browser (action button/menu item) should not be presented on
     * the UI surface.
     *
     * @param type {@link CustomTabsUiType} value.
     * @param incognito Whether the {@link CustomTabProfileType} is incongnito.
     */
    public static boolean isOpenInBrowserDisallowed(int type, boolean incognito) {
        return !isOpenInBrowserAllowedForType(type)
                || incognito
                || !FirstRunStatus.getFirstRunFlowComplete();
    }

    private static boolean isOpenInBrowserAllowedForType(int type) {
        switch (type) {
            case CustomTabsUiType.MEDIA_VIEWER:
            case CustomTabsUiType.READER_MODE:
            case CustomTabsUiType.MINIMAL_UI_WEBAPP:
            case CustomTabsUiType.OFFLINE_PAGE:
            case CustomTabsUiType.AUTH_TAB:
            case CustomTabsUiType.NETWORK_BOUND_TAB:
            case CustomTabsUiType.POPUP:
                return false;
            case CustomTabsUiType.TRUSTED_WEB_ACTIVITY:
                return !ChromeFeatureList.sAndroidWebAppMenuButton.isEnabled();
            case CustomTabsUiType.DEFAULT:
            default:
                return true;
        }
    }

    private @Nullable String resolveUrlToLoad(Intent intent) {
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

    private @Nullable TrustedWebActivityDisplayMode resolveTwaDisplayMode() {
        Bundle bundle =
                IntentUtils.safeGetBundleExtra(
                        mIntent, TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_MODE);
        if (bundle == null) {
            return null;
        }
        try {
            return TrustedWebActivityDisplayMode.fromBundle(bundle);
        } catch (Throwable e) {
            return null;
        }
    }

    private List<TrustedWebActivityDisplayMode> resolveTwaDisplayOverrideMode() {
        // display-override, if present and containing a supported mode, takes precedence over
        // display-mode.
        List<Bundle> displayOverrideBundle =
                IntentUtils.getParcelableArrayListExtra(
                        mIntent, TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_OVERRIDE);
        List<TrustedWebActivityDisplayMode> displayOverride = new ArrayList<>();
        if (displayOverrideBundle != null) {
            for (Bundle b : displayOverrideBundle) {
                try {
                    displayOverride.add(TrustedWebActivityDisplayMode.fromBundle(b));
                } catch (Throwable e) {
                    // If the given value isn't a valid display mode, skip it.
                }
            }
        }
        return displayOverride;
    }

    /**
     * Returns the {@link ScreenOrientationLockType} which matches {@link ScreenOrientation}.
     *
     * @param orientation {@link ScreenOrientation}
     * @return The matching ScreenOrientationLockType. {@link ScreenOrientationLockType#DEFAULT} if
     *     there is no match.
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
                Log.w(
                        TAG,
                        "The provided orientaton is not supported, orientation = %d",
                        orientation);
                return ScreenOrientationLockType.DEFAULT;
        }
    }

    /**
     * Logs the usage of intents of all CCT features to a large enum histogram in order to track
     * usage by apps.
     *
     * @param intent The intent used to launch the CCT.
     * @param colorScheme The requested color scheme to use with the CCT.
     */
    private void logCustomTabFeatures(Intent intent, int colorScheme) {
        CustomTabsFeatureUsage featureUsage = new CustomTabsFeatureUsage();

        // Ordering: Log all the features ordered by CustomTabsFeature enum, when they apply.
        if (mAnimationBundle != null) {
            featureUsage.log(CustomTabsFeature.EXTRA_EXIT_ANIMATION_BUNDLE);
        }
        if (IntentUtils.safeHasExtra(intent, CustomTabsIntent.EXTRA_TINT_ACTION_BUTTON)) {
            featureUsage.log(CustomTabsFeature.EXTRA_TINT_ACTION_BUTTON);
        }
        if (IntentUtils.safeHasExtra(intent, EXTRA_INITIAL_BACKGROUND_COLOR)) {
            featureUsage.log(CustomTabsFeature.EXTRA_INITIAL_BACKGROUND_COLOR);
        }
        if (mInteractWithBackground) {
            featureUsage.log(CustomTabsFeature.EXTRA_ENABLE_BACKGROUND_INTERACTION);
        }
        if (IntentUtils.safeHasExtra(intent, CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON)) {
            featureUsage.log(CustomTabsFeature.EXTRA_CLOSE_BUTTON_ICON);
        }
        if (getCloseButtonPosition() != CLOSE_BUTTON_POSITION_DEFAULT) {
            featureUsage.log(CustomTabsFeature.EXTRA_CLOSE_BUTTON_POSITION);
        }
        if (colorScheme == CustomTabsIntent.COLOR_SCHEME_DARK) {
            featureUsage.log(CustomTabsFeature.CTF_DARK);
        }
        if (colorScheme == CustomTabsIntent.COLOR_SCHEME_LIGHT) {
            featureUsage.log(CustomTabsFeature.CTF_LIGHT);
        }
        if (IntentUtils.safeHasExtra(intent, CustomTabsIntent.EXTRA_COLOR_SCHEME)) {
            featureUsage.log(CustomTabsFeature.EXTRA_COLOR_SCHEME);
        }
        if (colorScheme == CustomTabsIntent.COLOR_SCHEME_SYSTEM) {
            featureUsage.log(CustomTabsFeature.CTF_SYSTEM);
        }
        if (mDisableDownload) featureUsage.log(CustomTabsFeature.EXTRA_DISABLE_DOWNLOAD_BUTTON);
        if (mDisableStar) featureUsage.log(CustomTabsFeature.EXTRA_DISABLE_STAR_BUTTON);
        if (mGsaExperimentIds != null) featureUsage.log(CustomTabsFeature.EXPERIMENT_IDS);
        if (IntentUtils.safeHasExtra(
                        intent,
                        CustomTabIntentDataProvider.EXTRA_INITIAL_ACTIVITY_HEIGHT_IN_PIXEL_LEGACY)
                || IntentUtils.safeHasExtra(intent, EXTRA_INITIAL_ACTIVITY_HEIGHT_PX)) {
            featureUsage.log(CustomTabsFeature.EXTRA_INITIAL_ACTIVITY_HEIGHT_PX);
        }
        if (IntentUtils.safeHasExtra(intent, EXTRA_INITIAL_ACTIVITY_WIDTH_PX)) {
            featureUsage.log(CustomTabsFeature.EXTRA_INITIAL_ACTIVITY_WIDTH_PX);
        }
        if (IntentUtils.safeHasExtra(intent, EXTRA_ACTIVITY_SIDE_SHEET_BREAKPOINT_DP)) {
            featureUsage.log(CustomTabsFeature.EXTRA_ACTIVITY_SIDE_SHEET_BREAKPOINT_DP);
        }
        if (IntentUtils.safeHasExtra(intent, EXTRA_ACTIVITY_SIDE_SHEET_DECORATION_TYPE)) {
            featureUsage.log(CustomTabsFeature.EXTRA_ACTIVITY_SIDE_SHEET_DECORATION_TYPE);
        }
        if (IntentUtils.safeHasExtra(intent, EXTRA_ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION)) {
            featureUsage.log(CustomTabsFeature.EXTRA_ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION);
        }
        if (mEnableEmbeddedMediaExperience) {
            featureUsage.log(CustomTabsFeature.EXTRA_ENABLE_EMBEDDED_MEDIA_EXPERIENCE);
        }
        if (mIsFromMediaLauncherActivity) {
            featureUsage.log(CustomTabsFeature.EXTRA_BROWSER_LAUNCH_SOURCE);
        }
        if (mMediaViewerUrl != null) featureUsage.log(CustomTabsFeature.EXTRA_MEDIA_VIEWER_URL);
        if (mMenuEntries != null) featureUsage.log(CustomTabsFeature.EXTRA_MENU_ITEMS);
        if (IntentUtils.safeHasExtra(intent, IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE)) {
            featureUsage.log(CustomTabsFeature.EXTRA_CALLING_ACTIVITY_PACKAGE);
        }
        if (getClientPackageName() != null) featureUsage.log(CustomTabsFeature.CTF_PACKAGE_NAME);
        if (IntentUtils.safeHasExtra(intent, EXTRA_TOOLBAR_CORNER_RADIUS_IN_PIXEL_LEGACY)
                || IntentUtils.safeHasExtra(intent, EXTRA_TOOLBAR_CORNER_RADIUS_DP)) {
            featureUsage.log(CustomTabsFeature.EXTRA_TOOLBAR_CORNER_RADIUS_DP);
        }
        if (isPartialHeightCustomTab()) {
            featureUsage.log(CustomTabsFeature.CTF_PARTIAL);
        }
        if (isPartialWidthCustomTab()) {
            featureUsage.log(CustomTabsFeature.CTF_PARTIAL_SIDE_SHEET);
        }
        if (mRemoteViewsPendingIntent != null) {
            featureUsage.log(CustomTabsFeature.EXTRA_REMOTEVIEWS_PENDINGINTENT);
        }
        if (mClickableViewIds != null) {
            featureUsage.log(CustomTabsFeature.EXTRA_REMOTEVIEWS_VIEW_IDS);
        }
        if (mRemoteViews != null) featureUsage.log(CustomTabsFeature.EXTRA_REMOTEVIEWS);
        if (!mIsPartialCustomTabFixedHeight) {
            featureUsage.log(CustomTabsFeature.EXTRA_ACTIVITY_HEIGHT_RESIZE_BEHAVIOR);
        }
        if (mDefaultOrientation != ScreenOrientation.DEFAULT) {
            featureUsage.log(CustomTabsFeature.EXTRA_SCREEN_ORIENTATION);
        }
        if (mIsOpenedByChrome) featureUsage.log(CustomTabsFeature.CTF_SENT_BY_CHROME);
        if (mKeepAliveServiceIntent != null) featureUsage.log(CustomTabsFeature.EXTRA_KEEP_ALIVE);
        if (mShowShareItemInMenu) featureUsage.log(CustomTabsFeature.EXTRA_DEFAULT_SHARE_MENU_ITEM);
        if (IntentUtils.safeHasExtra(intent, CustomTabsIntent.EXTRA_SHARE_STATE)) {
            featureUsage.log(CustomTabsFeature.EXTRA_SHARE_STATE);
        }
        if (IntentUtils.safeHasExtra(intent, EXTRA_TITLE_VISIBILITY_STATE)) {
            featureUsage.log(CustomTabsFeature.EXTRA_TITLE_VISIBILITY_STATE);
        }
        if (IntentUtils.safeHasExtra(intent, CustomTabsIntent.EXTRA_TOOLBAR_ITEMS)) {
            featureUsage.log(CustomTabsFeature.EXTRA_TOOLBAR_ITEMS);
        }
        if (mTranslateLanguage != null) {
            featureUsage.log(CustomTabsFeature.EXTRA_TRANSLATE_LANGUAGE);
        }
        if (mAutoTranslateLanguage != null) {
            featureUsage.log(CustomTabsFeature.EXTRA_AUTO_TRANSLATE_LANGUAGE);
        }
        if (IntentUtils.safeHasExtra(intent, TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_MODE)) {
            featureUsage.log(CustomTabsFeature.EXTRA_DISPLAY_MODE);
        }
        if (mActivityType == ActivityType.TRUSTED_WEB_ACTIVITY) {
            featureUsage.log(CustomTabsFeature.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY);
        }
        if (mTrustedWebActivityAdditionalOrigins != null) {
            featureUsage.log(CustomTabsFeature.EXTRA_ADDITIONAL_TRUSTED_ORIGINS);
        }
        if (mEnableUrlBarHiding) featureUsage.log(CustomTabsFeature.EXTRA_ENABLE_URLBAR_HIDING);
        if (showSideSheetMaximizeButton()) {
            featureUsage.log(CustomTabsFeature.EXTRA_ACTIVITY_SIDE_SHEET_ENABLE_MAXIMIZATION);
        }
        if (mSecondaryToolbarSwipeUpPendingIntent != null) {
            featureUsage.log(CustomTabsFeature.EXTRA_SECONDARY_TOOLBAR_SWIPE_UP_ACTION);
        }
        if (IntentUtils.safeHasExtra(intent, EXTRA_ACTIVITY_SIDE_SHEET_POSITION)) {
            featureUsage.log(CustomTabsFeature.EXTRA_ACTIVITY_SIDE_SHEET_POSITION);
        }
        if (CustomTabsConnection.getInstance().shouldEnableGoogleBottomBarForIntent(this)) {
            featureUsage.log(CustomTabsFeature.EXTRA_ENABLE_GOOGLE_BOTTOM_BAR);
        }
        if (CustomTabsConnection.getInstance().hasExtraGoogleBottomBarButtons(this)) {
            featureUsage.log(CustomTabsFeature.EXTRA_GOOGLE_BOTTOM_BAR_BUTTONS);
        }
        if (mNetwork != null) featureUsage.log(CustomTabsFeature.EXTRA_NETWORK);
        if (IntentUtils.safeHasExtra(intent, EXTRA_OPEN_IN_BROWSER_STATE)) {
            featureUsage.log(CustomTabsFeature.EXTRA_OPEN_IN_BROWSER_STATE);
        }
        if (IntentUtils.safeHasExtra(
                intent, TrustedWebActivityIntentBuilder.EXTRA_LAUNCH_HANDLER_CLIENT_MODE)) {
            featureUsage.log(CustomTabsFeature.EXTRA_LAUNCH_HANDLER);
        }
        if (IntentUtils.safeHasExtra(
                intent, TrustedWebActivityIntentBuilder.EXTRA_FILE_HANDLING_DATA)) {
            featureUsage.log(CustomTabsFeature.EXTRA_FILE_HANDLERS);
        }
        if (IntentUtils.safeHasExtra(intent, EXTRA_CUSTOM_CONTENT_ACTIONS)) {
            featureUsage.log(CustomTabsFeature.EXTRA_CUSTOM_CONTENT_ACTIONS);
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
    public @Nullable SessionHolder<CustomTabsSessionToken> getSession() {
        return mSession;
    }

    @Override
    public @Nullable Intent getKeepAliveServiceIntent() {
        return mKeepAliveServiceIntent;
    }

    @Override
    public boolean isPartialHeightCustomTab() {
        if (DeviceInfo.isAutomotive()) return false;
        return getInitialActivityHeight() > 0;
    }

    @Override
    public boolean isPartialWidthCustomTab() {
        if (DeviceInfo.isAutomotive()) return false;
        return getInitialActivityWidth() > 0;
    }

    @Override
    public boolean isPartialCustomTab() {
        return isPartialHeightCustomTab() || isPartialWidthCustomTab();
    }

    @Override
    public boolean shouldAnimateOnFinish() {
        return getInsecureClientPackageNameForOnFinishAnimation() != null;
    }

    /** Returns client package name for finishing animation. */
    public @Nullable String getInsecureClientPackageNameForOnFinishAnimation() {
        // The package name may come from the insecure info contained in the animation
        // bundle which won't do any harm in the operation.
        if (mAnimationBundle == null) return null;
        return mAnimationBundle.getString(BUNDLE_PACKAGE_NAME);
    }

    @Override
    public @Nullable String getClientPackageName() {
        return getClientPackageNameFromSessionOrCallingActivity(mIntent, mSession);
    }

    @Override
    public @Nullable String getClientPackageNameIdentitySharing() {
        return IntentUtils.safeGetStringExtra(mIntent, IntentHandler.EXTRA_LAUNCHED_FROM_PACKAGE);
    }

    @Override
    public int getAnimationEnterRes() {
        return shouldAnimateOnFinish()
                ? assumeNonNull(mAnimationBundle).getInt(BUNDLE_ENTER_ANIMATION_RESOURCE)
                : 0;
    }

    @Override
    public int getAnimationExitRes() {
        return shouldAnimateOnFinish() && !isPartialCustomTab()
                ? assumeNonNull(mAnimationBundle).getInt(BUNDLE_EXIT_ANIMATION_RESOURCE)
                : 0;
    }

    @Override
    public boolean isTrustedIntent() {
        return mIsTrustedIntent;
    }

    @Override
    public @Nullable String getUrlToLoad() {
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
    public boolean contentScrollMayResizeTab() {
        return mContentScrollMayResizeTab;
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
    public @TitleVisibility int getTitleVisibilityState() {
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
    public List<CustomButtonParams> getCustomButtonsOnGoogleBottomBar() {
        return mGoogleBottomBarButtons;
    }

    @Override
    public @Nullable RemoteViews getBottomBarRemoteViews() {
        return mRemoteViews;
    }

    @Override
    public int @Nullable [] getClickableViewIDs() {
        if (mClickableViewIds == null) return null;
        return mClickableViewIds.clone();
    }

    @Override
    public @Nullable PendingIntent getRemoteViewsPendingIntent() {
        return mRemoteViewsPendingIntent;
    }

    @Override
    public @Nullable PendingIntent getSecondaryToolbarSwipeUpPendingIntent() {
        return mSecondaryToolbarSwipeUpPendingIntent;
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
    void setPendingIntentOnFinishedForTesting(PendingIntent.OnFinished onFinished) {
        mOnFinishedForTesting = onFinished;
        ResettersForTesting.register(() -> mOnFinishedForTesting = null);
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
    public @Nullable String getMediaViewerUrl() {
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
    public boolean shouldShowStarButton() {
        return !mDisableStar;
    }

    @Override
    public boolean shouldShowDownloadButton() {
        return !mDisableDownload;
    }

    @Override
    public @Nullable TrustedWebActivityDisplayMode getProvidedTwaDisplayMode() {
        return mTrustedWebActivityDisplayMode;
    }

    public List<TrustedWebActivityDisplayMode> getProvidedTwaDisplayOverrideMode() {
        return mTrustedWebActivityDisplayOverrideMode;
    }

    @Override
    public @Nullable List<String> getTrustedWebActivityAdditionalOrigins() {
        return mTrustedWebActivityAdditionalOrigins;
    }

    @Override
    public @Nullable Set<Origin> getAllTrustedWebActivityOrigins() {
        // Lazily compute this, since `getUrlToLoad()` requires native to be loaded.
        if (mAllTrustedWebActivityOrigins != null) {
            return mAllTrustedWebActivityOrigins;
        }

        mAllTrustedWebActivityOrigins = new HashSet<>();
        String url = getUrlToLoad();
        Origin initialOrigin = Origin.create(url == null ? "" : url);
        if (initialOrigin != null) mAllTrustedWebActivityOrigins.add(initialOrigin);
        if (mTrustedWebActivityAdditionalOrigins != null) {
            for (String originAsString : mTrustedWebActivityAdditionalOrigins) {
                Origin origin = Origin.create(originAsString);
                if (origin == null) continue;

                mAllTrustedWebActivityOrigins.add(origin);
            }
        }

        return mAllTrustedWebActivityOrigins;
    }

    @Override
    public @Nullable String getTranslateLanguage() {
        return shouldAutoTranslate() ? mAutoTranslateLanguage : mTranslateLanguage;
    }

    @Override
    public boolean shouldAutoTranslate() {
        return mAutoTranslateLanguage != null && isAllowedToAutoTranslate();
    }

    private static boolean isPackageNameInList(
            @Nullable String packageName, String pipeDelimitedList) {
        if (packageName == null || TextUtils.isEmpty(pipeDelimitedList)) return false;
        for (String p : pipeDelimitedList.split("\\|")) {
            if (packageName.equals(p)) return true;
        }
        return false;
    }

    private boolean isAllowedToAutoTranslate() {
        if (!ChromeFeatureList.sCctAutoTranslate.isEnabled()) {
            return false;
        }
        if (mIsTrustedIntent
                && ChromeFeatureList.sCctAutoTranslateAllowAllFirstParties.getValue()) {
            return true;
        }
        return isPackageNameInList(
                getClientPackageName(),
                ChromeFeatureList.sCctAutoTranslatePackageNamesAllowlist.getValue());
    }

    @Override
    public @Nullable ShareTarget getShareTarget() {
        Bundle bundle =
                IntentUtils.safeGetBundleExtra(
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
    public @Nullable ShareData getShareData() {
        Bundle bundle =
                IntentUtils.safeGetParcelableExtra(
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

    @ActivitySideSheetDecorationType
    @Override
    public int getActivitySideSheetDecorationType() {
        return mSideSheetDecorationType;
    }

    @ActivitySideSheetRoundedCornersPosition
    @Override
    public int getActivitySideSheetRoundedCornersPosition() {
        return mSideSheetRoundedCornersPosition;
    }

    @Override
    public int @Nullable [] getGsaExperimentIds() {
        return mGsaExperimentIds;
    }

    @Override
    public @Px int getInitialActivityHeight() {
        return getInitialActivityHeight(
                mIsTrustedIntent, mInitialActivityHeight, getClientPackageName());
    }

    @Override
    public @Px int getInitialActivityWidth() {
        return getInitialActivityWidth(
                mIsTrustedIntent, mInitialActivityWidth, getClientPackageName());
    }

    @Override
    public int getActivityBreakPoint() {
        return mBreakPointDp;
    }

    static boolean isAllowedThirdParty(@Nullable String packageName) {
        if (packageName == null) return false;
        String defaultPolicy =
                ChromeFeatureList.sCctResizableForThirdPartiesDefaultPolicy.getValue();
        if (defaultPolicy.equals(DEFAULT_POLICY_USE_ALLOWLIST)) {
            return isPackageNameInList(
                    packageName,
                    ChromeFeatureList.sCctResizableForThirdPartiesAllowlistEntries.getValue());
        } else if (defaultPolicy.equals(DEFAULT_POLICY_USE_DENYLIST)) {
            return !isPackageNameInList(
                    packageName,
                    ChromeFeatureList.sCctResizableForThirdPartiesDenylistEntries.getValue());
        }
        assert false : "We can't get here since the default policy is use denylist.";
        return false;
    }

    @Override
    public @CloseButtonPosition int getCloseButtonPosition() {
        if (!mIsCloseButtonEnabled) return CLOSE_BUTTON_POSITION_DEFAULT;
        return IntentUtils.safeGetIntExtra(
                mIntent, EXTRA_CLOSE_BUTTON_POSITION, CLOSE_BUTTON_POSITION_DEFAULT);
    }

    @Override
    public boolean shouldSuppressAppMenu() {
        // The media viewer has no default menu items, so if there are also no custom items, we
        // should disable the menu altogether.
        return isMediaViewer() && getMenuTitles().isEmpty();
    }

    @Override
    public int getPartialTabToolbarCornerRadius() {
        return mPartialTabToolbarCornerRadius;
    }

    @Override
    public boolean isPartialCustomTabFixedHeight() {
        return mIsPartialCustomTabFixedHeight;
    }

    @Override
    public boolean canInteractWithBackground() {
        return mInteractWithBackground;
    }

    @Override
    public boolean showSideSheetMaximizeButton() {
        return IntentUtils.safeGetBooleanExtra(
                mIntent, EXTRA_ACTIVITY_SIDE_SHEET_ENABLE_MAXIMIZATION, false);
    }

    @Override
    public int getSideSheetSlideInBehavior() {
        return ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE;
    }

    @Override
    public int getSideSheetPosition() {
        int position = CustomTabsIntent.getActivitySideSheetPosition(mIntent);
        return position == ACTIVITY_SIDE_SHEET_POSITION_DEFAULT
                ? ACTIVITY_SIDE_SHEET_POSITION_END
                : position;
    }

    @Override
    public boolean isInteractiveOmniboxAllowed() {
        if (isOffTheRecord()) return false;
        if (isPartialCustomTab()) return false;
        if (DeviceInfo.isAutomotive()) return false;

        return true;
    }

    @Override
    public boolean isInteractiveOmniboxEnabled() {
        return ChromeFeatureList.sSearchInCCT.isEnabled();
    }

    @Override
    public long getTargetNetwork() {
        return mNetwork != null ? mNetwork.getNetworkHandle() : NetId.INVALID;
    }

    @Override
    public boolean isCloseButtonEnabled() {
        return mIsCloseButtonEnabled;
    }

    @Override
    public @LaunchHandlerClientMode.ClientMode int getLaunchHandlerClientMode() {
        @LaunchHandlerClientMode.ClientMode
        int clientMode =
                IntentUtils.safeGetIntExtra(
                        mIntent,
                        TrustedWebActivityIntentBuilder.EXTRA_LAUNCH_HANDLER_CLIENT_MODE,
                        LaunchHandlerClientMode.AUTO);

        if (Arrays.asList(NAVIGATE_EXISTING, FOCUS_EXISTING, NAVIGATE_NEW).contains(clientMode)) {
            return clientMode;
        } else {
            return LaunchHandlerClientMode.AUTO;
        }
    }

    @Override
    public @Nullable FileHandlingData getFileHandlingData() {
        Bundle bundle =
                IntentUtils.safeGetBundleExtra(
                        getIntent(), TrustedWebActivityIntentBuilder.EXTRA_FILE_HANDLING_DATA);
        if (bundle == null) return null;
        try {
            return FileHandlingData.fromBundle(bundle);
        } catch (Throwable e) {
            // Catch unparcelling errors potentially thrown by AndroidX bundle parsing code
            return null;
        }
    }

    @Override
    public int getResolvedDisplayMode() {
        if (!isTrustedWebActivity()) {
            return DisplayMode.UNDEFINED;
        }

        if (mResolvedDisplayMode != DisplayMode.UNDEFINED) {
            return mResolvedDisplayMode;
        }

        mResolvedDisplayMode = resolveDisplayMode();
        return mResolvedDisplayMode;
    }

    @ExperimentalOpenInBrowser
    @Override
    public @OpenInBrowserState int getOpenInBrowserButtonState() {
        return mOpenInBrowserState;
    }

    @Override
    public int getShareButtonState() {
        return mShareState;
    }

    @ExperimentalCustomContentAction
    @Override
    public List<CustomContentAction> getCustomContentActions() {
        if (ChromeFeatureList.sCctContextualMenuItems.isEnabled()) {
            return CustomTabsIntent.getCustomContentActions(mIntent);
        }
        return List.of();
    }

    @Override
    public boolean isOptionalButtonSupported() {
        return ChromeFeatureList.sCctAdaptiveButton.isEnabled()
                && !isTrustedWebActivity()
                && mUiType == CustomTabsUiType.DEFAULT;
    }

    private static boolean isDisplayModeSupported(
            @Nullable TrustedWebActivityDisplayMode displayMode, boolean isDisplayOverride) {
        if (displayMode == null) {
            return false;
        }
        if (displayMode instanceof TrustedWebActivityDisplayMode.ImmersiveMode) {
            return true;
        }
        if (displayMode instanceof TrustedWebActivityDisplayMode.BrowserMode) {
            return !isDisplayOverride;
        }
        if (WebAppHeaderUtils.isMinimalUiEnabled()
                && displayMode instanceof TrustedWebActivityDisplayMode.MinimalUiMode) {
            return true;
        }

        if (WebAppHeaderUtils.isWindowControlsOverlayFlagEnabled()
                && displayMode instanceof TrustedWebActivityDisplayMode.WindowControlsOverlayMode) {
            return isDisplayOverride;
        }

        return false;
    }

    private @DisplayMode.EnumType int resolveDisplayMode() {
        TrustedWebActivityDisplayMode displayMode = null;

        List<TrustedWebActivityDisplayMode> displayOverride = getProvidedTwaDisplayOverrideMode();
        for (TrustedWebActivityDisplayMode override : displayOverride) {
            if (isDisplayModeSupported(override, /* isDisplayOverride= */ true)) {
                displayMode = override;
                break;
            }
        }

        if (displayMode == null
                && isDisplayModeSupported(
                        getProvidedTwaDisplayMode(), /* isDisplayOverride= */ false)) {
            displayMode = getProvidedTwaDisplayMode();
        }

        if (displayMode == null) {
            return DisplayMode.STANDALONE;
        }

        if (displayMode instanceof TrustedWebActivityDisplayMode.ImmersiveMode) {
            return DisplayMode.FULLSCREEN;
        }

        // `browser` display mode web apps are not installable by default, because by the spec they
        // should be opened in a new tab or browser window, but in Chrome they can be forcefully
        // installed via app menu. In this case display mode should resolve to the first supported
        // display mode in the "fullscreen -> standalone -> minimal-ui -> browser" fallback chain.
        boolean shouldUseMinimalUi =
                displayMode instanceof TrustedWebActivityDisplayMode.MinimalUiMode
                        || displayMode instanceof TrustedWebActivityDisplayMode.BrowserMode;
        if (WebAppHeaderUtils.isMinimalUiEnabled() && shouldUseMinimalUi) {
            return DisplayMode.MINIMAL_UI;
        }

        if (WebAppHeaderUtils.isWindowControlsOverlayFlagEnabled()
                && displayMode instanceof TrustedWebActivityDisplayMode.WindowControlsOverlayMode) {
            return DisplayMode.WINDOW_CONTROLS_OVERLAY;
        }

        return DisplayMode.STANDALONE;
    }

    @Override
    public long getTwaStartupUptimeMillis() {
        if (!isTrustedWebActivity()) return 0;
        return IntentUtils.safeGetLongExtra(getIntent(), EXTRA_TWA_STARTUP_UPTIME_MS, 0);
    }

    @Override
    public int getAndroidBrowserHelperVersion() {
        return IntentUtils.safeGetIntExtra(getIntent(), EXTRA_ANDROID_BROWSER_HELPER_VERSION, 0);
    }

    private boolean isCpaOnlyOpenInBrowserDefault() {
        return ChromeFeatureList.sCctAdaptiveButton.isEnabled()
                && ChromeFeatureList.sCctAdaptiveButtonContextualOnly.getValue()
                && ChromeFeatureList.sCctAdaptiveButtonDefaultVariant.getValue()
                        == AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER;
    }

    @Override
    public @Nullable WindowFeatures getRequestedWindowFeatures() {
        if (mUiType != CustomTabsUiType.POPUP) {
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

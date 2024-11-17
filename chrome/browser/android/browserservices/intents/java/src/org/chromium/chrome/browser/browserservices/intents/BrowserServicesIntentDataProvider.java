// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_POSITION_END;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE;
import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_DEFAULT;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.widget.RemoteViews;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsIntent.CloseButtonPosition;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.trusted.TrustedWebActivityDisplayMode;
import androidx.browser.trusted.sharing.ShareData;
import androidx.browser.trusted.sharing.ShareTarget;

import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.net.NetId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Base class for model classes which parse incoming intent for customization data. */
public abstract class BrowserServicesIntentDataProvider {
    // The type of UI for Custom Tab to use.
    @IntDef({
        CustomTabsUiType.DEFAULT,
        CustomTabsUiType.MEDIA_VIEWER,
        CustomTabsUiType.INFO_PAGE,
        CustomTabsUiType.READER_MODE,
        CustomTabsUiType.MINIMAL_UI_WEBAPP,
        CustomTabsUiType.OFFLINE_PAGE,
        CustomTabsUiType.AUTH_TAB
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CustomTabsUiType {
        int DEFAULT = 0;
        int MEDIA_VIEWER = 1;
        int INFO_PAGE = 2;
        int READER_MODE = 3;
        int MINIMAL_UI_WEBAPP = 4;
        int OFFLINE_PAGE = 5;
        int READ_LATER = 6;
        int AUTH_TAB = 7;
    }

    // The type of Disclosure for TWAs to use.
    @IntDef({
        TwaDisclosureUi.DEFAULT,
        TwaDisclosureUi.V1_INFOBAR,
        TwaDisclosureUi.V2_NOTIFICATION_OR_SNACKBAR
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TwaDisclosureUi {
        int DEFAULT = -1;
        int V1_INFOBAR = 0;
        int V2_NOTIFICATION_OR_SNACKBAR = 1;
    }

    @IntDef({
        ACTIVITY_SIDE_SHEET_SLIDE_IN_DEFAULT,
        ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_BOTTOM,
        ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActivitySideSheetSlideInBehavior {}

    // The type of Profile and UI that is used by the custom tab.
    @IntDef({
        CustomTabProfileType.REGULAR,
        CustomTabProfileType.INCOGNITO,
        CustomTabProfileType.EPHEMERAL
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CustomTabProfileType {
        // The normal user profile.
        int REGULAR = 0;
        // An off-the-record profile with incognito UI.
        int INCOGNITO = 1;
        // An off-the-record profile without references to incognito mode.
        int EPHEMERAL = 2;
    }

    /**
     * Side sheet's default slide-in behavior. Same as {@link
     * ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE}.
     */
    public static final int ACTIVITY_SIDE_SHEET_SLIDE_IN_DEFAULT = 0;

    /** Side sheet's slide-in behavior defined for bottom-to-up animation. */
    public static final int ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_BOTTOM = 1;

    /** Side shset's slide-in behavior for side-wise animation. */
    public static final int ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE = 2;

    /**
     * @return The type of the Activity;
     */
    public abstract @ActivityType int getActivityType();

    /**
     * @return the Intent this instance was created with.
     */
    public @Nullable Intent getIntent() {
        return null;
    }

    /**
     * @return The session specified in the intent, or null.
     */
    public @Nullable CustomTabsSessionToken getSession() {
        return null;
    }

    /**
     * @return The keep alive service intent specified in the intent, or null.
     */
    public @Nullable Intent getKeepAliveServiceIntent() {
        return null;
    }

    /**
     * @return Whether chrome should animate when it finishes. We show animations only if the client
     *         app has supplied the correct animation resources via intent extra.
     */
    public boolean shouldAnimateOnFinish() {
        return false;
    }

    /**
     * @return The package name of the client app. This is used for a workaround in order to
     *         retrieve the client's animation resources.
     */
    public @Nullable String getClientPackageName() {
        return null;
    }

    /**
     * @return The package name of the client app provided via identity sharing API introduced in
     *     Android U.
     */
    public @Nullable String getClientPackageNameIdentitySharing() {
        return null;
    }

    /**
     * @return The resource id for enter animation, which is used in {@link
     *     Activity#overridePendingTransition(int, int)}.
     */
    public int getAnimationEnterRes() {
        return 0;
    }

    /**
     * @return The resource id for exit animation, which is used in
     *         {@link Activity#overridePendingTransition(int, int)}.
     */
    public int getAnimationExitRes() {
        return 0;
    }

    /** Checks whether or not the Intent is from Chrome or other trusted first party. */
    public boolean isTrustedIntent() {
        return false;
    }

    /**
     * @return The URL that should be used from this intent.
     * Must be called only after native has loaded.
     */
    public @Nullable String getUrlToLoad() {
        return null;
    }

    /**
     * @return Whether url bar hiding should be enabled in the custom tab.
     */
    public boolean shouldEnableUrlBarHiding() {
        return true;
    }

    /**
     * @return Whether scroll on content view may drag/resize the custom tab.
     */
    public boolean contentScrollMayResizeTab() {
        return false;
    }

    /**
     * @return ColorProvider to be used.
     */
    public abstract @NonNull ColorProvider getColorProvider();

    /**
     * @return ColorProvider when the system is in light mode.
     */
    public @NonNull ColorProvider getLightColorProvider() {
        return getColorProvider();
    }

    /**
     * @return ColorProvider when the system is in dark mode.
     */
    public @NonNull ColorProvider getDarkColorProvider() {
        return getColorProvider();
    }

    /**
     * @return The drawable of the icon of close button shown in the custom tab toolbar.
     */
    public @Nullable Drawable getCloseButtonDrawable() {
        return null;
    }

    /**
     * @return The title visibility state for the toolbar.
     */
    public int getTitleVisibilityState() {
        return CustomTabsIntent.NO_TITLE;
    }

    /**
     * @return Whether the default share item should be shown in the menu.
     */
    public boolean shouldShowShareMenuItem() {
        return false;
    }

    /**
     * @return The params for the custom buttons that show on the toolbar.
     */
    public List<CustomButtonParams> getCustomButtonsOnToolbar() {
        return Collections.emptyList();
    }

    /**
     * @return The list of params representing the buttons on the bottombar.
     */
    public List<CustomButtonParams> getCustomButtonsOnBottombar() {
        return Collections.emptyList();
    }

    /**
     * @return The list of params representing the custom buttons on the Google Bottom Bar.
     */
    public List<CustomButtonParams> getCustomButtonsOnGoogleBottomBar() {
        return Collections.emptyList();
    }

    /**
     * @return The {@link RemoteViews} to show on the bottom bar, or null if the extra is not
     *     specified.
     */
    public @Nullable RemoteViews getBottomBarRemoteViews() {
        return null;
    }

    /**
     * @return A array of {@link View} ids, of which the onClick event is handled by the Activity.
     */
    @Nullable
    public int[] getClickableViewIDs() {
        return null;
    }

    /**
     * @return The {@link PendingIntent} that is sent when the user clicks on the remote view.
     */
    public @Nullable PendingIntent getRemoteViewsPendingIntent() {
        return null;
    }

    /**
     * @return The {@link PendingIntent} that is sent when the user swipes up from the secondary
     *         (bottom) toolbar.
     */
    public @Nullable PendingIntent getSecondaryToolbarSwipeUpPendingIntent() {
        return null;
    }

    /**
     * Gets params for all custom buttons, which is the combination of
     * {@link #getCustomButtonsOnBottombar()} and {@link #getCustomButtonsOnToolbar()}.
     */
    public List<CustomButtonParams> getAllCustomButtons() {
        return Collections.emptyList();
    }

    /**
     * @return Titles of menu items that were passed from client app via intent.
     */
    public List<String> getMenuTitles() {
        return Collections.emptyList();
    }

    /**
     * @return Whether or not the Activity is being launched by an intent fired by Chrome itself.
     */
    public boolean isOpenedByChrome() {
        return false;
    }

    public @CustomTabsUiType int getUiType() {
        return CustomTabsUiType.DEFAULT;
    }

    /**
     * @return URL that should be loaded in place of the URL in {@link Intent#getData()}.
     */
    public @Nullable String getMediaViewerUrl() {
        return null;
    }

    /**
     * @return Whether to enable the embedded media experience.
     */
    public boolean shouldEnableEmbeddedMediaExperience() {
        return false;
    }

    public boolean isFromMediaLauncherActivity() {
        return false;
    }

    /**
     * @return Whether there should be a star button in the menu.
     */
    public boolean shouldShowStarButton() {
        return true;
    }

    /**
     * @return Whether there should be a download button in the menu.
     */
    public boolean shouldShowDownloadButton() {
        return true;
    }

    /**
     * @return Whether the Activity uses an off-the-record profile.
     */
    public boolean isOffTheRecord() {
        switch (getCustomTabMode()) {
            case CustomTabProfileType.EPHEMERAL:
            case CustomTabProfileType.INCOGNITO:
                return true;
            case CustomTabProfileType.REGULAR:
                return false;
        }
        assert false; // NOTREACHED
        return false;
    }

    /**
     * @return Whether the Activity is a regular, incognito or ephemeral custom tab.
     */
    public @CustomTabProfileType int getCustomTabMode() {
        return CustomTabProfileType.REGULAR;
    }

    /**
     * @return Whether the Activity should attempt to display a Trusted Web Activity.
     */
    public final boolean isTrustedWebActivity() {
        return getActivityType() == ActivityType.TRUSTED_WEB_ACTIVITY;
    }

    /**
     * @return Whether the Activity is either a Webapp or a WebAPK activity.
     */
    public final boolean isWebappOrWebApkActivity() {
        return getActivityType() == ActivityType.WEBAPP
                || getActivityType() == ActivityType.WEB_APK;
    }

    /**
     * @return Whether the Activity is a WebAPK activity.
     */
    public final boolean isWebApkActivity() {
        return getActivityType() == ActivityType.WEB_APK;
    }

    /** Returns {@link TrustedWebActivityDisplayMode} supplied in the intent. */
    public @Nullable TrustedWebActivityDisplayMode getTwaDisplayMode() {
        return null;
    }

    /** Returns {@link ScreenOrientationLockType} supplied in the intent. */
    public int getDefaultOrientation() {
        return ScreenOrientationLockType.DEFAULT;
    }

    /**
     * @return The component name of the module entry point, or null if not specified.
     */
    public @Nullable ComponentName getModuleComponentName() {
        return null;
    }

    /**
     * @return The resource identifier for the dex that contains module code. {@code 0} if no dex
     * resource is provided.
     */
    public @Nullable String getModuleDexAssetName() {
        return null;
    }

    /**
     * @return Additional origins associated with a Trusted Web Activity client app.
     */
    @Nullable
    public List<String> getTrustedWebActivityAdditionalOrigins() {
        return null;
    }

    /**
     * @return All origins associated with a TrustedWebActivity client app, including the initially
     *     loaded origin.
     */
    @Nullable
    public Set<Origin> getAllTrustedWebActivityOrigins() {
        return null;
    }

    /**
     * @return ISO 639 code of target language the page should be translated to. This method
     *     requires native.
     */
    public @Nullable String getTranslateLanguage() {
        return null;
    }

    /**
     * @return Whether or not the page should be automatically translated into the target language
     *         indicated by {@link getTranslateLanguage()}.
     */
    public boolean shouldAutoTranslate() {
        return false;
    }

    /**
     * Returns {@link ShareTarget} describing the share target, or null if there is no associated
     * share target.
     */
    public @Nullable ShareTarget getShareTarget() {
        return null;
    }

    /** Returns {@link ShareData} if there is data to be shared, and null otherwise. */
    public @Nullable ShareData getShareData() {
        return null;
    }

    /** Returns {@link WebappExtras} if the intent targets a webapp, and null otherwise. */
    public @Nullable WebappExtras getWebappExtras() {
        return null;
    }

    /** Returns {@link WebApkExtras} if the intent targets a WebAPK, and null otherwise. */
    public @Nullable WebApkExtras getWebApkExtras() {
        return null;
    }

    /**
     * @return Whether the bottom bar should be shown.
     */
    public final boolean shouldShowBottomBar() {
        return !getCustomButtonsOnBottombar().isEmpty() || getBottomBarRemoteViews() != null;
    }

    /**
     * Searches for the toolbar button with the given {@code id} and returns its index.
     * @param id The ID of a toolbar button to search for.
     * @return The index of the toolbar button with the given {@code id}, or -1 if no such button
     *         can be found.
     */
    public final int getCustomToolbarButtonIndexForId(int id) {
        List<CustomButtonParams> toolbarButtons = getCustomButtonsOnToolbar();
        for (int i = 0; i < toolbarButtons.size(); i++) {
            if (toolbarButtons.get(i).getId() == id) return i;
        }
        return -1;
    }

    /**
     * @return The {@link CustomButtonParams} (either on the toolbar or bottom bar) with the given
     *         {@code id}, or null if no such button can be found.
     */
    public final @Nullable CustomButtonParams getButtonParamsForId(int id) {
        List<CustomButtonParams> customButtonParams = getAllCustomButtons();
        for (CustomButtonParams params : customButtonParams) {
            // A custom button params will always carry an ID. If the client calls updateVisuals()
            // without an id, we will assign the toolbar action button id to it.
            if (id == params.getId()) return params;
        }
        return null;
    }

    /**
     * @return See {@link #getUiType()}.
     */
    public final boolean isMediaViewer() {
        return getUiType() == CustomTabsUiType.MEDIA_VIEWER;
    }

    /**
     * @return If the Activity is an info page.
     */
    public final boolean isInfoPage() {
        return getUiType() == CustomTabsUiType.INFO_PAGE;
    }

    public @TwaDisclosureUi int getTwaDisclosureUi() {
        return TwaDisclosureUi.DEFAULT;
    }

    @Nullable
    public int[] getGsaExperimentIds() {
        return null;
    }

    /**
     * @return Whether the intent is for partial custom tabs bottom sheet.
     */
    public boolean isPartialHeightCustomTab() {
        return false;
    }

    /**
     * @return Whether the intent is for partial custom tabs side sheet.
     */
    public boolean isPartialWidthCustomTab() {
        return false;
    }

    /**
     * @return Whether the intent is partial custom tabs side sheet or bottom sheet.
     */
    public boolean isPartialCustomTab() {
        return false;
    }

    /**
     * @return The value in pixels of the initial height of the Activity. It will return 0 if there
     *         is no value set.
     */
    public @Px int getInitialActivityHeight() {
        return 0;
    }

    /**
     * @return The value in pixels of the initial width of the Activity. It will return 0 if there
     *          is no value set.
     */
    public @Px int getInitialActivityWidth() {
        return 0;
    }

    /**
     * @return The value in pixels of the breakpoint where Side Sheets behave as Bottom Sheets.
     *          It will return 0 if there is no value set.
     */
    public int getActivityBreakPoint() {
        return 0;
    }

    /**
     * @return An int representing the side sheet decoration type for the Activity.
     */
    public int getActivitySideSheetDecorationType() {
        return ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW;
    }

    /**
     * @return An int representing the side sheet rounded corner position for the Activity
     */
    public int getActivitySideSheetRoundedCornersPosition() {
        return ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE;
    }

    /** Returns the {@link CloseButtonPosition}. */
    public @CloseButtonPosition int getCloseButtonPosition() {
        return CLOSE_BUTTON_POSITION_DEFAULT;
    }

    /**
     * If {@code true} the App Menu will not be shown. If {@code false} it will be left to the
     * Activity to decide.
     */
    public boolean shouldSuppressAppMenu() {
        return false;
    }

    /** Returns the partial custom tab toolbar corner radius. */
    public @Px int getPartialTabToolbarCornerRadius() {
        return 0;
    }

    /** Returns false as by default PCCT is resizable. */
    public boolean isPartialCustomTabFixedHeight() {
        return false;
    }

    /**
     * @return true, as by default having a PCCT launched still allows interaction with the
     * background application
     */
    public boolean canInteractWithBackground() {
        return false;
    }

    /** Return false since by default side panel does not show maximize button. */
    public boolean showSideSheetMaximizeButton() {
        return false;
    }

    /** Return the default behavior. */
    public int getSideSheetSlideInBehavior() {
        return ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE;
    }

    /** Return the default position. */
    public int getSideSheetPosition() {
        return ACTIVITY_SIDE_SHEET_POSITION_END;
    }

    /** Return whether calling package should be allowed to present an interactive Omnibox. */
    public boolean isInteractiveOmniboxAllowed() {
        return false;
    }

    /**
     * Return the target network handle {@link android.net.Network#getNetworkHandle} that loads
     * associated with this intent must use. Defaults to {@code NetId.INVALID}, in which case we let
     * the underlying system make this choice.
     */
    public long getTargetNetwork() {
        return NetId.INVALID;
    }

    /**
     * Return whether this intent has a target network. Certain optimizations or features are not
     * support for tabs targeting a network. This helper is useful for handling those scenarios.
     */
    public boolean hasTargetNetwork() {
        return getTargetNetwork() != NetId.INVALID;
    }

    /** Return {@code true} if the service was launched for authentication. */
    public boolean isAuthTab() {
        return false;
    }

    /** Return the custom redirect scheme for AuthTab. */
    public String getAuthRedirectScheme() {
        return null;
    }

    /** Return the https redirect URL host (origin) for AuthTab. */
    public String getAuthRedirectHost() {
        return null;
    }

    /** Return the https redirect URL path for AuthTab. */
    public String getAuthRedirectPath() {
        return null;
    }
}

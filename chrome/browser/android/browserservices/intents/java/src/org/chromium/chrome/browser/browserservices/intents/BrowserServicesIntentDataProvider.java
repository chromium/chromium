// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.widget.RemoteViews;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.trusted.TrustedWebActivityDisplayMode;
import androidx.browser.trusted.sharing.ShareData;
import androidx.browser.trusted.sharing.ShareTarget;

import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.device.mojom.ScreenOrientationLockType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.List;

/**
 * Base class for model classes which parse incoming intent for customization data.
 */
public abstract class BrowserServicesIntentDataProvider {
    // The type of UI for Custom Tab to use.
    @IntDef({CustomTabsUiType.DEFAULT, CustomTabsUiType.MEDIA_VIEWER, CustomTabsUiType.INFO_PAGE,
            CustomTabsUiType.READER_MODE, CustomTabsUiType.MINIMAL_UI_WEBAPP,
            CustomTabsUiType.OFFLINE_PAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CustomTabsUiType {
        int DEFAULT = 0;
        int MEDIA_VIEWER = 1;
        int INFO_PAGE = 2;
        int READER_MODE = 3;
        int MINIMAL_UI_WEBAPP = 4;
        int OFFLINE_PAGE = 5;
        int READ_LATER = 6;
    }

    // The type of Disclosure for TWAs to use.
    @IntDef({TwaDisclosureUi.DEFAULT, TwaDisclosureUi.V1_INFOBAR,
            TwaDisclosureUi.V2_NOTIFICATION_OR_SNACKBAR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TwaDisclosureUi {
        int DEFAULT = -1;
        int V1_INFOBAR = 0;
        int V2_NOTIFICATION_OR_SNACKBAR = 1;
    }

    /**
     * @return The type of the Activity;
     */
    public abstract @ActivityType int getActivityType();

    /**
     * @return the Intent this instance was created with.
     */
    @Nullable
    public Intent getIntent() {
        return null;
    }

    /**
     * @return The session specified in the intent, or null.
     */
    @Nullable
    public CustomTabsSessionToken getSession() {
        return null;
    }

    /**
     * @return The keep alive service intent specified in the intent, or null.
     */
    @Nullable
    public Intent getKeepAliveServiceIntent() {
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
    @Nullable
    public String getClientPackageName() {
        return null;
    }

    /**
     * @return The resource id for enter animation, which is used in
     *         {@link Activity#overridePendingTransition(int, int)}.
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

    /**
     * Checks whether or not the Intent is from Chrome or other trusted first party.
     *
     * @deprecated This method is not reliable, see https://crbug.com/832124
     */
    @Deprecated
    public boolean isTrustedIntent() {
        return false;
    }

    /**
     * @return The URL that should be used from this intent. If it is a WebLite url, it may be
     *         overridden if the Data Reduction Proxy is using Lo-Fi previews.
     * Must be called only after native has loaded.
     */
    @Nullable
    public String getUrlToLoad() {
        return null;
    }

    /**
     * @return Whether url bar hiding should be enabled in the custom tab.
     */
    public boolean shouldEnableUrlBarHiding() {
        return true;
    }

    /**
     * @return The toolbar color.
     */
    public int getToolbarColor() {
        return Color.WHITE;
    }

    /**
     * @return Whether the intent specifies a custom toolbar color.
     */
    public boolean hasCustomToolbarColor() {
        return false;
    }

    /**
     * @return The navigation bar color specified in the intent, or null if not specified.
     */
    @Nullable
    public Integer getNavigationBarColor() {
        return null;
    }

    /**
     * @return The navigation bar divider color specified in the intent, or null if not specified.
     */
    @Nullable
    public Integer getNavigationBarDividerColor() {
        return null;
    }

    /**
     * @return The drawable of the icon of close button shown in the custom tab toolbar.
     */
    @Nullable
    public Drawable getCloseButtonDrawable() {
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
     * @return The color of the bottom bar.
     */
    public int getBottomBarColor() {
        return getToolbarColor();
    }

    /**
     * @return The {@link RemoteViews} to show on the bottom bar, or null if the extra is not
     *         specified.
     */
    @Nullable
    public RemoteViews getBottomBarRemoteViews() {
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
    @Nullable
    public PendingIntent getRemoteViewsPendingIntent() {
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
      @return Titles of menu items that were passed from client app via intent.
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

    @CustomTabsUiType
    public int getUiType() {
        return CustomTabsUiType.DEFAULT;
    }

    /**
     * @return URL that should be loaded in place of the URL in {@link Intent#getData()}.
     */
    @Nullable
    public String getMediaViewerUrl() {
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
     * @return Initial RGB background color.
     */
    public int getInitialBackgroundColor() {
        return Color.TRANSPARENT;
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
     * @return Whether the Activity should be opened in incognito mode.
     */
    public boolean isIncognito() {
        return false;
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

    /**
     * @return Whether the Activity should attempt to load a dynamic module.
     *
     * Will return false if native is not initialized.
     */
    public boolean isDynamicModuleEnabled() {
        return false;
    }

    /**
     * Returns {@link TrustedWebActivityDisplayMode} supplied in the intent.
     */
    @Nullable
    public TrustedWebActivityDisplayMode getTwaDisplayMode() {
        return null;
    }

    /**
     * Returns {@link ScreenOrientationLockType} supplied in the intent.
     */
    public int getDefaultOrientation() {
        return ScreenOrientationLockType.DEFAULT;
    }

    /**
     * @return The component name of the module entry point, or null if not specified.
     */
    @Nullable
    public ComponentName getModuleComponentName() {
        return null;
    }

    /**
     * @return The resource identifier for the dex that contains module code. {@code 0} if no dex
     * resource is provided.
     */
    @Nullable
    public String getModuleDexAssetName() {
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
     * @return ISO 639 code of target language the page should be translated to.
     * This method requires native.
     */
    @Nullable
    public String getTranslateLanguage() {
        return null;
    }

    /**
     * Returns {@link ShareTarget} describing the share target, or null if there is no associated
     * share target.
     */
    @Nullable
    public ShareTarget getShareTarget() {
        return null;
    }

    /**
     * Returns {@link ShareData} if there is data to be shared, and null otherwise.
     */
    @Nullable
    public ShareData getShareData() {
        return null;
    }

    /**
     * Returns {@link WebappExtras} if the intent targets a webapp, and null otherwise.
     */
    @Nullable
    public WebappExtras getWebappExtras() {
        return null;
    }

    /**
     * Returns {@link WebApkExtras} if the intent targets a WebAPK, and null otherwise.
     */
    @Nullable
    public WebApkExtras getWebApkExtras() {
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
    @Nullable
    public final CustomButtonParams getButtonParamsForId(int id) {
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

    @TwaDisclosureUi
    public int getTwaDisclosureUi() {
        return TwaDisclosureUi.DEFAULT;
    }

    @Nullable
    public int[] getGsaExperimentIds() {
        return null;
    }

    /**
     * Returns true if omnibox should hide cct related visits.
     */
    public boolean shouldHideOmniboxSuggestionsForCctVisits() {
        return false;
    }

    /**
     * Returns true if visits from cct should be hidden.
     */
    public boolean shouldHideCctVisits() {
        return false;
    }

    /**
     * Returns true if new notification requests from cct should be blocked.
     */
    public boolean shouldBlockNewNotificationRequests() {
        return false;
    }

    /**
     * Returns true if 'open in chrome' should be shown in the tab context menu.
     */
    public boolean shouldShowOpenInChromeMenuItemInContextMenu() {
        return true;
    }

    /**
     * Returns true if 'open in chrome' should be shown in the app menu.
     */
    public boolean shouldShowOpenInChromeMenuItem() {
        return true;
    }

    /**
     * @return Whether the incognito icon in the toolbar should be hidden in cct-incognito mode.
     */
    public boolean shouldHideIncognitoIconOnToolbarInCct() {
        return false;
    }
}

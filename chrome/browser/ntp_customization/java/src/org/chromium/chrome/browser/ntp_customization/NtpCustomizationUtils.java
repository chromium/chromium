// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MVT;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.SINGLE_THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.CHROME_COLOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO_FOR_DAILY_REFRESH;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_INFO;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_INFO_FOR_DAILY_REFRESH;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_INFO;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_INFO_FOR_DAILY_REFRESH;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_CHROME_COLOR_DAILY_REFRESH_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_MAIN_BOTTOM_SHEET_SHOWN;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR_FOR_DAILY_REFRESH;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID;
import static org.chromium.components.browser_ui.styles.SemanticColorUtils.getDefaultIconColor;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Point;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageButton;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.core.content.ContextCompat;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;

import com.google.android.material.color.DynamicColors;
import com.google.android.material.color.DynamicColorsOptions;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.policy.NtpCustomizationPolicyManager;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorFromHexInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorUtils;
import org.chromium.chrome.browser.ntp_customization.theme.daily_refresh.NtpThemeDailyRefreshManager;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.CropImageUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.Executor;

/** Utility class of the NTP customization. */
@NullMarked
public class NtpCustomizationUtils {

    // LINT.IfChange(NtpBackgroundType)
    @IntDef({
        NtpBackgroundType.DEFAULT,
        NtpBackgroundType.IMAGE_FROM_DISK,
        NtpBackgroundType.CHROME_COLOR,
        NtpBackgroundType.THEME_COLLECTION,
        NtpBackgroundType.COLOR_FROM_HEX
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NtpBackgroundType {
        int DEFAULT = 0;
        int IMAGE_FROM_DISK = 1;
        int CHROME_COLOR = 2;
        int THEME_COLLECTION = 3;
        int COLOR_FROM_HEX = 4;
        int NUM_ENTRIES = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:NtpBackgroundType)

    /** An interface to get the current NTP's theme color. */
    interface PrimaryColorProvider {
        // Returns the current primary theme color if exists.
        @Nullable
        @ColorInt
        Integer getPrimaryColor();
    }

    /** The time duration limit to refresh NTP's background. */
    @VisibleForTesting
    static final long DEFAULT_DAILY_REFRESH_HOURS_MS = TimeUtils.MILLISECONDS_PER_DAY;

    @VisibleForTesting static final String NTP_BACKGROUND_IMAGE_FILE = "ntp_background_image";

    @VisibleForTesting
    static final String NTP_BACKGROUND_IMAGE_FILE_FOR_DAILY_REFRESH =
            "ntp_background_image_for_daily_refresh";

    private static final int MAX_IMAGE_SIZE = 2556;
    private static final int IMAGE_SIZE_FOR_EXTRACTING_COLOR = 100;
    private static final String TAG = "NtpCustomization";
    private static final String DELIMITER = "|";
    private static final int CUSTOM_BACKGROUND_INFO_NUM_FIELDS = 4;
    private static @Nullable ImageFetcher sImageFetcherForTesting;

    /**
     * Every list in NTP customization bottom sheets should use this function to get the background
     * for its list items.
     *
     * @param size The number of the list items to be displayed in a container view.
     * @param index The index of the currently iterated list item.
     * @return The background of the list item view at the given index.
     */
    public static int getBackground(int size, int index) {
        if (size == 1) {
            return R.drawable.ntp_customization_bottom_sheet_list_item_background_single;
        }

        if (index == 0) {
            return R.drawable.ntp_customization_bottom_sheet_list_item_background_top;
        }

        if (index == size - 1) {
            return R.drawable.ntp_customization_bottom_sheet_list_item_background_bottom;
        }

        return R.drawable.ntp_customization_bottom_sheet_list_item_background_middle;
    }

    /**
     * Returns the resource ID of the content description for the bottom sheet. The main bottom
     * sheet's content description requires special handling beyond this function.
     */
    public static int getSheetContentDescription(
            @NtpCustomizationCoordinator.BottomSheetType int type) {
        switch (type) {
            case MAIN:
                return R.string.ntp_customization_main_bottom_sheet;
            case MVT:
                return R.string.ntp_customization_mvt_bottom_sheet;
            case NTP_CARDS:
                return R.string.ntp_customization_ntp_cards_bottom_sheet;
            case FEED:
                return R.string.ntp_customization_feed_bottom_sheet;
            case THEME:
                return R.string.ntp_customization_theme_bottom_sheet;
            case THEME_COLLECTIONS:
            case SINGLE_THEME_COLLECTION:
                return R.string.ntp_customization_theme_collections_bottom_sheet;
            case CHROME_COLORS:
                return R.string.ntp_customization_chrome_colors_bottom_sheet;
            default:
                assert false : "Bottom sheet type not supported!";
                return -1;
        }
    }

    /**
     * Returns the resource ID for the accessibility string announced when the bottom sheet is fully
     * expanded.
     */
    public static int getSheetFullHeightAccessibilityStringId(
            @NtpCustomizationCoordinator.BottomSheetType int type) {
        switch (type) {
            case MAIN:
                return R.string.ntp_customization_main_bottom_sheet_opened_full;
            case MVT:
                return R.string.ntp_customization_mvt_bottom_sheet_opened_full;
            case NTP_CARDS:
                return R.string.ntp_customization_ntp_cards_bottom_sheet_opened_full;
            case FEED:
                return R.string.ntp_customization_feed_bottom_sheet_opened_full;
            case THEME:
                return R.string.ntp_customization_theme_bottom_sheet_opened_full;
            case THEME_COLLECTIONS:
            case SINGLE_THEME_COLLECTION:
                return R.string.ntp_customization_theme_collections_bottom_sheet_opened_full;
            case CHROME_COLORS:
                return R.string.ntp_customization_chrome_colors_bottom_sheet_opened_full;
            default:
                assert false : "Bottom sheet type not supported!";
                return -1;
        }
    }

    /**
     * Returns the resource ID for the accessibility string announced when the bottom sheet is half
     * expanded.
     */
    public static int getSheetHalfHeightAccessibilityStringId(
            @NtpCustomizationCoordinator.BottomSheetType int type) {
        switch (type) {
            case MAIN:
                return R.string.ntp_customization_main_bottom_sheet_opened_half;
            case MVT:
                return R.string.ntp_customization_mvt_bottom_sheet_opened_half;
            case NTP_CARDS:
                return R.string.ntp_customization_ntp_cards_bottom_sheet_opened_half;
            case FEED:
                return R.string.ntp_customization_feed_bottom_sheet_opened_half;
            case THEME:
                return R.string.ntp_customization_theme_bottom_sheet_opened_half;
            case THEME_COLLECTIONS:
            case SINGLE_THEME_COLLECTION:
                return R.string.ntp_customization_theme_collections_bottom_sheet_opened_half;
            case CHROME_COLORS:
                return R.string.ntp_customization_chrome_colors_bottom_sheet_opened_half;
            default:
                assert false : "Bottom sheet type not supported!";
                return -1;
        }
    }

    /** Returns whether custom Ntp's background theme is enabled. */
    public static boolean isNtpThemeCustomizationEnabled() {
        return ChromeFeatureList.sNewTabPageCustomizationV2.isEnabled()
                && NtpCustomizationPolicyManager.getInstance().isNtpCustomBackgroundEnabled();
    }

    /**
     * Returns the customized primary color if set, null otherwise.
     *
     * @param context The application context to get themed colors.
     * @param checkDailyRefresh Whether to check daily update when getting the primiary color.
     */
    public @Nullable static @ColorInt Integer getPrimaryColorFromCustomizedThemeColor(
            Context context, boolean checkDailyRefresh) {
        @NtpBackgroundType int imageType = getNtpBackgroundType();
        if (imageType == NtpBackgroundType.DEFAULT) {
            return null;
        }

        NtpThemeDailyRefreshManager ntpThemeDailyRefreshManager =
                NtpThemeDailyRefreshManager.getInstance();
        @ColorInt int color;
        if (imageType == NtpBackgroundType.CHROME_COLOR) {
            @NtpThemeColorId
            int colorId = ntpThemeDailyRefreshManager.getNtpThemeColorIdForChromeColorTheme();
            if (colorId <= NtpThemeColorId.DEFAULT || colorId >= NtpThemeColorId.NUM_ENTRIES) {
                return null;
            }
            if (checkDailyRefresh) {
                colorId = ntpThemeDailyRefreshManager.maybeApplyDailyRefreshForChromeColor(colorId);
            }
            return context.getColor(NtpThemeColorUtils.getNtpThemePrimaryColorResId(colorId));
        } else if (imageType == NtpBackgroundType.THEME_COLLECTION) {
            if (checkDailyRefresh) {
                ntpThemeDailyRefreshManager.maybeApplyDailyRefreshForThemeCollection();
            }
            color = ntpThemeDailyRefreshManager.getNtpThemeColorForThemeCollection();
        } else {
            color = getCustomizedPrimaryColorFromSharedPreference();
        }

        return (color != NtpThemeColorInfo.COLOR_NOT_SET) ? color : null;
    }

    /**
     * Applies the primary color to the given activity using DynamicColors API.
     *
     * @param activity The Activity instance to apply the new primary theme color.
     * @param primaryColor The primary theme color to apply.
     */
    public static void applyDynamicColorToActivity(Activity activity, @ColorInt int primaryColor) {
        DynamicColorsOptions.Builder builder = new DynamicColorsOptions.Builder();
        builder.setContentBasedSource(primaryColor);
        DynamicColorsOptions dynamicColorsOptions = builder.build();
        DynamicColors.applyToActivityIfAvailable(activity, dynamicColorsOptions);
    }

    /** Loads the NtpThemeColorInfo from the SharedPreference, null otherwise. */
    public @Nullable static NtpThemeColorInfo loadColorInfoFromSharedPreference(Context context) {
        if (!NtpCustomizationUtils.isNtpThemeCustomizationEnabled()) return null;

        @NtpBackgroundType int imageType = getNtpBackgroundTypeFromSharedPreference();
        if (imageType != NtpBackgroundType.CHROME_COLOR
                && imageType != NtpBackgroundType.COLOR_FROM_HEX) {
            return null;
        }

        if (imageType == NtpBackgroundType.CHROME_COLOR) {
            // For CHROME_COLOR, a color resource id is saved in the SharedPreference.
            @NtpThemeColorId int colorId = getNtpThemeColorIdFromSharedPreference();
            if (colorId == NtpThemeColorId.DEFAULT) return null;

            return NtpThemeColorUtils.createNtpThemeColorInfo(context, colorId);
        }

        // For other types, a color value is saved in the SharedPreference.
        @ColorInt int primaryColor = getCustomizedPrimaryColorFromSharedPreference();
        if (primaryColor == NtpThemeColorInfo.COLOR_NOT_SET) return null;

        @ColorInt int backgroundColor = NtpThemeColorInfo.COLOR_NOT_SET;
        if (imageType == NtpBackgroundType.COLOR_FROM_HEX) {
            backgroundColor =
                    getBackgroundColorFromSharedPreference(NtpThemeColorInfo.COLOR_NOT_SET);
        }
        return new NtpThemeColorFromHexInfo(context, backgroundColor, primaryColor);
    }

    // Gets the content based primary color for a bitmap.
    public @Nullable static @ColorInt Integer getContentBasedSeedColor(Bitmap bitmap) {
        // Resize the bitmap to a smaller size to avoid OOM errors and improve performance
        // when extracting the color. A 100x100 image is sufficient for color extraction.
        // This matches the behavior in
        // NtpCustomBackgroundService::UpdateCustomLocalBackgroundColorAsync.
        Bitmap scaledBitmap = bitmap;
        if (bitmap.getWidth() > IMAGE_SIZE_FOR_EXTRACTING_COLOR
                || bitmap.getHeight() > IMAGE_SIZE_FOR_EXTRACTING_COLOR) {
            scaledBitmap =
                    Bitmap.createScaledBitmap(
                            bitmap,
                            IMAGE_SIZE_FOR_EXTRACTING_COLOR,
                            IMAGE_SIZE_FOR_EXTRACTING_COLOR,
                            /* filter= */ true);
        }

        DynamicColorsOptions.Builder builder = new DynamicColorsOptions.Builder();
        builder.setContentBasedSource(scaledBitmap);
        DynamicColorsOptions dynamicColorsOptions = builder.build();
        return dynamicColorsOptions.getContentBasedSeedColor();
    }

    // Launch a new activity in the same task with the given uri as a CCT.
    public static void launchUriActivity(Context context, String uri) {
        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.setShareState(CustomTabsIntent.SHARE_STATE_ON);
        Intent intent = builder.build().intent;
        intent.setPackage(context.getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        intent.setData(Uri.parse(uri));
        intent.setAction(Intent.ACTION_VIEW);
        intent.setClassName(context, "org.chromium.chrome.browser.customtabs.CustomTabActivity");
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        context.startActivity(intent);
    }

    /**
     * Sets the NTP's background image type to the SharedPreference.
     *
     * @param imageType The new image type.
     */
    public static void setNtpBackgroundTypeToSharedPreference(@NtpBackgroundType int imageType) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE, imageType);
    }

    /** Gets the current NTP's background type from the SharedPreference. */
    public static @NtpBackgroundType int getNtpBackgroundTypeFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE, NtpBackgroundType.DEFAULT);
    }

    /** Removes the NTP's background image type from the SharedPreference. */
    public static void removeNtpBackgroundTypeFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE);
    }

    /**
     * Gets the current NTP's background type from the SharedPreference. Returns
     * NtpBackgroundType.DEFAULT if the feature flag is disabled.
     */
    public static @NtpBackgroundType int getNtpBackgroundType() {
        if (!isNtpThemeCustomizationEnabled()) {
            return NtpBackgroundType.DEFAULT;
        }

        return getNtpBackgroundTypeFromSharedPreference();
    }

    /**
     * Saves the background image.
     *
     * @param backgroundImageBitmap The bitmap of the background image.
     */
    @VisibleForTesting
    static void saveBackgroundImageFile(@Nullable Bitmap backgroundImageBitmap) {
        File file = createBackgroundImageFile();
        saveBitmapImageToFile(backgroundImageBitmap, file);
    }

    /**
     * Saves the daily refresh background image.
     *
     * @param backgroundImageBitmap The bitmap of the daily refresh background image.
     */
    @VisibleForTesting
    static void saveDailyRefreshBackgroundImageFile(@Nullable Bitmap backgroundImageBitmap) {
        File file = createDailyRefreshBackgroundImageFile();
        saveBitmapImageToFile(backgroundImageBitmap, file);
    }

    /**
     * Sets whether the NTP customization bottom sheet has shown.
     *
     * @param hasShown Whether the bottom sheet has shown.
     */
    public static void setNtpCustomizationBottomSheetShownToSharedPreferences(boolean hasShown) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeBoolean(NTP_CUSTOMIZATION_MAIN_BOTTOM_SHEET_SHOWN, hasShown);
    }

    /** Gets whether the NTP customization bottom sheet has shown. */
    public static boolean getNtpCustomizationBottomSheetShownFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readBoolean(NTP_CUSTOMIZATION_MAIN_BOTTOM_SHEET_SHOWN, false);
    }

    /**
     * Saves the background image if it isn't null, otherwise removes the file.
     *
     * @param backgroundImageBitmap The bitmap of the background image.
     * @param file The file to save the image to.
     */
    @VisibleForTesting
    public static void saveBitmapImageToFile(@Nullable Bitmap backgroundImageBitmap, File file) {
        if (backgroundImageBitmap == null) {
            deleteBackgroundImageFile(file);
            return;
        }

        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                saveBitmapImageToFileImpl(backgroundImageBitmap, file);
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Saves the background transformation matrices to SharedPreferences.
     *
     * @param backgroundImageInfo The {@link BackgroundImageInfo} object containing the portrait and
     *     landscape matrices.
     */
    @VisibleForTesting
    public static void updateBackgroundImageInfo(BackgroundImageInfo backgroundImageInfo) {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();

        prefs.writeString(
                NTP_BACKGROUND_IMAGE_PORTRAIT_INFO, backgroundImageInfo.getPortraitInfoString());
        prefs.writeString(
                NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO, backgroundImageInfo.getLandscapeInfoString());
    }

    /**
     * Saves the background transformation matrices to SharedPreferences for daily refresh.
     *
     * @param backgroundImageInfo The {@link BackgroundImageInfo} object containing the portrait and
     *     landscape matrices.
     */
    @VisibleForTesting
    public static void updateDailyRefreshBackgroundImageInfo(
            BackgroundImageInfo backgroundImageInfo) {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();

        prefs.writeString(
                NTP_BACKGROUND_IMAGE_PORTRAIT_INFO_FOR_DAILY_REFRESH,
                backgroundImageInfo.getPortraitInfoString());
        prefs.writeString(
                NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO_FOR_DAILY_REFRESH,
                backgroundImageInfo.getLandscapeInfoString());
    }

    /** Returns whether a white background should be applied on fake search box. */
    public static boolean shouldApplyWhiteBackgroundOnSearchBox() {
        if (!NtpCustomizationUtils.isNtpThemeCustomizationEnabled()) return false;

        return shouldApplyWhiteBackgroundOnSearchBox(
                NtpCustomizationConfigManager.getInstance().getBackgroundType());
    }

    /**
     * Returns whether a white background should be applied on fake search box based on the provided
     * background image type.
     */
    public static boolean shouldApplyWhiteBackgroundOnSearchBox(@NtpBackgroundType int type) {
        return type == NtpBackgroundType.IMAGE_FROM_DISK
                || type == NtpBackgroundType.THEME_COLLECTION;
    }

    /**
     * Returns whether the edit icon should use a grey background on LFF devices. This is true if
     * the current background image type is theme collection or uploaded image.
     */
    @VisibleForTesting
    static boolean shouldUseEditIconWithGreyBackground() {
        return shouldApplyWhiteBackgroundOnSearchBox();
    }

    /**
     * Saves the background image bitmap to the specified file.
     *
     * @param backgroundImageBitmap The bitmap to save.
     * @param file The file to save the image to.
     */
    private static void saveBitmapImageToFileImpl(Bitmap backgroundImageBitmap, File file) {
        try (FileOutputStream fileOutputStream = new FileOutputStream(file)) {
            backgroundImageBitmap.compress(Bitmap.CompressFormat.PNG, 100, fileOutputStream);
        } catch (IOException e) {
            Log.i(TAG, "Failed to save background image to: " + file.getAbsolutePath());
        }
    }

    /** Returns the file to save the NTP's background image. */
    @VisibleForTesting
    public static File createBackgroundImageFile() {
        return new File(
                ContextUtils.getApplicationContext().getFilesDir(), NTP_BACKGROUND_IMAGE_FILE);
    }

    /** Returns the file to save the NTP's daily refresh background image. */
    @VisibleForTesting
    public static File createDailyRefreshBackgroundImageFile() {
        return new File(
                ContextUtils.getApplicationContext().getFilesDir(),
                NTP_BACKGROUND_IMAGE_FILE_FOR_DAILY_REFRESH);
    }

    /** Deletes the background image file from disk. */
    @VisibleForTesting
    static void deleteBackgroundImageFile(File file) {
        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                deleteBackgroundImageFileImpl(file);
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Deletes the given file from disk.
     *
     * @param file The file to be deleted.
     */
    @VisibleForTesting
    static void deleteBackgroundImageFileImpl(File file) {
        if (file.exists()) {
            file.delete();
        }
    }

    /**
     * Loads the NTP's background bitmap image from disk.
     *
     * @param callback The callback to notice when the image is loaded.
     * @param executor The executor for the loading task.
     */
    public static void readNtpBackgroundImage(
            Callback<@Nullable Bitmap> callback, Executor executor) {
        readNtpBackgroundImageFromFile(callback, executor, createBackgroundImageFile());
    }

    /**
     * Loads the NTP's daily refresh background bitmap image from disk.
     *
     * @param callback The callback to notice when the image is loaded.
     * @param executor The executor for the loading task.
     */
    public static void readDailyRefreshNtpBackgroundImage(
            Callback<@Nullable Bitmap> callback, Executor executor) {
        readNtpBackgroundImageFromFile(callback, executor, createDailyRefreshBackgroundImageFile());
    }

    /**
     * Loads a background bitmap image from a given file.
     *
     * @param callback The callback to notice when the image is loaded.
     * @param executor The executor for the loading task.
     * @param file The file to read the image from.
     */
    private static void readNtpBackgroundImageFromFile(
            Callback<@Nullable Bitmap> callback, Executor executor, File file) {
        new AsyncTask<Bitmap>() {
            @Override
            // The return value of the super class doesn't have @Nullable annotation.
            @SuppressWarnings("NullAway")
            protected Bitmap doInBackground() {
                return readNtpBackgroundImageImpl(file);
            }

            @Override
            protected void onPostExecute(Bitmap bitmap) {
                if (bitmap == null) {
                    callback.onResult(null);
                    return;
                }
                callback.onResult(bitmap);
            }
        }.executeOnExecutor(executor);
    }

    /**
     * Reads and decodes a bitmap from the specified file.
     *
     * @param file The file to read from.
     */
    @VisibleForTesting
    static @Nullable Bitmap readNtpBackgroundImageImpl(File file) {
        if (!file.exists()) {
            return null;
        }

        return BitmapFactory.decodeFile(file.getPath(), null);
    }

    /** Loads the NTP's background transformation matrices from SharedPreferences. */
    public static @Nullable BackgroundImageInfo readNtpBackgroundImageInfo() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();

        String portraitInfoString = prefs.readString(NTP_BACKGROUND_IMAGE_PORTRAIT_INFO, null);
        String landscapeInfoString = prefs.readString(NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO, null);

        return BackgroundImageInfo.createFromStrings(portraitInfoString, landscapeInfoString);
    }

    /**
     * Loads the background transformation matrices for the daily refresh image from
     * SharedPreferences.
     */
    public static @Nullable BackgroundImageInfo readDailyRefreshNtpBackgroundImageInfo() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();

        String portraitInfoString =
                prefs.readString(NTP_BACKGROUND_IMAGE_PORTRAIT_INFO_FOR_DAILY_REFRESH, null);
        String landscapeInfoString =
                prefs.readString(NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO_FOR_DAILY_REFRESH, null);

        return BackgroundImageInfo.createFromStrings(portraitInfoString, landscapeInfoString);
    }

    /**
     * Removes the background transformation matrices for the daily refresh image from
     * SharedPreferences.
     */
    public static void removeDailyRefreshNtpBackgroundImageInfo() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.removeKey(NTP_BACKGROUND_IMAGE_PORTRAIT_INFO_FOR_DAILY_REFRESH);
        prefs.removeKey(NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO_FOR_DAILY_REFRESH);
    }

    /**
     * Sets the NTP's background color to the SharedPreference.
     *
     * @param color The new background color.
     */
    public static void setBackgroundColorToSharedPreference(@ColorInt int color) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR, color);
    }

    /** Gets the NTP's background color from the SharedPreference. */
    public static @ColorInt int getBackgroundColorFromSharedPreference(@ColorInt int defaultColor) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR, defaultColor);
    }

    /**
     * Sets the NTP's color theme id to the SharedPreference.
     *
     * @param themeColorId The new color theme id.
     */
    public static void setNtpThemeColorIdToSharedPreference(@NtpThemeColorId int themeColorId) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID, themeColorId);
    }

    /** Gets the NTP's color theme id from the SharedPreference. */
    public static @NtpThemeColorId int getNtpThemeColorIdFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID, NtpThemeColorId.DEFAULT);
    }

    /**
     * Picks the primary color for the bitmap and saves it to the SharedPreference.
     *
     * @param bitmap The bitmap from which to extract and save the primary color.
     */
    static void pickAndSavePrimaryColor(Bitmap bitmap) {
        @ColorInt Integer primaryColor = getContentBasedSeedColor(bitmap);
        if (primaryColor != null) {
            setCustomizedPrimaryColorToSharedPreference(primaryColor.intValue());
        } else {
            removeCustomizedPrimaryColorFromSharedPreference();
        }
    }

    /**
     * Picks the primary color for the daily refresh bitmap and saves it to SharedPreferences.
     *
     * @param bitmap The bitmap from which to extract and save the primary color.
     */
    static void pickAndSaveDailyRefreshPrimaryColor(Bitmap bitmap) {
        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                @ColorInt Integer primaryColor = getContentBasedSeedColor(bitmap);
                if (primaryColor != null) {
                    setDailyRefreshCustomizedPrimaryColorToSharedPreference(
                            primaryColor.intValue());
                } else {
                    removeDailyRefreshCustomizedPrimaryColorFromSharedPreference();
                }
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Sets the customized primary color to the SharedPreference.
     *
     * @param color The new primary theme color.
     */
    public static void setCustomizedPrimaryColorToSharedPreference(@ColorInt int color) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(NTP_CUSTOMIZATION_PRIMARY_COLOR, color);
    }

    /**
     * Sets the customized primary color for daily refresh to SharedPreferences.
     *
     * @param color The new primary theme color for daily refresh.
     */
    public static void setDailyRefreshCustomizedPrimaryColorToSharedPreference(
            @ColorInt int color) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(NTP_CUSTOMIZATION_PRIMARY_COLOR_FOR_DAILY_REFRESH, color);
    }

    /** Gets the customized primary color from the SharedPreference. */
    public static @ColorInt int getCustomizedPrimaryColorFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readInt(
                NTP_CUSTOMIZATION_PRIMARY_COLOR, NtpThemeColorInfo.COLOR_NOT_SET);
    }

    /** Gets the customized primary color for daily refresh from SharedPreferences. */
    public static @ColorInt int getDailyRefreshCustomizedPrimaryColorFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readInt(
                NTP_CUSTOMIZATION_PRIMARY_COLOR_FOR_DAILY_REFRESH, NtpThemeColorInfo.COLOR_NOT_SET);
    }

    /** Removes the customized primary color from the SharedPreference. */
    public static void removeCustomizedPrimaryColorFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(NTP_CUSTOMIZATION_PRIMARY_COLOR);
    }

    /** Removes the customized primary color for daily refresh from SharedPreferences. */
    public static void removeDailyRefreshCustomizedPrimaryColorFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(NTP_CUSTOMIZATION_PRIMARY_COLOR_FOR_DAILY_REFRESH);
    }

    /**
     * Updates the daily refresh timestamp if enabled.
     *
     * @param timestamp The new timestamp.
     */
    public static void maybeUpdateDailyRefreshTimestamp(
            long timestamp,
            @NtpBackgroundType int backgroundType,
            @Nullable CustomBackgroundInfo customBackgroundInfo) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();

        if (backgroundType == NtpBackgroundType.CHROME_COLOR) {
            if (!prefsManager.readBoolean(
                    NTP_CUSTOMIZATION_CHROME_COLOR_DAILY_REFRESH_ENABLED, false)) {
                return;
            }
        }

        if (backgroundType == NtpBackgroundType.THEME_COLLECTION) {
            if (customBackgroundInfo == null || !customBackgroundInfo.isDailyRefreshEnabled) {
                return;
            }
        }

        prefsManager.writeLong(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP, timestamp);
    }

    /**
     * Sets whether daily refresh for Chrome Color is enabled to the SharedPreference.
     *
     * @param enabled Whether daily refresh should be enabled.
     */
    public static void setIsChromeColorDailyRefreshEnabledToSharedPreference(boolean enabled) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeBoolean(NTP_CUSTOMIZATION_CHROME_COLOR_DAILY_REFRESH_ENABLED, enabled);
    }

    /** Gets whether daily refresh for Chrome Color is enabled from the SharedPreference. */
    public static boolean getIsChromeColorDailyRefreshEnabledFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readBoolean(
                NTP_CUSTOMIZATION_CHROME_COLOR_DAILY_REFRESH_ENABLED, false);
    }

    /**
     * Sets the timestamp of the last time when a daily refreshed theme color or background image
     * was set.
     */
    @VisibleForTesting
    public static void setDailyRefreshTimestampToSharedPreference(long timestamp) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeLong(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP, timestamp);
    }

    /**
     * Gets the timestamp of the last time when a daily refreshed theme color or background image
     * was set.
     */
    @VisibleForTesting
    public static long getDailyRefreshTimestampToSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readLong(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP, 0);
    }

    /** Removes the NTP's background color key and primary color key from the SharedPreference. */
    static void resetCustomizedColors() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID);
        prefsManager.removeKey(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_CHROME_COLOR_DAILY_REFRESH_ENABLED);
    }

    /** Removes the NTP's background image related keys from the SharedPreference */
    static void resetCustomizedImage() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(NTP_CUSTOMIZATION_PRIMARY_COLOR);
        prefsManager.removeKey(NTP_CUSTOMIZATION_PRIMARY_COLOR_FOR_DAILY_REFRESH);
        prefsManager.removeKey(NTP_CUSTOMIZATION_BACKGROUND_INFO);
        prefsManager.removeKey(NTP_CUSTOMIZATION_BACKGROUND_INFO_FOR_DAILY_REFRESH);
        prefsManager.removeKey(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP);
        prefsManager.removeKey(NTP_BACKGROUND_IMAGE_PORTRAIT_INFO);
        prefsManager.removeKey(NTP_BACKGROUND_IMAGE_PORTRAIT_INFO_FOR_DAILY_REFRESH);
        prefsManager.removeKey(NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO);
        prefsManager.removeKey(NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO_FOR_DAILY_REFRESH);
        deleteBackgroundImageFile(createBackgroundImageFile());
        deleteBackgroundImageFile(createDailyRefreshBackgroundImageFile());
    }

    /** Removes all NTP custom background related data. */
    public static void resetNtpCustomBackgroundData() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        if (!prefsManager.contains(NTP_CUSTOMIZATION_BACKGROUND_TYPE)) {
            // If the no data has been cached or has been cleaned up before, exits here.
            return;
        }

        @NtpBackgroundType int type = prefsManager.readInt(NTP_CUSTOMIZATION_BACKGROUND_TYPE);
        removeNtpBackgroundTypeFromSharedPreference();
        switch (type) {
            case CHROME_COLOR -> resetCustomizedColors();
            case IMAGE_FROM_DISK, THEME_COLLECTION -> resetCustomizedImage();
        }
    }

    /** Returns whether all flags are enabled to allow edge-to-edge for customized theme. */
    public static boolean canEnableEdgeToEdgeForCustomizedTheme(
            WindowAndroid windowAndroid, boolean isTablet) {
        return canEnableEdgeToEdgeForCustomizedTheme(isTablet)
                && EdgeToEdgeStateProvider.isEdgeToEdgeEnabledForWindow(windowAndroid);
    }

    /**
     * Returns whether all flags are enabled to allow edge-to-edge for customized theme. This method
     * doesn't check EdgeToEdgeStateProvider.
     */
    public static boolean canEnableEdgeToEdgeForCustomizedTheme(boolean isTablet) {
        return !isTablet
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && NtpCustomizationUtils.isNtpThemeCustomizationEnabled();
    }

    /**
     * Returns whether the given Tab supports to remove the top Status bar to make it truly edge to
     * edge.
     */
    public static boolean supportsEnableEdgeToEdgeOnTop(@Nullable Tab tab) {
        if (tab == null || !tab.isNativePage()) {
            return false;
        }

        return assumeNonNull(tab.getNativePage()).supportsEdgeToEdgeOnTop();
    }

    /**
     * Returns whether to skip a layout change from the given systemTopInset and consumeTopInset
     * status.
     *
     * @param appliedTopPadding The value of currently applied top padding.
     * @param systemTopInset The system's top inset, i.e., the height of Status bar.
     * @param consumeTopInset Whether should consume the system's top inset.
     */
    public static boolean shouldSkipTopInsetsChange(
            int appliedTopPadding, int systemTopInset, boolean consumeTopInset) {
        // We skip a layout change if the top padding doesn't need adjusting. This occurs in two
        // scenarios:
        // 1) Top padding was already added and should remain.
        // 2) No top padding was added and none is needed now.
        return ((appliedTopPadding == systemTopInset) && consumeTopInset)
                || ((appliedTopPadding == 0) && !consumeTopInset);
    }

    /**
     * Sets tint color for the default Google logo.
     *
     * @param context Used to look up current day/night mode status.
     * @param defaultGoogleLogoDrawable The drawable instance for default Google Logo.
     */
    public static void setTintForDefaultGoogleLogo(
            Context context, @Nullable Drawable defaultGoogleLogoDrawable) {
        if (defaultGoogleLogoDrawable == null) {
            return;
        }

        @NtpBackgroundType
        int backgroundType = NtpCustomizationConfigManager.getInstance().getBackgroundType();
        getTintedGoogleLogoDrawableImpl(
                context,
                defaultGoogleLogoDrawable,
                backgroundType,
                () ->
                        NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(
                                context, /* checkDailyRefresh= */ false));
    }

    /**
     * Returns the default Google logo with tint.
     *
     * @param context Used to look up current day/night mode status.
     * @param defaultGoogleLogoDrawable The drawable instance for default Google Logo.
     * @param backgroundType backgroundType The NTP's background theme type.
     * @param primaryColor The primary theme color.
     */
    public static Drawable getTintedGoogleLogoDrawableImpl(
            Context context,
            Drawable defaultGoogleLogoDrawable,
            @NtpBackgroundType int backgroundType,
            @Nullable @ColorInt Integer primaryColor) {
        return getTintedGoogleLogoDrawableImpl(
                context, defaultGoogleLogoDrawable, backgroundType, () -> primaryColor);
    }

    /**
     * Returns the default Google logo with tint.
     *
     * @param context Used to look up current day/night mode status.
     * @param defaultGoogleLogoDrawable The drawable instance for default Google Logo.
     * @param backgroundType backgroundType The NTP's background theme type.
     * @param primaryColorProvider The interface to get primary theme color.
     */
    private static Drawable getTintedGoogleLogoDrawableImpl(
            Context context,
            Drawable defaultGoogleLogoDrawable,
            @NtpBackgroundType int backgroundType,
            PrimaryColorProvider primaryColorProvider) {
        // Check the mode before applying a tinted color. A transparent tint in light mode will
        // cause the logo's color to disappear.
        boolean isNightMode = ColorUtils.inNightMode(context);
        // The colorful Google logo is shown for default theme in light mode.
        if (!isNightMode && backgroundType == NtpBackgroundType.DEFAULT) {
            return defaultGoogleLogoDrawable;
        }

        @ColorInt int tintColor;
        if (backgroundType == NtpBackgroundType.CHROME_COLOR
                || backgroundType == NtpBackgroundType.COLOR_FROM_HEX) {
            @Nullable
            @ColorInt
            Integer primaryColor = primaryColorProvider.getPrimaryColor();
            if (primaryColor != null) {
                tintColor = primaryColor.intValue();
            } else if (!isNightMode) {
                // When primary color is missing, falls back to colorful Google logo in light mode.
                return defaultGoogleLogoDrawable;
            } else {
                // When primary color is missing, falls back to white Google logo in light mode.
                tintColor = Color.WHITE;
            }
        } else {
            // For all other cases, white color is used. This includes: Ntps with a customized
            // background image in either light or dark mode; or Ntps without any theme in dark
            // mode.
            tintColor = Color.WHITE;
        }

        Drawable tintedDrawable = defaultGoogleLogoDrawable.mutate();
        tintedDrawable.setTint(tintColor);
        return tintedDrawable;
    }

    /**
     * Creates an {@link ImageFetcher} for fetching theme collection images.
     *
     * @param profile The profile to create the image fetcher for.
     */
    public static ImageFetcher createImageFetcher(Profile profile) {
        if (sImageFetcherForTesting != null) {
            return sImageFetcherForTesting;
        }

        return ImageFetcherFactory.createImageFetcher(
                ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                profile.getProfileKey(),
                GlobalDiscardableReferencePool.getReferencePool());
    }

    /**
     * Fetches an image for the theme collection.
     *
     * @param imageFetcher The {@link ImageFetcher} to use.
     * @param imageUrl The URL of the image to fetch.
     * @param callback The callback to be invoked with the bitmap.
     */
    public static void fetchThemeCollectionImage(
            ImageFetcher imageFetcher, GURL imageUrl, Callback<@Nullable Bitmap> callback) {
        ImageFetcher.Params params =
                ImageFetcher.Params.create(
                        imageUrl, ImageFetcher.NTP_CUSTOMIZATION_THEME_COLLECTION_NAME);
        imageFetcher.fetchImage(params, callback);
    }

    /**
     * Returns whether it is necessary to apply an adjusted icon tint for NTPs. Returns true if the
     * device is a phone, edge-to-edge is enabled and NTP has a customized background image.
     *
     * @param isTablet Whether the current device is a tablet.
     */
    public static boolean shouldAdjustIconTintForNtp(boolean isTablet) {
        if (!canEnableEdgeToEdgeForCustomizedTheme(isTablet)) return false;

        @NtpBackgroundType
        int backgroundType = NtpCustomizationConfigManager.getInstance().getBackgroundType();
        return backgroundType == NtpBackgroundType.IMAGE_FROM_DISK
                || backgroundType == NtpBackgroundType.THEME_COLLECTION;
    }

    /**
     * Sets the NTP's {@link CustomBackgroundInfo} to the SharedPreference.
     *
     * @param customBackgroundInfo The new {@link CustomBackgroundInfo} which contains the theme
     *     collection info.
     */
    @VisibleForTesting
    public static void setCustomBackgroundInfoToSharedPreference(
            CustomBackgroundInfo customBackgroundInfo) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeString(
                NTP_CUSTOMIZATION_BACKGROUND_INFO,
                customBackgroundInfoToString(customBackgroundInfo));
    }

    /**
     * Sets the NTP's daily refresh {@link CustomBackgroundInfo} to SharedPreferences.
     *
     * @param customBackgroundInfo The new {@link CustomBackgroundInfo} for daily refresh.
     */
    @VisibleForTesting
    public static void setDailyRefreshCustomBackgroundInfoToSharedPreference(
            CustomBackgroundInfo customBackgroundInfo) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeString(
                NTP_CUSTOMIZATION_BACKGROUND_INFO_FOR_DAILY_REFRESH,
                customBackgroundInfoToString(customBackgroundInfo));
    }

    /** Gets the current NTP's {@link CustomBackgroundInfo} from the SharedPreference. */
    public static @Nullable CustomBackgroundInfo getCustomBackgroundInfoFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return stringToCustomBackgroundInfo(
                prefsManager.readString(NTP_CUSTOMIZATION_BACKGROUND_INFO, null));
    }

    /** Gets the current NTP's daily refresh {@link CustomBackgroundInfo} from SharedPreferences. */
    public static @Nullable CustomBackgroundInfo
            getDailyRefreshCustomBackgroundInfoFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return stringToCustomBackgroundInfo(
                prefsManager.readString(NTP_CUSTOMIZATION_BACKGROUND_INFO_FOR_DAILY_REFRESH, null));
    }

    /** Removes the {@link CustomBackgroundInfo} from the SharedPreference. */
    @VisibleForTesting
    static void removeCustomBackgroundInfoFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(NTP_CUSTOMIZATION_BACKGROUND_INFO);
    }

    /** Removes the daily refresh {@link CustomBackgroundInfo} from SharedPreferences. */
    @VisibleForTesting
    static void removeDailyRefreshCustomBackgroundInfoFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(NTP_CUSTOMIZATION_BACKGROUND_INFO_FOR_DAILY_REFRESH);
    }

    /**
     * Converts a {@link CustomBackgroundInfo} object into a string representation for storage.
     * Format: "backgroundUrl|collectionId|isUploadedImage|isDailyRefreshEnabled"
     *
     * @param info The {@link CustomBackgroundInfo} object to convert.
     */
    private static @Nullable String customBackgroundInfoToString(
            @Nullable CustomBackgroundInfo info) {
        if (info == null || info.backgroundUrl == null || info.collectionId == null) {
            return null;
        }

        String backgroundUrlStr = info.backgroundUrl.getPossiblyInvalidSpec();

        return TextUtils.join(
                DELIMITER,
                new String[] {
                    backgroundUrlStr,
                    info.collectionId,
                    String.valueOf(info.isUploadedImage),
                    String.valueOf(info.isDailyRefreshEnabled)
                });
    }

    /**
     * Converts a string representation back into a {@link CustomBackgroundInfo} object. Expects
     * format: "backgroundUrl|collectionId|isUploadedImage|isDailyRefreshEnabled"
     *
     * @param infoString The string to convert.
     */
    private static @Nullable CustomBackgroundInfo stringToCustomBackgroundInfo(
            @Nullable String infoString) {
        if (TextUtils.isEmpty(infoString)) {
            return null;
        }

        String[] parts =
                infoString.split(
                        "\\" + DELIMITER, -1); // Limit -1 to include trailing empty strings
        if (parts.length != CUSTOM_BACKGROUND_INFO_NUM_FIELDS || parts[0].isEmpty()) {
            return null;
        }

        try {
            GURL backgroundUrl = new GURL(parts[0]);
            if (!backgroundUrl.isValid()) {
                return null;
            }

            String collectionId = parts[1];
            boolean isUploadedImage = Boolean.parseBoolean(parts[2]);
            boolean isDailyRefreshEnabled = Boolean.parseBoolean(parts[3]);

            return new CustomBackgroundInfo(
                    backgroundUrl, collectionId, isUploadedImage, isDailyRefreshEnabled);

        } catch (Exception e) {
            Log.i(TAG, "Error parsing CustomBackgroundInfo from string: " + e.getMessage(), e);
            return null;
        }
    }

    /**
     * Generates the default background image information, including center-crop matrices and screen
     * dimensions for both device orientations. The dimensions for the alternate orientation are
     * estimated by swapping the current width and height.
     *
     * @param context The application context to access resources like display metrics.
     * @param bitmap The source bitmap for which the matrices are to be calculated.
     */
    public static BackgroundImageInfo getDefaultBackgroundImageInfo(
            Context context, Bitmap bitmap) {
        Resources resources = context.getResources();
        Point windowSize = CropImageUtils.getCurrentWindowDimensions(context);

        // If the device is portrait, use the current width/height; otherwise, swap them.
        boolean isPortrait =
                resources.getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
        int portraitWidth = isPortrait ? windowSize.x : windowSize.y;
        int portraitHeight = isPortrait ? windowSize.y : windowSize.x;

        Matrix portraitMatrix = new Matrix();
        CropImageUtils.calculateInitialCenterCropMatrix(
                portraitMatrix, portraitWidth, portraitHeight, bitmap);

        // The landscape dimensions are the inverse of the portrait dimensions.
        // Calculate the landscape matrix using these swapped values.
        Matrix landscapeMatrix = new Matrix();
        CropImageUtils.calculateInitialCenterCropMatrix(
                landscapeMatrix, portraitHeight, portraitWidth, bitmap);

        return new BackgroundImageInfo(
                portraitMatrix,
                landscapeMatrix,
                new Point(portraitWidth, portraitHeight),
                new Point(portraitHeight, portraitWidth));
    }

    /**
     * Updates the necessary preferences and files for theme collection image or user uploaded
     * image.
     *
     * @param customBackgroundInfo The {@link CustomBackgroundInfo} containing the theme collection
     *     info if passed in a theme collection image.
     * @param bitmap The bitmap of the theme collection or uploaded image.
     * @param backgroundImageInfo The {@link BackgroundImageInfo} containing the portrait and
     *     landscape transformation matrices of the image.
     * @param skipSavingPrimaryColor True if color selection and saving are deferred until the
     *     bottom sheet is dismissed.
     */
    public static void saveBackgroundInfo(
            @Nullable CustomBackgroundInfo customBackgroundInfo,
            Bitmap bitmap,
            BackgroundImageInfo backgroundImageInfo,
            boolean skipSavingPrimaryColor) {
        saveBackgroundImageFile(bitmap);

        if (customBackgroundInfo != null) {
            setCustomBackgroundInfoToSharedPreference(customBackgroundInfo);
        } else {
            removeCustomBackgroundInfoFromSharedPreference();
        }

        if (!skipSavingPrimaryColor) {
            pickAndSavePrimaryColor(bitmap);
        }

        updateBackgroundImageInfo(backgroundImageInfo);
    }

    /**
     * Updates the necessary preferences and files for daily refresh of theme collection image.
     *
     * @param customBackgroundInfo The {@link CustomBackgroundInfo} containing the theme collection
     *     info if passed in a theme collection image.
     * @param bitmap The bitmap of the theme collection or uploaded image.
     * @param backgroundImageInfo The {@link BackgroundImageInfo} containing the portrait and
     *     landscape transformation matrices of the image.
     */
    public static void saveDailyRefreshBackgroundInfo(
            @Nullable CustomBackgroundInfo customBackgroundInfo,
            Bitmap bitmap,
            BackgroundImageInfo backgroundImageInfo) {
        saveDailyRefreshBackgroundImageFile(bitmap);

        if (customBackgroundInfo != null) {
            setDailyRefreshCustomBackgroundInfoToSharedPreference(customBackgroundInfo);
        } else {
            removeDailyRefreshCustomBackgroundInfoFromSharedPreference();
        }

        pickAndSaveDailyRefreshPrimaryColor(bitmap);

        updateDailyRefreshBackgroundImageInfo(backgroundImageInfo);
    }

    /**
     * Applies the daily refresh theme collection image by overwriting the current theme collection
     * image settings with the daily refresh settings. This includes updating SharedPreferences and
     * renaming the background image file.
     */
    public static void commitThemeCollectionDailyRefresh() {
        // 1. Overwrite current theme collection image info with daily refresh image info in
        // SharedPreferences.
        BackgroundImageInfo dailyRefreshNtpBackgroundImageInfo =
                readDailyRefreshNtpBackgroundImageInfo();
        if (dailyRefreshNtpBackgroundImageInfo != null) {
            updateBackgroundImageInfo(dailyRefreshNtpBackgroundImageInfo);
        }
        setCustomizedPrimaryColorToSharedPreference(
                getDailyRefreshCustomizedPrimaryColorFromSharedPreference());
        CustomBackgroundInfo dailyRefreshCustomBackgroundInfo =
                getDailyRefreshCustomBackgroundInfoFromSharedPreference();
        if (dailyRefreshCustomBackgroundInfo != null) {
            setCustomBackgroundInfoToSharedPreference(dailyRefreshCustomBackgroundInfo);
        }

        // 2. Remove the daily refresh info from SharedPreferences.
        removeDailyRefreshNtpBackgroundImageInfo();
        removeDailyRefreshCustomizedPrimaryColorFromSharedPreference();
        removeDailyRefreshCustomBackgroundInfoFromSharedPreference();

        // 3. Atomically swap the image files in the background.
        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                File dailyRefreshBackgroundImageFile = createDailyRefreshBackgroundImageFile();
                if (!dailyRefreshBackgroundImageFile.exists()) {
                    return null;
                }

                File mainFile = createBackgroundImageFile();
                // Delete the old main file before renaming. This is necessary because renameTo
                // might fail if the destination file already exists on some systems.
                if (mainFile.exists()) {
                    mainFile.delete();
                }

                if (!dailyRefreshBackgroundImageFile.renameTo(mainFile)) {
                    Log.i(TAG, "Failed to rename daily refresh background image file.");
                    // As a fallback, try to delete the daily refresh file to clean up.
                    dailyRefreshBackgroundImageFile.delete();
                }
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Checks if the current default search engine has a logo.
     *
     * <p>If the {@code profile} is available, this queries the {@link TemplateUrlService}.
     * Otherwise, it falls back to the cached state stored in shared preferences from the last app
     * launch.
     *
     * @param profile The current profile, or null if not yet initialized.
     * @return True if the default search engine supports showing a logo, false otherwise.
     */
    public static boolean doesDefaultSearchEngineHaveLogo(@Nullable Profile profile) {
        return profile != null
                ? TemplateUrlServiceFactory.getForProfile(profile).doesDefaultSearchEngineHaveLogo()
                : ChromeSharedPreferences.getInstance()
                        .readBoolean(APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, true);
    }

    /**
     * Determines if the app is running in a narrow layout (e.g., split-screen) on a tablet.
     *
     * <p>A tablet is considered to be in a narrow window if its horizontal display style is less
     * than {@link HorizontalDisplayStyle#WIDE}.
     *
     * @param isTablet Whether the device is a tablet.
     * @param uiConfig The {@link UiConfig} providing the current display style.
     * @return True if the device is a tablet but the current window width is restricted.
     */
    public static boolean isInNarrowWindowOnTablet(boolean isTablet, UiConfig uiConfig) {
        return isTablet
                && uiConfig.getCurrentDisplayStyle().horizontal < HorizontalDisplayStyle.WIDE;
    }

    /**
     * Calculates the total horizontal margin (sum of start and end margins) for the search box.
     *
     * <p>The margin scales based on the device type and available window width:
     *
     * <ul>
     *   <li>Tablets in narrow/split-screen: Uses narrow-window specific margins.
     *   <li>Tablets in full/wide view: Uses wide-window specific margins.
     *   <li>Phones: Uses the standard Most Visited Tiles (MVT) container margin.
     * </ul>
     *
     * @param resource The {@link Resources} to retrieve dimension pixel sizes.
     * @param uiConfig The {@link UiConfig} to check the current display style.
     * @param isTablet Whether the device is a tablet.
     * @return The combined left and right margins in pixels.
     */
    public static int getSearchBoxTwoSideMargin(
            Resources resource, UiConfig uiConfig, boolean isTablet) {
        if (isInNarrowWindowOnTablet(isTablet, uiConfig)) {
            return resource.getDimensionPixelSize(
                            R.dimen.ntp_search_box_lateral_margin_narrow_window_tablet)
                    * 2;
        } else if (isTablet) {
            return resource.getDimensionPixelSize(R.dimen.ntp_search_box_lateral_margin_tablet) * 2;
        } else {
            return resource.getDimensionPixelSize(R.dimen.mvt_container_lateral_margin) * 2;
        }
    }

    /**
     * Returns the adjusted height of the search box on NTP.
     *
     * @param resources The resources to get dimens.
     * @param showSearchBoxTall Whether to show a tall search box.
     * @param hasShadowApplied Whether a shadow is shown on the search box. Drawing shadow requires
     *     extra paddings on top and bottom of the search box.
     */
    public static int getSearchBoxHeightWithShadows(
            Resources resources, boolean showSearchBoxTall, boolean hasShadowApplied) {
        int searchBoxHeight =
                showSearchBoxTall
                        ? resources.getDimensionPixelSize(R.dimen.ntp_search_box_height_tall)
                        : resources.getDimensionPixelSize(R.dimen.ntp_search_box_height);
        if (!hasShadowApplied) return searchBoxHeight;

        int extraPadding = getLogoVerticalPaddingForShadowPx(resources) * 2;
        return searchBoxHeight + extraPadding;
    }

    /**
     * Calculates the adjusted bottom margin for the Logo view in pixels.
     *
     * <p>If a shadow is applied to the search box, this method subtracts the shadow's padding from
     * the margin. This ensures the perceived visual gap between the logo and the search box remains
     * consistent, regardless of whether the shadow is present.
     *
     * @param resources Android resources.
     * @param applyShadow Whether to account for the search box's shadow padding.
     * @return The final adjusted bottom margin in pixels.
     */
    public static int getLogoViewBottomMarginPx(Resources resources, boolean applyShadow) {
        int bottomMargin = resources.getDimensionPixelSize(R.dimen.ntp_logo_margin_bottom);
        if (applyShadow) {
            bottomMargin -= getLogoVerticalPaddingForShadowPx(resources);
        }
        return bottomMargin;
    }

    /**
     * Returns the internal padding required to accommodate the search box shadow.
     *
     * <p>This padding provides the necessary space for the shadow to be rendered without being
     * clipped by the view's boundaries.
     *
     * @param resources Android resources.
     * @return The shadow padding in pixels.
     */
    private static int getLogoVerticalPaddingForShadowPx(Resources resources) {
        return resources.getDimensionPixelSize(
                R.dimen.composeplate_view_button_padding_for_shadow_bottom);
    }

    /**
     * Creates and configures an NTP customization button.
     *
     * @param context The current Context.
     * @param onClickListener The listener to be attached to the button.
     * @return The created and configured ImageButton.
     */
    public static ImageButton createNtpCustomizationButton(
            Context context, View.OnClickListener onClickListener) {
        ImageButton ntpCustomizationButton = new ImageButton(context);
        Resources resources = context.getResources();
        ntpCustomizationButton.setImageResource(R.drawable.ic_edit_24dp);
        ntpCustomizationButton.setBackgroundResource(R.drawable.edit_icon_circle_background);

        if (shouldUseEditIconWithGreyBackground()) {
            ImageViewCompat.setImageTintList(
                    ntpCustomizationButton, ColorStateList.valueOf(Color.WHITE));
            ViewCompat.setBackgroundTintList(
                    ntpCustomizationButton,
                    ColorStateList.valueOf(
                            ContextCompat.getColor(
                                    context,
                                    R.color.ntp_customization_edit_icon_color_in_grey_background)));
        } else {
            ImageViewCompat.setImageTintList(
                    ntpCustomizationButton, ColorStateList.valueOf(getDefaultIconColor(context)));
        }

        int size =
                resources.getDimensionPixelSize(
                        R.dimen.ntp_customization_edit_icon_background_size);
        FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(size, size);
        layoutParams.gravity = Gravity.BOTTOM | Gravity.END;
        int margin = resources.getDimensionPixelSize(R.dimen.ntp_customization_button_margin);
        layoutParams.setMargins(0, 0, margin, margin);
        ntpCustomizationButton.setLayoutParams(layoutParams);

        ntpCustomizationButton.setOnClickListener(onClickListener);
        ntpCustomizationButton.setContentDescription(
                context.getString(R.string.ntp_customization_title));

        return ntpCustomizationButton;
    }

    public static void resetSharedPreferenceForTesting() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR);
        prefsManager.removeKey(NTP_BACKGROUND_IMAGE_PORTRAIT_INFO);
        prefsManager.removeKey(NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO);
        prefsManager.removeKey(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP);
        prefsManager.removeKey(NTP_CUSTOMIZATION_CHROME_COLOR_DAILY_REFRESH_ENABLED);
        prefsManager.removeKey(NTP_CUSTOMIZATION_THEME_COLOR_ID);
        prefsManager.removeKey(NTP_CUSTOMIZATION_PRIMARY_COLOR_FOR_DAILY_REFRESH);
        prefsManager.removeKey(NTP_BACKGROUND_IMAGE_PORTRAIT_INFO_FOR_DAILY_REFRESH);
        prefsManager.removeKey(NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO_FOR_DAILY_REFRESH);
        prefsManager.removeKey(NTP_CUSTOMIZATION_BACKGROUND_INFO_FOR_DAILY_REFRESH);
        prefsManager.removeKey(NTP_CUSTOMIZATION_MAIN_BOTTOM_SHEET_SHOWN);
    }

    public static void setImageFetcherForTesting(ImageFetcher imageFetcher) {
        sImageFetcherForTesting = imageFetcher;
        ResettersForTesting.register(() -> sImageFetcherForTesting = null);
    }

    /**
     * Reads a bitmap from a URI, downsampling with the same height and width ratio if it is too
     * large.
     *
     * @param context The context to use.
     * @param uri The URI of the image.
     * @param callback The callback to invoke with the bitmap.
     */
    public static void getBitmapFromUriAsync(
            Context context, Uri uri, Callback<@Nullable Bitmap> callback) {
        new AsyncTask<@Nullable Bitmap>() {
            @Override
            protected @Nullable Bitmap doInBackground() {
                try {
                    // 1. Decode with inJustDecodeBounds=true to check dimensions
                    BitmapFactory.Options options = new BitmapFactory.Options();
                    options.inJustDecodeBounds = true;
                    try (var inputStream = context.getContentResolver().openInputStream(uri)) {
                        BitmapFactory.decodeStream(inputStream, null, options);
                    }

                    // 2. Calculate inSampleSize
                    options.inSampleSize =
                            calculateInSampleSize(options, MAX_IMAGE_SIZE, MAX_IMAGE_SIZE);

                    // 3. Decode bitmap with inSampleSize set
                    options.inJustDecodeBounds = false;
                    try (var inputStream = context.getContentResolver().openInputStream(uri)) {
                        return BitmapFactory.decodeStream(inputStream, null, options);
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Error reading bitmap from URI", e);
                    return null;
                }
            }

            @Override
            protected void onPostExecute(@Nullable Bitmap bitmap) {
                callback.onResult(bitmap);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @VisibleForTesting
    static int calculateInSampleSize(BitmapFactory.Options options, int reqWidth, int reqHeight) {
        // Raw height and width of image
        final int height = options.outHeight;
        final int width = options.outWidth;
        int inSampleSize = 1;

        if (height > reqHeight || width > reqWidth) {
            final int halfHeight = height / 2;
            final int halfWidth = width / 2;

            // Calculate the largest inSampleSize value that is a power of 2 and keeps height or
            // width larger than the requested height and width.
            while ((halfHeight / inSampleSize) >= reqHeight
                    || (halfWidth / inSampleSize) >= reqWidth) {
                inSampleSize *= 2;
            }
        }
        return inSampleSize;
    }
}

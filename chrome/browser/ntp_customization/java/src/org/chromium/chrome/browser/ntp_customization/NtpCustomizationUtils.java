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

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Matrix;
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.theme.BackgroundImageInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.concurrent.Executor;

/** Utility class of the NTP customization. */
@NullMarked
public class NtpCustomizationUtils {

    @IntDef({
        NtpBackgroundImageType.DEFAULT,
        NtpBackgroundImageType.IMAGE_FROM_DISK,
        NtpBackgroundImageType.CHROME_COLOR,
        NtpBackgroundImageType.CHROME_THEME
    })
    public @interface NtpBackgroundImageType {
        int DEFAULT = 0;
        int IMAGE_FROM_DISK = 1;
        int CHROME_COLOR = 2;
        int CHROME_THEME = 3;
        int NUM_ENTRIES = 4;
    }

    @VisibleForTesting static final String NTP_BACKGROUND_IMAGE_FILE = "ntp_background_image";
    private static final String TAG = "NtpCustomization";

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

    /** Returns the customized primary color if set, null otherwise. */
    public @Nullable static @ColorInt Integer getPrimaryColorFromCustomizedThemeColor() {
        if (!ChromeFeatureList.sNewTabPageCustomizationV2.isEnabled()
                || (getNtpBackgroundImageType() != NtpBackgroundImageType.CHROME_COLOR)) {
            return null;
        }

        @ColorInt int color = getCustomizedPrimaryColorFromSharedPreference();

        if (color == NtpCustomizationConfigManager.COLOR_NOT_SET) return null;

        return color;
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
     * Sets the NTP's background image type.
     *
     * @param imageType The new image type.
     */
    public static void setNtpBackgroundImageType(@NtpBackgroundImageType int imageType) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_IMAGE_TYPE, imageType);
    }

    /** Gets the current NTP's background image type. */
    public static @NtpBackgroundImageType int getNtpBackgroundImageType() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_IMAGE_TYPE,
                NtpBackgroundImageType.DEFAULT);
    }

    /**
     * Saves the background image if it isn't null, otherwise removes the file.
     *
     * @param backgroundImageBitmap The bitmap of the background image.
     */
    public static void updateBackgroundImageFile(@Nullable Bitmap backgroundImageBitmap) {
        if (backgroundImageBitmap == null) {
            deleteBackgroundImageFile();
            return;
        }

        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                saveBackgroundImageFile(backgroundImageBitmap);
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
    public static void updateBackgroundImageMatrices(BackgroundImageInfo backgroundImageInfo) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeString(
                ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_MATRIX,
                matrixToString(backgroundImageInfo.portraitMatrix));
        prefsManager.writeString(
                ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_MATRIX,
                matrixToString(backgroundImageInfo.landscapeMatrix));
    }

    @VisibleForTesting
    static void saveBackgroundImageFile(Bitmap backgroundImageBitmap) {
        File file = getBackgroundImageFile();

        try (FileOutputStream fileOutputStream = new FileOutputStream(file)) {
            backgroundImageBitmap.compress(Bitmap.CompressFormat.PNG, 100, fileOutputStream);
        } catch (IOException e) {
            Log.i(TAG, "Failed to save background image to: " + file.getAbsolutePath());
        }
    }

    /** Returns the file to save the NTP's background image. */
    @VisibleForTesting
    public static File getBackgroundImageFile() {
        return new File(
                ContextUtils.getApplicationContext().getFilesDir(), NTP_BACKGROUND_IMAGE_FILE);
    }

    @VisibleForTesting
    static void deleteBackgroundImageFile() {
        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                deleteBackgroundImageFileImpl();
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @VisibleForTesting
    static void deleteBackgroundImageFileImpl() {
        File file = getBackgroundImageFile();
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
        new AsyncTask<Bitmap>() {
            @Override
            // The return value of the super class doesn't have @Nullable annotation.
            @SuppressWarnings("NullAway")
            protected Bitmap doInBackground() {
                return readNtpBackgroundImageImpl();
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

    @VisibleForTesting
    static @Nullable Bitmap readNtpBackgroundImageImpl() {
        File file = getBackgroundImageFile();

        if (!file.exists()) {
            return null;
        }

        return BitmapFactory.decodeFile(file.getPath(), null);
    }

    /** Loads the NTP's background transformation matrices from SharedPreferences. */
    public static @Nullable BackgroundImageInfo readNtpBackgroundImageMatrices() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        String portraitMatrixString =
                prefsManager.readString(
                        ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_MATRIX, null);
        String landscapeMatrixString =
                prefsManager.readString(
                        ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_MATRIX, null);

        if (TextUtils.isEmpty(portraitMatrixString) || TextUtils.isEmpty(landscapeMatrixString)) {
            return null;
        }

        Matrix portraitMatrix = stringToMatrix(portraitMatrixString);
        Matrix landscapeMatrix = stringToMatrix(landscapeMatrixString);

        if (portraitMatrix == null || landscapeMatrix == null) {
            return null;
        }

        return new BackgroundImageInfo(portraitMatrix, landscapeMatrix);
    }

    /** Converts a Matrix into a string representation for storage. */
    public static String matrixToString(Matrix matrix) {
        float[] values = new float[9];
        matrix.getValues(values);
        return Arrays.toString(values);
    }

    /** Converts a string representation back into a Matrix. Returns null on failure. */
    public static @Nullable Matrix stringToMatrix(String matrixString) {
        if (matrixString == null || !matrixString.startsWith("[") || !matrixString.endsWith("]")) {
            return null;
        }

        try {
            // Remove brackets and spaces
            String[] stringValues =
                    matrixString.substring(1, matrixString.length() - 1).split(", ");
            if (stringValues.length != 9) return null;

            float[] values = new float[9];
            for (int i = 0; i < 9; i++) {
                values[i] = Float.parseFloat(stringValues[i]);
            }

            Matrix matrix = new Matrix();
            matrix.setValues(values);
            return matrix;
        } catch (Exception e) {
            Log.i(TAG, "Error in stringToMatrix: " + e);
            return null;
        }
    }

    /**
     * Sets the NTP's background color to the SharedPreference.
     *
     * @param color The new background color.
     */
    static void setBackgroundColor(@ColorInt int color) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR, color);
    }

    /** Gets the NTP's background color from the SharedPreference. */
    static @ColorInt int getBackgroundColorFromSharedPreference(@ColorInt int defaultColor) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR, defaultColor);
    }

    /**
     * Sets the customized primary color to the SharedPreference.
     *
     * @param color The new primary theme color.
     */
    static void setCustomizedPrimaryColor(@ColorInt int color) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR, color);
    }

    /** Gets the customized primary color from the SharedPreference. */
    static @ColorInt int getCustomizedPrimaryColorFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR,
                NtpCustomizationConfigManager.COLOR_NOT_SET);
    }

    /** Removes the NTP's background color key and primary color key from the SharedPreference. */
    static void resetCustomizedColors() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR);
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
                && ChromeFeatureList.sNewTabPageCustomizationV2.isEnabled();
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

    public static void resetSharedPreferenceForTesting() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_IMAGE_TYPE);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_MATRIX);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_MATRIX);
    }
}

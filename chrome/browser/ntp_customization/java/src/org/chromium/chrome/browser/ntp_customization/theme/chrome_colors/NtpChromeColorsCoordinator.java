// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CompoundButton;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationMetricsUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.text.EmptyTextWatcher;

import java.util.ArrayList;
import java.util.List;

/** Coordinator for the NTP appearance chrome colors bottom sheet in the NTP customization. */
@NullMarked
public class NtpChromeColorsCoordinator {
    private static final String TAG = "NtpChromeColor";
    private static final int MAX_NUMBER_OF_COLORS_PER_ROW = 7;

    private final List<NtpThemeColorInfo> mChromeColorsList = new ArrayList<>();
    private final Context mContext;
    private final BottomSheetDelegate mDelegate;
    private final PropertyModel mPropertyModel;
    private final int mItemWidth;
    private final int mSpacing;
    private final Runnable mOnChromeColorSelectedCallback;

    // The color info when the Chrome color bottom sheet is created. We compare it with the newly
    // selected one to see if recreate() is necessary when the bottom sheet is closed. This color
    // info should never been changed after creation.
    private final @Nullable NtpThemeColorInfo mPrimaryColorInfo;
    private boolean mIsDailyRefreshToggled;
    private boolean mIsDailyRefreshEnabled;

    private @Nullable NtpChromeColorsAdapter mNtpChromeColorsAdapter;

    private @Nullable NtpThemeColorInfo mLastClickedColorInfo;
    private @Nullable @ColorInt Integer mTypedBackgroundColor;
    private @Nullable @ColorInt Integer mTypedPrimaryColor;

    /**
     * Constructor for the chrome colors coordinator.
     *
     * @param context The context for inflating views and accessing resources.
     * @param delegate The delegate to handle bottom sheet interactions.
     * @param onChromeColorSelectedCallback The callback to run when a color is selected.
     */
    public NtpChromeColorsCoordinator(
            Context context, BottomSheetDelegate delegate, Runnable onChromeColorSelectedCallback) {
        mContext = context;
        mDelegate = delegate;
        mOnChromeColorSelectedCallback = onChromeColorSelectedCallback;
        View ntpChromeColorsBottomSheetView =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.ntp_customization_chrome_colors_bottom_sheet_layout,
                                null,
                                false);

        mPropertyModel = new PropertyModel(NtpChromeColorsProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mPropertyModel,
                ntpChromeColorsBottomSheetView,
                NtpChromeColorsLayoutViewBinder::bind);

        delegate.registerBottomSheetLayout(CHROME_COLORS, ntpChromeColorsBottomSheetView);

        mPropertyModel.set(
                NtpChromeColorsProperties.BACK_BUTTON_CLICK_LISTENER,
                v -> delegate.showBottomSheet(THEME));
        mPropertyModel.set(
                NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER,
                this::onDailyRefreshSwitchToggled);

        if (ChromeFeatureList.sNewTabPageCustomizationV2ShowColorPicker.getValue()) {
            setupColorInputs();
        }

        mItemWidth =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.ntp_customization_chrome_colors_selector_size);
        mSpacing =
                context.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.ntp_customization_chrome_colors_grid_lateral_margin)
                        * 2;

        mPropertyModel.set(
                NtpChromeColorsProperties.RECYCLER_VIEW_LAYOUT_MANAGER,
                new GridLayoutManager(mContext, 1));
        mPropertyModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_ITEM_WIDTH, mItemWidth);
        mPropertyModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_SPACING, mSpacing);
        mPropertyModel.set(
                NtpChromeColorsProperties.RECYCLER_VIEW_MAX_ITEM_COUNT,
                MAX_NUMBER_OF_COLORS_PER_ROW);

        mPrimaryColorInfo = NtpCustomizationConfigManager.getInstance().getNtpThemeColorInfo();
    }

    /**
     * Called before showing the bottom sheet. This method will update the highlighted item of the
     * color list, as well as the state of the daily refresh toggle.
     */
    public void prepareToShow() {
        NtpThemeColorInfo currentColorInfo =
                NtpCustomizationConfigManager.getInstance().getNtpThemeColorInfo();

        // Initializes the state of the daily refresh toggle.
        mIsDailyRefreshEnabled =
                NtpCustomizationConfigManager.getInstance().getBackgroundType()
                                == NtpBackgroundType.CHROME_COLOR
                        && NtpCustomizationUtils
                                .getIsChromeColorDailyRefreshEnabledFromSharedPreference();
        mPropertyModel.set(
                NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED, mIsDailyRefreshEnabled);

        // Initializes the state of the color list recyclerview.
        int primaryColorIndex =
                NtpThemeColorUtils.initColorsListAndFindPrimaryColorIndex(
                        mContext, mChromeColorsList, currentColorInfo);
        if (mNtpChromeColorsAdapter == null) {
            mNtpChromeColorsAdapter =
                    new NtpChromeColorsAdapter(
                            mContext, mChromeColorsList, this::onItemClicked, primaryColorIndex);
            mPropertyModel.set(
                    NtpChromeColorsProperties.RECYCLER_VIEW_ADAPTER, mNtpChromeColorsAdapter);
        }
        // Sets the highlighted color item if user has chosen a customized color theme.
        mPropertyModel.set(NtpChromeColorsProperties.HIGHLIGHTED_ITEM_INDEX, primaryColorIndex);
    }

    @VisibleForTesting
    void onDailyRefreshSwitchToggled(CompoundButton buttonView, boolean isChecked) {
        mIsDailyRefreshToggled = true;
        mIsDailyRefreshEnabled = isChecked;
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(isChecked);

        if (isChecked
                && NtpCustomizationConfigManager.getInstance().getBackgroundType()
                        != NtpBackgroundType.CHROME_COLOR) {
            // If the current background type isn't Chrome color and user turns on daily refresh,
            // highlights the first color info.
            mPropertyModel.set(NtpChromeColorsProperties.HIGHLIGHTED_ITEM_INDEX, 0);
        }
    }

    /**
     * Called when the item view is clicked.
     *
     * @param ntpThemeColorInfo The color instance that the user clicked.
     */
    @VisibleForTesting
    void onItemClicked(NtpThemeColorInfo ntpThemeColorInfo) {
        mDelegate.onNewColorSelected(
                !NtpThemeColorUtils.isPrimaryColorMatched(
                        mContext, mPrimaryColorInfo, ntpThemeColorInfo));
        @NtpBackgroundType
        int newType =
                ntpThemeColorInfo instanceof NtpThemeColorFromHexInfo
                        ? NtpBackgroundType.COLOR_FROM_HEX
                        : NtpBackgroundType.CHROME_COLOR;

        // Applies the primary theme color to the activity before calculating the background color
        // which is a themed color depending on the activity's theme.
        if (mContext instanceof Activity activity) {
            NtpCustomizationUtils.applyDynamicColorToActivity(
                    activity,
                    NtpThemeColorUtils.getPrimaryColorFromColorInfo(mContext, ntpThemeColorInfo));
        }
        NtpCustomizationConfigManager.getInstance()
                .onBackgroundColorChanged(mContext, ntpThemeColorInfo, newType);

        mOnChromeColorSelectedCallback.run();
        mLastClickedColorInfo = ntpThemeColorInfo;
    }

    /** Cleans up the resources used by this coordinator. */
    public void destroy() {
        if (mLastClickedColorInfo != null) {
            NtpCustomizationMetricsUtils.recordChromeColorId(mLastClickedColorInfo.id);
        }

        if (mIsDailyRefreshToggled) {
            NtpCustomizationMetricsUtils.recordChromeColorTurnOnDailyRefresh(
                    mIsDailyRefreshEnabled);
        }

        mPropertyModel.set(NtpChromeColorsProperties.BACK_BUTTON_CLICK_LISTENER, null);
        mPropertyModel.set(NtpChromeColorsProperties.BACKGROUND_COLOR_INPUT_TEXT_WATCHER, null);
        mPropertyModel.set(NtpChromeColorsProperties.PRIMARY_COLOR_INPUT_TEXT_WATCHER, null);
        mPropertyModel.set(NtpChromeColorsProperties.SAVE_BUTTON_CLICK_LISTENER, null);
        mPropertyModel.set(
                NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER, null);

        mChromeColorsList.clear();
    }

    /** Sets up the color picker view. */
    private void setupColorInputs() {
        mPropertyModel.set(NtpChromeColorsProperties.SAVE_BUTTON_CLICK_LISTENER, this::saveColors);
        mPropertyModel.set(
                NtpChromeColorsProperties.BACKGROUND_COLOR_INPUT_TEXT_WATCHER,
                new EmptyTextWatcher() {
                    @Override
                    public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                        mTypedBackgroundColor = getColorFromHex(charSequence.toString());
                        if (mTypedBackgroundColor == null) return;

                        mPropertyModel.set(
                                NtpChromeColorsProperties.BACKGROUND_COLOR_CIRCLE_VIEW_COLOR,
                                mTypedBackgroundColor);
                    }
                });
        mPropertyModel.set(
                NtpChromeColorsProperties.PRIMARY_COLOR_INPUT_TEXT_WATCHER,
                new EmptyTextWatcher() {
                    @Override
                    public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                        mTypedPrimaryColor = getColorFromHex(charSequence.toString());
                        if (mTypedPrimaryColor == null) return;

                        mPropertyModel.set(
                                NtpChromeColorsProperties.PRIMARY_COLOR_CIRCLE_VIEW_COLOR,
                                mTypedPrimaryColor);
                    }
                });
        mPropertyModel.set(
                NtpChromeColorsProperties.CUSTOM_COLOR_PICKER_CONTAINER_VISIBILITY, View.VISIBLE);
    }

    /**
     * Gets the color from the color hexadecimal string, null if the string isn't valid.
     *
     * @param hex The hexadecimal string of a color.
     */
    @VisibleForTesting
    @Nullable
    @ColorInt
    Integer getColorFromHex(String hex) {
        if (hex.length() < 6) return null;

        String colorString = hex.trim();
        if (!colorString.startsWith("#")) {
            colorString = "#" + colorString;
        }

        @ColorInt Integer colorInt = null;
        try {
            colorInt = Color.parseColor(colorString);
        } catch (IllegalArgumentException e) {
            Log.i(TAG, "Unknown color: " + colorString, e);
        }
        return colorInt;
    }

    /**
     * Called to save the manually set colors if both the background color and the primary color are
     * valid.
     */
    private void saveColors(View view) {
        if (mTypedBackgroundColor == null || mTypedPrimaryColor == null) return;

        NtpThemeColorInfo colorInfo =
                new NtpThemeColorFromHexInfo(
                        mContext, mTypedBackgroundColor.intValue(), mTypedPrimaryColor.intValue());
        onItemClicked(colorInfo);
    }

    public @Nullable NtpThemeColorInfo getPrimaryColorInfoForTesting() {
        return mPrimaryColorInfo;
    }

    public PropertyModel getPropertyModelForTesting() {
        return mPropertyModel;
    }

    public boolean getIsDailyRefreshEnabledForTesting() {
        return mIsDailyRefreshEnabled;
    }

    public boolean getIsDailyRefreshToggledForTesting() {
        return mIsDailyRefreshToggled;
    }
}

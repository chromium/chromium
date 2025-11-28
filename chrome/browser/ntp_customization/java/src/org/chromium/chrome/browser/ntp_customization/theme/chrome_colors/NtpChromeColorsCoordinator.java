// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.launchUriActivity;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CompoundButton;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationMetricsUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.text.EmptyTextWatcher;

import java.util.ArrayList;
import java.util.List;

/** Coordinator for the NTP appearance chrome colors bottom sheet in the NTP customization. */
@NullMarked
public class NtpChromeColorsCoordinator {
    // TODO(crbug.com/423579377): Update the url for learn more button.
    private static final String LEARN_MORE_CLICK_URL =
            "https://support.google.com/chrome/?p=new_tab";
    private static final int MAX_NUMBER_OF_COLORS_PER_ROW = 7;

    private final List<NtpThemeColorInfo> mChromeColorsList = new ArrayList<>();
    private final Context mContext;
    private final BottomSheetDelegate mDelegate;
    private final PropertyModel mPropertyModel;
    private final NtpChromeColorGridRecyclerView mChromeColorsRecyclerView;
    private final int mItemWidth;
    private final int mSpacing;
    private final Runnable mOnChromeColorSelectedCallback;
    private final @Nullable NtpThemeColorInfo mPrimaryColorInfo;
    private boolean mIsDailyRefreshToggled;
    private boolean mIsDailyRefreshEnabled;
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
                NtpChromeColorsProperties.LEARN_MORE_BUTTON_CLICK_LISTENER,
                this::handleLearnMoreClick);
        mIsDailyRefreshEnabled =
                NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference();
        mPropertyModel.set(
                NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED, mIsDailyRefreshEnabled);
        mPropertyModel.set(
                NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER,
                this::onDailyRefreshSwitchToggled);

        if (ChromeFeatureList.sNewTabPageCustomizationV2ShowColorPicker.getValue()) {
            setupColorInputs();
        }

        mChromeColorsRecyclerView =
                ntpChromeColorsBottomSheetView.findViewById(R.id.chrome_colors_recycler_view);
        mItemWidth =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.ntp_customization_back_button_clickable_size);
        mSpacing =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.ntp_customization_chrome_colors_grid_spacing);

        mPrimaryColorInfo = NtpCustomizationUtils.loadColorInfoFromSharedPreference(mContext);
        buildRecyclerView();
        setRecyclerViewMaxWidth();

        // Post the task to expand the sheet to ensure that the bottom sheet view is laid out and
        // has a height, allowing it to correctly open to the half-height state.
        mChromeColorsRecyclerView.post(
                () -> {
                    delegate.getBottomSheetController().expandSheet();
                });
    }

    private void onDailyRefreshSwitchToggled(CompoundButton buttonView, boolean isChecked) {
        mIsDailyRefreshToggled = true;
        mIsDailyRefreshEnabled = isChecked;
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(isChecked);
    }

    private void buildRecyclerView() {
        mPropertyModel.set(
                NtpChromeColorsProperties.RECYCLER_VIEW_LAYOUT_MANAGER,
                new GridLayoutManager(mContext, 1));
        mPropertyModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_ITEM_WIDTH, mItemWidth);
        mPropertyModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_SPACING, mSpacing);

        int primaryColorIndex =
                NtpThemeColorUtils.initColorsListAndFindPrimaryColorIndex(
                        mContext, mChromeColorsList, mPrimaryColorInfo);
        NtpChromeColorsAdapter adapter =
                new NtpChromeColorsAdapter(
                        mContext, mChromeColorsList, this::onItemClicked, primaryColorIndex);

        mPropertyModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_ADAPTER, adapter);
    }

    private void setRecyclerViewMaxWidth() {
        int maxWidthPx = MAX_NUMBER_OF_COLORS_PER_ROW * (mItemWidth + mSpacing);
        mPropertyModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_MAX_WIDTH_PX, maxWidthPx);
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
        @NtpBackgroundImageType
        int newType =
                ntpThemeColorInfo instanceof NtpThemeColorFromHexInfo
                        ? NtpBackgroundImageType.COLOR_FROM_HEX
                        : NtpBackgroundImageType.CHROME_COLOR;

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
        mPropertyModel.set(NtpChromeColorsProperties.LEARN_MORE_BUTTON_CLICK_LISTENER, null);
        mPropertyModel.set(NtpChromeColorsProperties.BACKGROUND_COLOR_INPUT_TEXT_WATCHER, null);
        mPropertyModel.set(NtpChromeColorsProperties.PRIMARY_COLOR_INPUT_TEXT_WATCHER, null);
        mPropertyModel.set(NtpChromeColorsProperties.SAVE_BUTTON_CLICK_LISTENER, null);
        mPropertyModel.set(
                NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER, null);

        mChromeColorsList.clear();
    }

    /**
     * Handles the click event for the "Learn More" button in the Chrome Colors bottom sheet.
     *
     * @param view The view that was clicked.
     */
    @VisibleForTesting
    void handleLearnMoreClick(View view) {
        launchUriActivity(view.getContext(), LEARN_MORE_CLICK_URL);
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

        return Color.parseColor(colorString);
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
}

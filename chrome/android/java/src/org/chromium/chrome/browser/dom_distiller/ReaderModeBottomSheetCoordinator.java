// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;

import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinates the bottom-sheet for reader mode. */
@NullMarked
public class ReaderModeBottomSheetCoordinator {
    private final Context mContext;
    private final PropertyModel mPropertyModel;
    private final PropertyModelChangeProcessor<
                    PropertyModel, ReaderModeBottomSheetView, PropertyKey>
            mChangeProcessor;
    private final DestroyChecker mDestroyChecker;
    private final BottomSheetController mBottomSheetController;
    private final ReaderModeBottomSheetContent mBottomSheetContent;
    private final ReaderModeBottomSheetView mReaderModeBottomSheetView;
    private final DomDistillerService mDomDistillerService;
    private final ThemeColorProvider mThemeColorProvider;
    private final ThemeColorProvider.ThemeColorObserver mThemeColorObserver;
    private final ThemeColorProvider.TintObserver mThemeTintObserver;

    private @Nullable Tab mTab;

    /**
     * @param context The {@link Context} associated with this coordinator.
     * @param profile The {@link Profile} associated with this coordinator.
     * @param bottomSheetController Allows displaying content in the bottom sheet.
     * @param themeColorProvider Provides the theme color for the bottom sheet.
     */
    public ReaderModeBottomSheetCoordinator(
            Context context,
            Profile profile,
            BottomSheetController bottomSheetController,
            ThemeColorProvider themeColorProvider) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mDestroyChecker = new DestroyChecker();
        mDomDistillerService = DomDistillerServiceFactory.getForProfile(profile);
        mThemeColorProvider = themeColorProvider;

        mReaderModeBottomSheetView =
                (ReaderModeBottomSheetView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.reader_mode_bottom_sheet, /* root= */ null);

        mPropertyModel = new PropertyModel(ReaderModeBottomSheetProperties.ALL_KEYS);
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel,
                        mReaderModeBottomSheetView,
                        ReaderModeBottomSheetViewBinder::bind);

        mPropertyModel.set(
                ReaderModeBottomSheetProperties.CONTENT_VIEW,
                ReaderModePrefsView.create(mContext, mDomDistillerService.getDistilledPagePrefs()));

        // Expand the peeked bottom sheet when tapped.
        mReaderModeBottomSheetView.setOnClickListener(
                view -> {
                    if (mBottomSheetController.getSheetState()
                            == BottomSheetController.SheetState.PEEK) {
                        mBottomSheetController.expandSheet();
                    }
                });

        mThemeColorObserver =
                (color, shouldAnimate) -> {
                    updateThemeProperties();
                };
        mThemeColorProvider.addThemeColorObserver(mThemeColorObserver);

        mThemeTintObserver =
                (tint, activityFocusTint, brandedColorScheme) -> {
                    updateThemeProperties();
                };
        mThemeColorProvider.addTintObserver(mThemeTintObserver);

        mBottomSheetContent = new ReaderModeBottomSheetContent(mReaderModeBottomSheetView);
    }

    /**
     * Shows the reader mode bottom sheet.
     *
     * @param showFullSheet Whether the bottomsheet should be shown fully, if false it's shown in a
     *     peeked state.
     */
    public void show(Tab tab) {
        mDestroyChecker.checkNotDestroyed();
        setTab(tab);
        // Only try to show the bottom sheet if it's not already showing. BottomSheetController
        // makes a copy of the sheet content, so equals comparison isn't useful here.
        boolean showing =
                mBottomSheetController.getCurrentSheetContent() == mBottomSheetContent;

        // Workaround for a bug where the bottom sheet will get stuck in the NONE state after the
        // activity is recreated by a theme change.
        if (!showing) {
            showing = mBottomSheetController.requestShowContent(
                            mBottomSheetContent, /* animate= */ true);
            if (showing) {
                ReaderModeMetrics.reportReaderModePrefsOpened();
            }
        }
    }

    /** Hides the reader mode bottom sheet. */
    public void hide() {
        mDestroyChecker.checkNotDestroyed();
        mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ true);
    }

    /** Destroys the coordinator. */
    public void destroy() {
        mDestroyChecker.destroy();
        mChangeProcessor.destroy();
        mThemeColorProvider.removeThemeColorObserver(mThemeColorObserver);
        mThemeColorProvider.removeTintObserver(mThemeTintObserver);
    }

    private class ReaderModeBottomSheetContent implements BottomSheetContent {
        private final View mContentView;

        ReaderModeBottomSheetContent(View contentView) {
            mContentView = contentView;
        }

        @Override
        public View getContentView() {
            return mContentView;
        }

        @Override
        public @Nullable View getToolbarView() {
            return null;
        }

        @Override
        public int getVerticalScrollOffset() {
            return 0;
        }

        @Override
        public void destroy() {}

        @Override
        public int getPriority() {
            return BottomSheetContent.ContentPriority.LOW;
        }

        @Override
        public int getPeekHeight() {
            return ((ReaderModeBottomSheetView) mContentView).getPeekHeight();
        }

        @Override
        public float getFullHeightRatio() {
            return BottomSheetContent.HeightMode.WRAP_CONTENT;
        }

        @Override
        public boolean swipeToDismissEnabled() {
            // Required to be false for tapping scrim to return bottomsheet to peek state.
            return false;
        }

        @Override
        public boolean hideOnScroll() {
            // This bottom sheet is "persistent", but the default #hideOnScroll behavior is too
            // buggy when the sheet interacts with the bottom controls. Correct implementation for
            // this is to integrate BottomSheetManager directly with BottomControlsStacker, but the
            // implementation is non-trivial. Instead, this sheet will be easily dismissable and
            // come back on scroll up.
            return false;
        }

        @Override
        public String getSheetContentDescription(Context context) {
            return context.getString(R.string.reader_mode_bottom_sheet_content_description);
        }

        @Override
        public @StringRes int getSheetClosedAccessibilityStringId() {
            return R.string.reader_mode_bottom_sheet_closed_content_description;
        }

        @Override
        public @StringRes int getSheetHalfHeightAccessibilityStringId() {
            return R.string.reader_mode_bottom_sheet_half_height_content_description;
        }

        @Override
        public @StringRes int getSheetFullHeightAccessibilityStringId() {
            return R.string.reader_mode_bottom_sheet_full_height_content_description;
        }

        @Override
        public boolean canSuppressInAnyState() {
            return true;
        }

        @Override
        public @ColorInt int getSheetBackgroundColorOverride() {
            return mPropertyModel.containsKey(ReaderModeBottomSheetProperties.BACKGROUND_COLOR)
                    ? mPropertyModel.get(ReaderModeBottomSheetProperties.BACKGROUND_COLOR)
                    : Color.TRANSPARENT;
        }
    }

    void setTab(Tab tab) {
        mTab = tab;
        // Update the theme properties to handle the case where the bottom sheet is shown in both
        // incognito and regular tabs.
        updateThemeProperties();
    }

    private void updateThemeProperties() {
        // A non-null tab is required to get the correct color for the toolbar. This could be called prior to showing the bottom sheet and in that case we want to early return.
        if (mTab == null) return;
        @ColorInt int color = mThemeColorProvider.getThemeColor();
        @BrandedColorScheme int brandedColorScheme = mThemeColorProvider.getBrandedColorScheme();

        mPropertyModel.set(ReaderModeBottomSheetProperties.BACKGROUND_COLOR, color);
        mPropertyModel.set(
                ReaderModeBottomSheetProperties.SECONDARY_BACKGROUND_COLOR,
                ThemeUtils.getTextBoxColorForToolbarBackground(mContext, mTab, color));
        mPropertyModel.set(
                ReaderModeBottomSheetProperties.ICON_TINT,
                ThemeUtils.getThemedToolbarIconTint(mContext, brandedColorScheme));
        mPropertyModel.set(
                ReaderModeBottomSheetProperties.PRIMARY_TEXT_COLOR,
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(mContext, brandedColorScheme));
        mPropertyModel.set(
                ReaderModeBottomSheetProperties.SECONDARY_TEXT_COLOR,
                OmniboxResourceProvider.getUrlBarSecondaryTextColor(mContext, brandedColorScheme));
        mPropertyModel.set(
                ReaderModeBottomSheetProperties.ICON_TINT,
                ThemeUtils.getThemedToolbarIconTint(mContext, brandedColorScheme));
        mBottomSheetController.onSheetBackgroundColorOverrideChanged();
    }

    // For testing methods.

    View getViewForTesting() {
        return mReaderModeBottomSheetView;
    }

    BottomSheetContent getBottomSheetContentForTesting() {
        return mBottomSheetContent;
    }
}

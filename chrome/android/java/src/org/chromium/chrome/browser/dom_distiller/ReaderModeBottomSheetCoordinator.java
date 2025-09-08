// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

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

    /**
     * @param tab The {@link Tab} associated with this coordinator.
     * @param context The {@link Context} associated with this coordinator.
     * @param profile The {@link Profile} associated with this coordinator.
     * @param bottomSheetController Allows displaying content in the bottom sheet.
     * @param themeColorProvider Provides the theme color for the bottom sheet.
     */
    public ReaderModeBottomSheetCoordinator(
            Tab tab,
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

        mThemeColorObserver =
                (color, shouldAnimate) -> {
                    mPropertyModel.set(ReaderModeBottomSheetProperties.BACKGROUND_COLOR, color);
                    mPropertyModel.set(
                            ReaderModeBottomSheetProperties.SECONDARY_BACKGROUND_COLOR,
                            ThemeUtils.getTextBoxColorForToolbarBackground(mContext, tab, color));
                };
        mThemeColorProvider.addThemeColorObserver(mThemeColorObserver);
        mThemeColorObserver.onThemeColorChanged(
                mThemeColorProvider.getThemeColor(), /* shouldAnimate= */ false);

        mThemeTintObserver =
                (tint, activityFocusTint, brandedColorScheme) -> {
                    mPropertyModel.set(
                            ReaderModeBottomSheetProperties.ICON_TINT,
                            ThemeUtils.getThemedToolbarIconTint(mContext, brandedColorScheme));
                    mPropertyModel.set(
                            ReaderModeBottomSheetProperties.PRIMARY_TEXT_COLOR,
                            OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                                    mContext, brandedColorScheme));
                    mPropertyModel.set(
                            ReaderModeBottomSheetProperties.SECONDARY_TEXT_COLOR,
                            OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                                    mContext, brandedColorScheme));
                    mPropertyModel.set(
                            ReaderModeBottomSheetProperties.ICON_TINT,
                            ThemeUtils.getThemedToolbarIconTint(mContext, brandedColorScheme));
                };
        mThemeColorProvider.addTintObserver(mThemeTintObserver);
        mThemeTintObserver.onTintChanged(
                mThemeColorProvider.getTint(),
                mThemeColorProvider.getActivityFocusTint(),
                mThemeColorProvider.getBrandedColorScheme());

        mBottomSheetContent = new ReaderModeBottomSheetContent(mReaderModeBottomSheetView);
    }

    /**
     * Shows the reader mode bottom sheet.
     *
     * @param showFullSheet Whether the bottomsheet should be shown fully, if false it's shown in a
     *     peeked state.
     */
    public void show(boolean showFullSheet) {
        mDestroyChecker.checkNotDestroyed();
        // Only try to show the bottom sheet if it's not already showing. BottomSheetController
        // makes a copy of the sheet content, so equals comparison isn't useful here.
        boolean showing =
                mBottomSheetController.getCurrentSheetContent()
                        instanceof ReaderModeBottomSheetContent;
        if (!showing) {
            ReaderModeMetrics.reportReaderModePrefsOpened();
            showing =
                    mBottomSheetController.requestShowContent(
                            mBottomSheetContent, /* animate= */ true);
        }

        if (showing && showFullSheet) {
            mBottomSheetController.expandSheet();
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

    private static class ReaderModeBottomSheetContent implements BottomSheetContent {
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
        public boolean canSuppressInAnyState() {
            return true;
        }

        @Override
        public float getFullHeightRatio() {
            return BottomSheetContent.HeightMode.WRAP_CONTENT;
        }

        @Override
        public boolean swipeToDismissEnabled() {
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
        public boolean hasCustomScrimLifecycle() {
            return false;
        }

        @Override
        public boolean hasCustomLifecycle() {
            return true;
        }

        @Override
        public int getPeekHeight() {
            return ((ReaderModeBottomSheetView) mContentView).getPeekHeight();
        }

        @Override
        public boolean hideOnScroll() {
            return true;
        }
    }

    // For testing methods.

    View getViewForTesting() {
        return mReaderModeBottomSheetView;
    }

    BottomSheetContent getBottomSheetContentForTesting() {
        return mBottomSheetContent;
    }
}

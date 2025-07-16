// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
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

    /**
     * @param context The {@link Context} associated with this coordinator.
     * @param profile The {@link Profile} associated with this coordinator.
     * @param bottomSheetController Allows displaying content in the bottom sheet.
     */
    public ReaderModeBottomSheetCoordinator(
            Context context, Profile profile, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mDestroyChecker = new DestroyChecker();
        mDomDistillerService = DomDistillerServiceFactory.getForProfile(profile);

        mPropertyModel = new PropertyModel(ReaderModeBottomSheetProperties.ALL_KEYS);
        mPropertyModel.set(
                ReaderModeBottomSheetProperties.CONTENT_VIEW,
                DistilledPagePrefsView.create(
                        mContext, mDomDistillerService.getDistilledPagePrefs()));
        mReaderModeBottomSheetView =
                (ReaderModeBottomSheetView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.reader_mode_bottom_sheet, /* root= */ null);

        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel,
                        mReaderModeBottomSheetView,
                        ReaderModeBottomSheetViewBinder::bind);
        mBottomSheetContent = new ReaderModeBottomSheetContent(mReaderModeBottomSheetView);
    }

    /** Shows the reader mode bottom sheet. */
    public void show() {
        mDestroyChecker.checkNotDestroyed();
        mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
    }

    /** Destroys the coordinator. */
    public void destroy() {
        mDestroyChecker.destroy();
        mChangeProcessor.destroy();
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
        public void destroy() {
            // Note: This bottom sheet can be hidden/shown multiple times without re-creation.
        }

        @Override
        public int getPriority() {
            return BottomSheetContent.ContentPriority.HIGH;
        }

        @Override
        public float getFullHeightRatio() {
            return BottomSheetContent.HeightMode.WRAP_CONTENT;
        }

        @Override
        public boolean swipeToDismissEnabled() {
            return true;
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
        public int getPeekHeight() {
            return mContentView.findViewById(R.id.drag_handle).getHeight()
                    + mContentView.findViewById(R.id.title).getHeight();
        }

        @Override
        public boolean hideOnScroll() {
            return true;
        }
    }

    // For testing methods.

    @VisibleForTesting
    View getView() {
        return mReaderModeBottomSheetView;
    }
}

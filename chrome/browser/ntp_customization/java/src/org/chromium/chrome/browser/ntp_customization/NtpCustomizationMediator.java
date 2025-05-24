// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LAYOUT_TO_DISPLAY;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.View;
import android.widget.ViewFlipper;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A mediator class that manages the view flipper and {@link BottomSheetContent} of NTP
 * customization bottom sheets.
 */
@NullMarked
public class NtpCustomizationMediator {
    /**
     * A map of <{@link NtpCustomizationCoordinator.BottomSheetType}, view's position index in the
     * {@link ViewFlipper}>.
     */
    private final Map<Integer, Integer> mViewFlipperMap;

    private final Map<Integer, View.OnClickListener> mTypeToListenersMap;
    private final BottomSheetController mBottomSheetController;
    private final NtpCustomizationBottomSheetContent mBottomSheetContent;
    private final BottomSheetObserver mBottomSheetObserver;
    private final PropertyModel mViewFlipperPropertyModel;
    private List<Integer> mListContent;
    private final Supplier<Profile> mProfileSupplier;
    private final @Nullable PropertyModel mContainerPropertyModel;
    private @Nullable Profile mProfile;
    private @Nullable Integer mCurrentBottomSheet;
    private static @Nullable PrefService sPrefServiceForTest;

    public NtpCustomizationMediator(
            BottomSheetController bottomSheetController,
            NtpCustomizationBottomSheetContent bottomSheetContent,
            PropertyModel viewFlipperPropertyModel,
            @Nullable PropertyModel containerPropertyModel,
            Supplier<Profile> profileSupplier) {
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mViewFlipperPropertyModel = viewFlipperPropertyModel;
        mContainerPropertyModel = containerPropertyModel;
        mProfileSupplier = profileSupplier;
        mViewFlipperMap = new HashMap<>();
        mTypeToListenersMap = new HashMap<>();
        mListContent = buildListContent();

        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetOpened(@BottomSheetController.StateChangeReason int reason) {
                        mBottomSheetContent.onSheetOpened();
                    }

                    @Override
                    public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                        mBottomSheetContent.onSheetClosed();
                        mBottomSheetController.removeObserver(mBottomSheetObserver);
                    }
                };
        mBottomSheetController.addObserver(mBottomSheetObserver);
    }

    /**
     * Records the position of the bottom sheet layout in the view flipper view. The position index
     * starts at 0.
     *
     * @param type The type of the bottom sheet.
     */
    void registerBottomSheetLayout(@NtpCustomizationCoordinator.BottomSheetType int type) {
        if (mViewFlipperMap.containsKey(type)) return;

        mViewFlipperMap.put(type, mViewFlipperMap.size());
    }

    /** Shows the given type of the bottom sheet. */
    void showBottomSheet(@NtpCustomizationCoordinator.BottomSheetType int type) {
        assert mViewFlipperMap.get(type) != null;

        int viewIndex = mViewFlipperMap.get(type);
        mViewFlipperPropertyModel.set(LAYOUT_TO_DISPLAY, viewIndex);
        boolean shouldRequestShowContent = mCurrentBottomSheet == null;
        mCurrentBottomSheet = type;

        // requestShowContent() is called only once when showBottomSheet() is invoked for the first
        // time.
        if (shouldRequestShowContent) {
            mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
        }
        NtpCustomizationMetricsUtils.recordBottomSheetShown(type);
    }

    /** Handles system back press and back button clicks on the bottom sheet. */
    void backPressOnCurrentBottomSheet() {
        if (mCurrentBottomSheet == null) return;

        if (mCurrentBottomSheet == MAIN) {
            dismissBottomSheet();
        } else {
            showBottomSheet(MAIN);

            // Updates the visibility status (on or off) of the feeds section in the main bottom
            // sheet.
            updateFeedSectionSubtitle(getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE));
        }
    }

    /** Closes the entire bottom sheet view and returns to the New Tab Page. */
    void dismissBottomSheet() {
        mBottomSheetController.hideContent(mBottomSheetContent, true);
        mCurrentBottomSheet = null;
    }

    /**
     * Returns {@link ListContainerViewDelegate} that defines the content of each list item in the
     * main bottom sheet.
     */
    ListContainerViewDelegate createListDelegate() {
        return new ListContainerViewDelegate() {
            @Override
            public List<Integer> getListItems() {
                return mListContent;
            }

            @Override
            public int getListItemId(int type) {
                switch (type) {
                    case NTP_CARDS:
                        return R.id.ntp_cards;
                    case FEED:
                        return R.id.feed_settings;
                    default:
                        return View.NO_ID;
                }
            }

            @Override
            public String getListItemTitle(int type, Context context) {
                switch (type) {
                    case NTP_CARDS:
                        return context.getString(R.string.home_modules_configuration);
                    case FEED:
                        return context.getString(R.string.ntp_customization_feed_settings_title);
                    default:
                        assert false : "Bottom sheet type not supported!";
                        return assumeNonNull(null);
                }
            }

            @Override
            public @Nullable String getListItemSubtitle(int type, Context context) {
                if (type == FEED) {
                    return context.getString(getFeedSectionSubtitleId());
                }
                return null;
            }

            @Override
            public View.@Nullable OnClickListener getListener(int type) {
                return mTypeToListenersMap.get(type);
            }

            @Override
            public @Nullable Integer getTrailingIcon(int type) {
                return R.drawable.forward_arrow_icon;
            }
        };
    }

    /** Renders the options list in the main bottom sheet. */
    void renderListContent() {
        assumeNonNull(mContainerPropertyModel);
        mContainerPropertyModel.set(LIST_CONTAINER_VIEW_DELEGATE, createListDelegate());
    }

    /**
     * Sets click listener on the list item view of the given type.
     *
     * @param type The type of the list item to which the click listener will be added.
     * @param listener The click listener to set on the list item of the given type.
     */
    void registerClickListener(int type, View.OnClickListener listener) {
        mTypeToListenersMap.put(type, listener);
    }

    /**
     * Returns the content of the list displayed in the main bottom sheet and includes the "Discover
     * Feed" list item only if the feed section exists in the new tab page.
     */
    @VisibleForTesting
    List<Integer> buildListContent() {
        if (!mProfileSupplier.hasValue()) {
            return List.of(NTP_CARDS);
        }

        mProfile = mProfileSupplier.get().getOriginalProfile();
        List<Integer> content = new ArrayList<>();
        content.add(NTP_CARDS);
        if (FeedFeatures.isFeedEnabled(mProfile)) {
            content.add(FEED);
        }
        return content;
    }

    /** Clears maps */
    void destroy() {
        mViewFlipperMap.clear();
        mTypeToListenersMap.clear();
    }

    /**
     * Updates the subtitle of the feed section in the main bottom sheet.
     *
     * <p>Called when users enable or disable showing Feeds.
     *
     * @param isFeedVisible True when the feed is visible to the user.
     */
    void updateFeedSectionSubtitle(boolean isFeedVisible) {
        assumeNonNull(mContainerPropertyModel);
        mContainerPropertyModel.set(
                MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE,
                isFeedVisible ? R.string.text_on : R.string.text_off);
    }

    /** Returns the source id of the feed section subtitle. */
    private int getFeedSectionSubtitleId() {
        return getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE)
                ? R.string.text_on
                : R.string.text_off;
    }

    private PrefService getPrefService() {
        if (sPrefServiceForTest != null) return sPrefServiceForTest;

        assert mProfile != null;
        return UserPrefs.get(mProfile);
    }

    static void setPrefForTesting(PrefService prefService) {
        sPrefServiceForTest = prefService;
        ResettersForTesting.register(
                () -> {
                    sPrefServiceForTest = null;
                });
    }

    Map<Integer, Integer> getViewFlipperMapForTesting() {
        return mViewFlipperMap;
    }

    @SuppressWarnings("NullAway") // The call sites require non-null but the value is nullable.
    @NtpCustomizationCoordinator.BottomSheetType
    Integer getCurrentBottomSheetType() {
        return mCurrentBottomSheet;
    }

    void setCurrentBottomSheetForTesting(
            @NtpCustomizationCoordinator.BottomSheetType int bottomSheetType) {
        mCurrentBottomSheet = bottomSheetType;
    }

    BottomSheetObserver getBottomSheetObserverForTesting() {
        return mBottomSheetObserver;
    }

    Map<Integer, View.OnClickListener> getTypeToListenersForTesting() {
        return mTypeToListenersMap;
    }

    void setListContetForTesting(List<Integer> listContent) {
        mListContent = listContent;
    }
}

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
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LAYOUT_TO_DISPLAY;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MAIN_BOTTOM_SHEET_MVT_SECTION_SUBTITLE;

import android.content.Context;
import android.view.View;
import android.widget.ViewFlipper;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeStateProvider;
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
import java.util.function.Supplier;

/**
 * A mediator class that manages the view flipper and {@link BottomSheetContent} of NTP
 * customization bottom sheets.
 */
@NullMarked
public class NtpCustomizationMediator {
    // Defines the back navigation hierarchy for theme-related bottom sheets. <Child, Parent>
    private final Map<Integer, Integer> mThemeBackNavigationMap =
            Map.ofEntries(
                    Map.entry(SINGLE_THEME_COLLECTION, THEME_COLLECTIONS),
                    Map.entry(THEME_COLLECTIONS, THEME),
                    Map.entry(CHROME_COLORS, THEME));

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
    private final List<Integer> mListContent;
    private final Supplier<@Nullable Profile> mProfileSupplier;
    private final @Nullable PropertyModel mContainerPropertyModel;
    private final boolean mNtpCustomizationForMvtFeatureEnabled;
    private @Nullable Profile mProfile;
    private @Nullable Integer mCurrentBottomSheet;
    private boolean mShouldRecreate;
    private static @Nullable PrefService sPrefServiceForTest;

    public NtpCustomizationMediator(
            Context context,
            BottomSheetController bottomSheetController,
            NtpCustomizationBottomSheetContent bottomSheetContent,
            PropertyModel viewFlipperPropertyModel,
            @Nullable PropertyModel containerPropertyModel,
            Supplier<@Nullable Profile> profileSupplier) {
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mViewFlipperPropertyModel = viewFlipperPropertyModel;
        mContainerPropertyModel = containerPropertyModel;
        mProfileSupplier = profileSupplier;
        mViewFlipperMap = new HashMap<>();
        mTypeToListenersMap = new HashMap<>();
        mListContent = buildListContent();
        mNtpCustomizationForMvtFeatureEnabled =
                ChromeFeatureList.sNewTabPageCustomizationForMvt.isEnabled();

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
                        // Notify to recreate activities if a new customized theme color is selected
                        // or removed.
                        if (mShouldRecreate) {
                            NtpThemeStateProvider.getInstance().notifyApplyThemeChanges();
                        }
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

    // Called when a customized theme color is selected or removed.
    void onNewColorSelected(boolean isDifferentColor) {
        mShouldRecreate = isDifferentColor;
    }

    /** Handles system back press and back button clicks on the bottom sheet. */
    void backPressOnCurrentBottomSheet() {
        if (mCurrentBottomSheet == null) return;

        if (mCurrentBottomSheet == MAIN) {
            dismissBottomSheet(/* animate= */ true);
            return;
        }

        @NtpCustomizationCoordinator.BottomSheetType
        Integer parentSheet = mThemeBackNavigationMap.get(mCurrentBottomSheet);

        if (parentSheet != null) {
            showBottomSheet(parentSheet);
        } else {
            showBottomSheet(MAIN);

            // Updates the visibility status (on or off) of the feeds section in the main bottom
            // sheet.
            updateFeedSectionSubtitle(getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE));

            boolean isMvtVisible =
                    mNtpCustomizationForMvtFeatureEnabled
                            && NtpCustomizationConfigManager.getInstance().getPrefIsMvtToggleOn();
            updateMvtSectionSubtitle(isMvtVisible);
        }
    }

    /** Closes the entire bottom sheet view and returns to the New Tab Page. */
    void dismissBottomSheet(boolean animate) {
        mBottomSheetController.hideContent(mBottomSheetContent, animate);
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
                    case MVT:
                        return R.id.mvt_settings;
                    case NTP_CARDS:
                        return R.id.ntp_cards;
                    case FEED:
                        return R.id.feed_settings;
                    case THEME:
                        return R.id.theme;
                    default:
                        return View.NO_ID;
                }
            }

            @Override
            public String getListItemTitle(int type, Context context) {
                switch (type) {
                    case MVT:
                        return context.getString(R.string.ntp_customization_mvt_settings_title);
                    case NTP_CARDS:
                        return context.getString(R.string.home_modules_configuration);
                    case FEED:
                        return context.getString(R.string.ntp_customization_feed_settings_title);
                    case THEME:
                        return context.getString(R.string.ntp_customization_theme_title);
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

                if (type == MVT) {
                    return context.getString(getMvtSectionSubtitleId());
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

            @Override
            public @Nullable Integer getTrailingIconDescriptionResId(int type) {
                return R.string.ntp_customization_show_more;
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
        Profile profile = mProfileSupplier.get();
        if (profile == null) {
            return List.of(NTP_CARDS);
        }

        mProfile = profile.getOriginalProfile();
        List<Integer> content = new ArrayList<>();
        if (ChromeFeatureList.sNewTabPageCustomizationForMvt.isEnabled()) {
            content.add(MVT);
        }
        content.add(NTP_CARDS);
        if (FeedFeatures.isFeedEnabled(mProfile)) {
            content.add(FEED);
        }
        if (ChromeFeatureList.sNewTabPageCustomizationV2.isEnabled()) {
            content.add(THEME);
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

    void updateMvtSectionSubtitle(boolean isMvtVisible) {
        assumeNonNull(mContainerPropertyModel);
        mContainerPropertyModel.set(
                MAIN_BOTTOM_SHEET_MVT_SECTION_SUBTITLE,
                isMvtVisible ? R.string.text_on : R.string.text_off);
    }

    /** Returns the source id of the feed section subtitle. */
    private int getFeedSectionSubtitleId() {
        return getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE)
                ? R.string.text_on
                : R.string.text_off;
    }

    /** Returns the source id of the mvt section subtitle. */
    @StringRes
    private int getMvtSectionSubtitleId() {
        return NtpCustomizationConfigManager.getInstance().getPrefIsMvtToggleOn()
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

    @Nullable
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
}

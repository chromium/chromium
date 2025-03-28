// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.DISCOVER_FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LAYOUT_TO_DISPLAY;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.View;
import android.widget.ViewFlipper;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A mediator class that manages the view flipper and {@link BottomSheetContent} of NTP
 * customization bottom sheets.
 */
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
    private final List<Integer> mListContent;
    private final PropertyModel mContainerPropertyModel;
    private Integer mCurrentBottomSheet;

    public NtpCustomizationMediator(
            BottomSheetController bottomSheetController,
            NtpCustomizationBottomSheetContent bottomSheetContent,
            PropertyModel viewFlipperPropertyModel,
            PropertyModel containerPropertyModel) {
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mViewFlipperPropertyModel = viewFlipperPropertyModel;
        mContainerPropertyModel = containerPropertyModel;
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
    }

    /** Handles system back press and back button clicks on the bottom sheet. */
    void backPressOnCurrentBottomSheet() {
        if (mCurrentBottomSheet == null) return;

        if (mCurrentBottomSheet == MAIN) {
            mBottomSheetController.hideContent(mBottomSheetContent, true);
            mCurrentBottomSheet = null;
        } else {
            showBottomSheet(MAIN);
        }
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
            public String getListItemTitle(int type, Context context) {
                switch (type) {
                    case NTP_CARDS:
                        return context.getString(R.string.home_modules_configuration);
                    case DISCOVER_FEED:
                        return context.getString(R.string.ntp_customization_feed_setting_title);
                    default:
                        assert false : "Bottom sheet type not supported!";
                        return null;
                }
            }

            @Override
            @Nullable
            public String getListItemSubtitle(int type, Context context) {
                if (type == DISCOVER_FEED) {
                    // TODO(crbug.com/397439004): Add logics to display "off".
                    return context.getString(R.string.ntp_customization_feed_section_on);
                }
                return null;
            }

            @Override
            public View.OnClickListener getListener(int type) {
                return mTypeToListenersMap.get(type);
            }

            @Override
            @Nullable
            public Integer getTrailingIcon(int type) {
                return R.drawable.forward_arrow_icon;
            }
        };
    }

    /** Renders the options list in the main bottom sheet. */
    void renderListContent() {
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

    /** Returns the content of the list displayed in the main bottom sheet. */
    List<Integer> buildListContent() {
        List<Integer> content = new ArrayList<>();
        content.add(NTP_CARDS);
        // TODO(crbug.com/397439004): Add logics to hide the "feeds" list item.
        content.add(DISCOVER_FEED);
        return content;
    }

    /** Clears maps */
    void destroy() {
        mViewFlipperMap.clear();
        mTypeToListenersMap.clear();
    }

    Map<Integer, Integer> getViewFlipperMapForTesting() {
        return mViewFlipperMap;
    }

    @NtpCustomizationCoordinator.BottomSheetType
    Integer getCurrentBottomSheetForTesting() {
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

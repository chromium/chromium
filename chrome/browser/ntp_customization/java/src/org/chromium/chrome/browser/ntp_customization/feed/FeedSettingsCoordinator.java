// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.feed;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.FEED_SETTINGS_KEYS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.FEED_SWITCH_ON_CHECKED_CHANGE_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_FEED_LIST_ITEMS_TITLE_VISIBLE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_FEED_SWITCH_CHECKED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LEARN_MORE_BUTTON_CLICK_LISTENER;

import android.content.Context;
import android.support.annotation.IntDef;
import android.support.annotation.VisibleForTesting;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.BottomSheetListContainerViewBinder;
import org.chromium.chrome.browser.ntp_customization.BottomSheetViewBinder;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Coordinator for the feed settings bottom sheet in the NTP customization. */
@NullMarked
public class FeedSettingsCoordinator {
    private FeedSettingsMediator mMediator;

    /** Feed management sections that are shown in the feed settings bottom sheet. */
    @IntDef({
        FeedSettingsBottomSheetSection.ACTIVITY,
        FeedSettingsBottomSheetSection.FOLLOWING,
        FeedSettingsBottomSheetSection.HIDDEN,
        FeedSettingsBottomSheetSection.INTERESTS,
        FeedSettingsBottomSheetSection.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FeedSettingsBottomSheetSection {
        int ACTIVITY = 0;
        int FOLLOWING = 1;
        int HIDDEN = 2;
        int INTERESTS = 3;
        int NUM_ENTRIES = 4;
    }

    public FeedSettingsCoordinator(Context context, BottomSheetDelegate delegate, Profile profile) {
        View view =
                LayoutInflater.from(context)
                        .inflate(R.layout.ntp_customization_feed_bottom_sheet, null, false);
        delegate.registerBottomSheetLayout(FEED, view);

        // The bottomSheetPropertyModel is responsible for managing the back press handler of the
        // back button in the bottom sheet.
        PropertyModel bottomSheetPropertyModel =
                new PropertyModel(NtpCustomizationViewProperties.BOTTOM_SHEET_KEYS);
        PropertyModelChangeProcessor.create(
                bottomSheetPropertyModel, view, BottomSheetViewBinder::bind);

        // The containerPropertyModel is responsible for managing a BottomSheetDelegate which
        // provides list content and event handlers to the list container view.
        PropertyModel containerPropertyModel =
                new PropertyModel(NtpCustomizationViewProperties.LIST_CONTAINER_KEYS);
        PropertyModelChangeProcessor.create(
                containerPropertyModel,
                view.findViewById(R.id.feed_list_items_container),
                BottomSheetListContainerViewBinder::bind);

        // The feedSettingsPropertyModel is responsible for managing the feed switch in the feed
        // settings bottom sheet.
        PropertyModel feedSettingsPropertyModel = new PropertyModel(FEED_SETTINGS_KEYS);
        PropertyModelChangeProcessor.create(
                feedSettingsPropertyModel,
                view,
                FeedSettingsCoordinator::bindFeedSettingsBottomSheet);

        mMediator =
                new FeedSettingsMediator(
                        containerPropertyModel,
                        bottomSheetPropertyModel,
                        feedSettingsPropertyModel,
                        delegate,
                        profile);
    }

    public void destroy() {
        mMediator.destroy();
    }

    /**
     * Handles the binding of display and interaction for the feed toggle control and feed list
     * items title in the feed settings bottom sheet.
     */
    @VisibleForTesting
    static void bindFeedSettingsBottomSheet(
            PropertyModel model, View view, PropertyKey propertyKey) {
        MaterialSwitchWithText feedSwitch = view.findViewById(R.id.feed_switch_button);
        if (propertyKey == FEED_SWITCH_ON_CHECKED_CHANGE_LISTENER) {
            feedSwitch.setOnCheckedChangeListener(
                    model.get(FEED_SWITCH_ON_CHECKED_CHANGE_LISTENER));
        } else if (propertyKey == IS_FEED_SWITCH_CHECKED) {
            feedSwitch.setChecked(model.get(IS_FEED_SWITCH_CHECKED));
        } else if (propertyKey == IS_FEED_LIST_ITEMS_TITLE_VISIBLE) {
            View feedListItemsTitle = view.findViewById(R.id.feed_list_items_title);
            feedListItemsTitle.setVisibility(
                    model.get(IS_FEED_LIST_ITEMS_TITLE_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == LEARN_MORE_BUTTON_CLICK_LISTENER) {
            ImageView learnMoreButton = view.findViewById(R.id.learn_more_button);
            learnMoreButton.setOnClickListener(model.get(LEARN_MORE_BUTTON_CLICK_LISTENER));
        }
    }

    FeedSettingsMediator getMediatorForTesting() {
        return mMediator;
    }

    void setMediatorForTesting(FeedSettingsMediator mediator) {
        mMediator = mediator;
    }
}

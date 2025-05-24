// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.feed;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.FEED_SWITCH_ON_CHECKED_CHANGE_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_FEED_LIST_ITEMS_TITLE_VISIBLE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_FEED_SWITCH_CHECKED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LEARN_MORE_BUTTON_CLICK_LISTENER;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.ImageView;
import android.widget.ViewFlipper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link FeedSettingsCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class FeedSettingsCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock private Profile mProfile;
    @Mock private PrefChangeRegistrar mPrefChangeRegistrar;
    @Mock private PrefService mPrefService;

    private Context mContext;
    private PropertyModel mPropertyModel;
    private FeedSettingsCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        FeedServiceBridgeJni.setInstanceForTesting(mFeedServiceBridgeJniMock);
        WebFeedBridgeJni.setInstanceForTesting(mWebFeedBridgeJniMock);
        FeedSettingsMediator.setPrefForTesting(mPrefChangeRegistrar, mPrefService);

        mCoordinator =
                new FeedSettingsCoordinator(
                        mContext, Mockito.mock(BottomSheetDelegate.class), mProfile);
        mPropertyModel = new PropertyModel(NtpCustomizationViewProperties.FEED_SETTINGS_KEYS);
    }

    @Test
    public void testConstructor() {
        assertNotNull(mCoordinator.getMediatorForTesting());
    }

    @Test
    public void testDestroy() {
        FeedSettingsMediator mediator = mock(FeedSettingsMediator.class);
        mCoordinator.setMediatorForTesting(mediator);

        mCoordinator.destroy();
        verify(mediator).destroy();
    }

    @Test
    public void testBindFeedSettingsBottomSheet() {
        // Adds the feed bottom sheet to the view of the activity.
        View contentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.ntp_customization_bottom_sheet, /* root= */ null);
        ViewFlipper viewFlipperView = contentView.findViewById(R.id.ntp_customization_view_flipper);
        View feedBottomSheet =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.ntp_customization_feed_bottom_sheet,
                                viewFlipperView,
                                true);
        PropertyModelChangeProcessor.create(
                mPropertyModel,
                feedBottomSheet,
                FeedSettingsCoordinator::bindFeedSettingsBottomSheet);

        // Verifies the on checked change listener is added to the feed bottom sheet's feed switch.
        OnCheckedChangeListener onCheckedChangeListener = mock(OnCheckedChangeListener.class);
        mPropertyModel.set(FEED_SWITCH_ON_CHECKED_CHANGE_LISTENER, onCheckedChangeListener);
        MaterialSwitchWithText feedSwitch = feedBottomSheet.findViewById(R.id.feed_switch_button);
        feedSwitch.setChecked(true);
        verify(onCheckedChangeListener)
                .onCheckedChanged(feedSwitch.findViewById(R.id.switch_widget), true);
        feedSwitch.setChecked(false);
        verify(onCheckedChangeListener)
                .onCheckedChanged(feedSwitch.findViewById(R.id.switch_widget), false);

        // Verifies the feed switch will get updated timely.
        Assert.assertFalse(feedSwitch.isChecked());
        mPropertyModel.set(IS_FEED_SWITCH_CHECKED, true);
        Assert.assertTrue(feedSwitch.isChecked());
        mPropertyModel.set(IS_FEED_SWITCH_CHECKED, false);
        Assert.assertFalse(feedSwitch.isChecked());

        // Verifies the feed list items title will get updated timely.
        View feedListItemsTitle = feedBottomSheet.findViewById(R.id.feed_list_items_title);
        mPropertyModel.set(IS_FEED_LIST_ITEMS_TITLE_VISIBLE, true);
        Assert.assertEquals(View.VISIBLE, feedListItemsTitle.getVisibility());
        mPropertyModel.set(IS_FEED_LIST_ITEMS_TITLE_VISIBLE, false);
        Assert.assertEquals(View.GONE, feedListItemsTitle.getVisibility());

        // Verifies that the onClickListener is added to the learn-more button on feed bottom sheet.
        View.OnClickListener onClickListener = mock(View.OnClickListener.class);
        mPropertyModel.set(LEARN_MORE_BUTTON_CLICK_LISTENER, onClickListener);
        ImageView learnMoreButton = feedBottomSheet.findViewById(R.id.learn_more_button);
        learnMoreButton.performClick();
        verify(onClickListener).onClick(eq(learnMoreButton));
    }
}

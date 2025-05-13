// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.feed;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.FEED_SWITCH_ON_CHECKED_CHANGE_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_FEED_LIST_ITEMS_TITLE_VISIBLE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_FEED_SWITCH_CHECKED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LEARN_MORE_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;
import static org.chromium.chrome.browser.ntp_customization.feed.FeedSettingsCoordinator.FeedSettingsBottomSheetSection.ACTIVITY;
import static org.chromium.chrome.browser.ntp_customization.feed.FeedSettingsCoordinator.FeedSettingsBottomSheetSection.FOLLOWING;
import static org.chromium.chrome.browser.ntp_customization.feed.FeedSettingsCoordinator.FeedSettingsBottomSheetSection.HIDDEN;
import static org.chromium.chrome.browser.ntp_customization.feed.FeedSettingsCoordinator.FeedSettingsBottomSheetSection.INTERESTS;
import static org.chromium.chrome.browser.ntp_customization.feed.FeedSettingsMediator.handleLearnMoreClick;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.view.View;
import android.widget.CompoundButton.OnCheckedChangeListener;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.ListContainerViewDelegate;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link FeedSettingsMediator} */
@RunWith(BaseRobolectricTestRunner.class)
public class FeedSettingsMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock private PropertyModel mContainerPropertyModel;
    @Mock private PropertyModel mBottomSheetPropertyModel;
    @Mock private PropertyModel mFeedSettingsPropertyModel;
    @Mock private BottomSheetDelegate mDelegate;
    @Mock private Profile mProfile;
    @Mock private PrefChangeRegistrar mPrefChangeRegistrar;
    @Mock private PrefService mPrefService;
    @Mock View mView;
    @Captor private ArgumentCaptor<View.OnClickListener> mBackPressHandlerCaptor;

    private FeedSettingsMediator mFeedSettingsMediator;
    private Context mContext;
    private Activity mActivity;
    private ShadowActivity mShadowActivity;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mShadowActivity = Shadows.shadowOf(mActivity);
        FeedServiceBridgeJni.setInstanceForTesting(mFeedServiceBridgeJniMock);
        WebFeedBridgeJni.setInstanceForTesting(mWebFeedBridgeJniMock);
        FeedSettingsMediator.setPrefForTesting(mPrefChangeRegistrar, mPrefService);
        mFeedSettingsMediator =
                new FeedSettingsMediator(
                        mContainerPropertyModel,
                        mBottomSheetPropertyModel,
                        mFeedSettingsPropertyModel,
                        mDelegate,
                        mProfile);
    }

    @Test
    public void testConstructor() {
        verify(mContainerPropertyModel)
                .set(eq(LIST_CONTAINER_VIEW_DELEGATE), any(ListContainerViewDelegate.class));

        verify(mFeedSettingsPropertyModel).set(eq(IS_FEED_LIST_ITEMS_TITLE_VISIBLE), anyBoolean());
        verify(mFeedSettingsPropertyModel)
                .set(
                        eq(FEED_SWITCH_ON_CHECKED_CHANGE_LISTENER),
                        any(OnCheckedChangeListener.class));
        verify(mFeedSettingsPropertyModel).set(eq(IS_FEED_SWITCH_CHECKED), anyBoolean());
        verify(mFeedSettingsPropertyModel)
                .set(eq(LEARN_MORE_BUTTON_CLICK_LISTENER), any(View.OnClickListener.class));
    }

    @Test
    public void testBackPressHandler() {
        // Verifies that when the feed settings bottom sheet should show alone, the back press
        // handler should be set to null.
        when(mDelegate.shouldShowAlone()).thenReturn(true);
        new FeedSettingsMediator(
                mContainerPropertyModel,
                mBottomSheetPropertyModel,
                mFeedSettingsPropertyModel,
                mDelegate,
                mProfile);
        verify(mBottomSheetPropertyModel).set(BACK_PRESS_HANDLER, null);

        // Verifies that when the feed settings bottom sheet is part of the navigation flow starting
        // from the main bottom sheet, and the back press handler should be set to
        // backPressOnCurrentBottomSheet()
        clearInvocations(mBottomSheetPropertyModel);
        when(mDelegate.shouldShowAlone()).thenReturn(false);
        new FeedSettingsMediator(
                mContainerPropertyModel,
                mBottomSheetPropertyModel,
                mFeedSettingsPropertyModel,
                mDelegate,
                mProfile);
        verify(mBottomSheetPropertyModel)
                .set(eq(BACK_PRESS_HANDLER), mBackPressHandlerCaptor.capture());
        mBackPressHandlerCaptor.getValue().onClick(mView);
        verify(mDelegate).backPressOnCurrentBottomSheet();
    }

    @Test
    public void testDestroy() {
        mFeedSettingsMediator.destroy();
        verify(mPrefChangeRegistrar).removeObserver(Pref.ARTICLES_LIST_VISIBLE);
        verify(mBottomSheetPropertyModel).set(eq(BACK_PRESS_HANDLER), eq(null));
        verify(mContainerPropertyModel).set(eq(LIST_CONTAINER_VIEW_DELEGATE), eq(null));
    }

    @Test
    public void testOnFeedSwitchToggled() {
        mFeedSettingsMediator.onFeedSwitchToggled(/* isChecked= */ true);
        verify(mPrefService).setBoolean(eq(Pref.ARTICLES_LIST_VISIBLE), eq(true));

        mFeedSettingsMediator.onFeedSwitchToggled(/* isChecked= */ false);
        verify(mPrefService).setBoolean(eq(Pref.ARTICLES_LIST_VISIBLE), eq(false));
    }

    @Test
    public void testUpdateFeedSwitch() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        mFeedSettingsMediator.updateFeedSwitch();
        verify(mFeedSettingsPropertyModel).set(eq(IS_FEED_SWITCH_CHECKED), eq(true));

        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);
        mFeedSettingsMediator.updateFeedSwitch();
        verify(mFeedSettingsPropertyModel, times(2)).set(eq(IS_FEED_SWITCH_CHECKED), eq(false));
    }

    @Test
    public void testCreateListContainerViewDelegate() {
        // Verifies that there is no sections when user has not signed in.
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(false);
        mFeedSettingsMediator.setListItemsContentForTesting(
                mFeedSettingsMediator.buildFeedListContent());
        ListContainerViewDelegate delegateForNotSignedIn =
                mFeedSettingsMediator.createListDelegate();
        Assert.assertTrue(delegateForNotSignedIn.getListItems().isEmpty());

        // Verifies the sections when user has signed in and web feed is enabled.
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mWebFeedBridgeJniMock.isWebFeedEnabled()).thenReturn(true);
        mFeedSettingsMediator.setListItemsContentForTesting(
                mFeedSettingsMediator.buildFeedListContent());
        ListContainerViewDelegate delegateForWebFeedEnabled =
                mFeedSettingsMediator.createListDelegate();
        List<Integer> content = delegateForWebFeedEnabled.getListItems();
        Assert.assertTrue(content.contains(ACTIVITY));
        Assert.assertTrue(content.contains(FOLLOWING));
        Assert.assertTrue(content.contains(HIDDEN));

        // Verifies the sections when user has signed in and web feed is disabled.
        when(mWebFeedBridgeJniMock.isWebFeedEnabled()).thenReturn(false);
        mFeedSettingsMediator.setListItemsContentForTesting(
                mFeedSettingsMediator.buildFeedListContent());
        ListContainerViewDelegate delegateForWebFeedDisabled =
                mFeedSettingsMediator.createListDelegate();
        content = delegateForWebFeedDisabled.getListItems();
        Assert.assertTrue(content.contains(ACTIVITY));
        Assert.assertTrue(content.contains(INTERESTS));

        testCreateListContainerViewDelegateImplForSectionTitle(
                delegateForWebFeedEnabled, delegateForWebFeedDisabled);

        testCreateListContainerViewDelegateImplForSectionSubtitle(
                delegateForWebFeedEnabled, delegateForWebFeedDisabled);

        testCreateListContainerViewDelegateImplForSectionListener(
                delegateForWebFeedEnabled, delegateForWebFeedDisabled);
    }

    @Test
    public void testHandleLearnMoreClick() {
        when(mView.getContext()).thenReturn(mActivity);
        handleLearnMoreClick(mView);
        Intent intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(intent.getData(), Uri.parse("https://support.google.com/chrome/?p=new_tab"));
    }

    /** Verifies that the subtitles of sections are correct. */
    private void testCreateListContainerViewDelegateImplForSectionSubtitle(
            ListContainerViewDelegate delegateForWebFeedEnabled,
            ListContainerViewDelegate delegateForWebFeedDisabled) {
        assertEquals(
                mContext.getString(R.string.feed_manage_activity_description),
                delegateForWebFeedEnabled.getListItemSubtitle(ACTIVITY, mContext));
        assertEquals(
                mContext.getString(R.string.feed_manage_following_description),
                delegateForWebFeedEnabled.getListItemSubtitle(FOLLOWING, mContext));
        assertEquals(
                mContext.getString(R.string.feed_manage_hidden_description),
                delegateForWebFeedEnabled.getListItemSubtitle(HIDDEN, mContext));
        assertEquals(
                mContext.getString(R.string.feed_manage_interests_description),
                delegateForWebFeedDisabled.getListItemSubtitle(INTERESTS, mContext));
    }

    /** Verifies that the titles of sections are correct. */
    private void testCreateListContainerViewDelegateImplForSectionTitle(
            ListContainerViewDelegate delegateForWebFeedEnabled,
            ListContainerViewDelegate delegateForWebFeedDisabled) {
        assertEquals(
                mContext.getString(R.string.feed_manage_activity),
                delegateForWebFeedEnabled.getListItemTitle(ACTIVITY, mContext));
        assertEquals(
                mContext.getString(R.string.feed_manage_following),
                delegateForWebFeedEnabled.getListItemTitle(FOLLOWING, mContext));
        assertEquals(
                mContext.getString(R.string.feed_manage_hidden),
                delegateForWebFeedEnabled.getListItemTitle(HIDDEN, mContext));
        assertEquals(
                mContext.getString(R.string.feed_manage_interests),
                delegateForWebFeedDisabled.getListItemTitle(INTERESTS, mContext));
    }

    /** Verifies that the click listener of sections are correct. */
    private void testCreateListContainerViewDelegateImplForSectionListener(
            ListContainerViewDelegate delegateForWebFeedEnabled,
            ListContainerViewDelegate delegateForWebFeedDisabled) {
        when(mView.getContext()).thenReturn(mActivity);

        // Verifies the click listener is correct for Activity section.
        delegateForWebFeedEnabled.getListener(ACTIVITY).onClick(mView);
        Intent intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(
                intent.getData(), Uri.parse("https://myactivity.google.com/myactivity?product=50"));

        // Verifies the click listener is correct for Following section.
        delegateForWebFeedEnabled.getListener(FOLLOWING).onClick(mView);
        intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(
                intent.getData(),
                Uri.parse("https://www.google.com/preferences/interests/yourinterests?sh=n"));

        // Verifies the click listener is correct for Hidden section.
        delegateForWebFeedEnabled.getListener(HIDDEN).onClick(mView);
        intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(
                intent.getData(),
                Uri.parse("https://www.google.com/preferences/interests/hidden?sh=n"));

        // Verifies the click listener is correct for Interests section.
        delegateForWebFeedDisabled.getListener(INTERESTS).onClick(mView);
        intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(intent.getData(), Uri.parse("https://www.google.com/preferences/interests"));
    }
}

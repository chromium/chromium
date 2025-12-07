// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.argThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Page;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link PrivacySandbox3pcdRollbackMessageController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.ROLL_BACK_MODE_B)
public class PrivacySandbox3pcdRollbackMessageControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TrackingProtectionSettingsBridge.Natives mTrackingProtectionSettingsBridgeJni;
    @Mock UserPrefs.Natives mUserPrefsJniMock;

    @Mock PrefService mPrefService;
    @Mock private Tab mTab;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private Context mContext;
    @Mock private MessageDispatcher mMessageDispatcher;
    @Mock private Profile mProfile;

    private PrivacySandbox3pcdRollbackMessageController mController;

    private PropertyModel showMessage() {
        when(mPrefService.getBoolean(Pref.SHOW_ROLLBACK_UI_MODE_B)).thenReturn(true);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);

        Assert.assertTrue(mController.maybeShow());
        verify(mMessageDispatcher).enqueueWindowScopedMessage(modelCaptor.capture(), eq(true));
        return modelCaptor.getValue();
    }

    private void verifyHistograms(
            PropertyModel model,
            @RollBack3pcdNoticeAction int action,
            @DismissReason int dismissReason) {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Privacy.3PCD.RollbackNotice.Action", action)
                        .expectBooleanRecord(
                                "Privacy.3PCD.RollbackNotice.AutomaticallyDismissed", false)
                        .build();
        model.get(MessageBannerProperties.ON_DISMISSED).onResult(dismissReason);
        watcher.assertExpected();
    }

    private void navigate(boolean hasCommitted) {
        NavigationHandle navigation =
                NavigationHandle.createForTesting(JUnitTestGURLs.EXAMPLE_URL, false, 0, false);
        navigation.didFinish(
                JUnitTestGURLs.EXAMPLE_URL,
                /* isErrorPage= */ false,
                hasCommitted,
                /* isPrimaryMainFrameFragmentNavigation= */ false,
                /* isDownload= */ false,
                /* isValidSearchFormUrl= */ false,
                /* transition= */ 0,
                /* errorCode= */ 0,
                /* httpStatuscode= */ 200,
                /* isExternalProtocol= */ false,
                /* isPdf= */ false,
                /* mimeType= */ "",
                Page.createForTesting());
        ActivityTabTabObserver observer = assertNonNull(mController.getActivityTabTabObserver());
        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, navigation);
    }

    @Before
    public void setUp() {
        doReturn(Mockito.mock(Resources.class)).when(mContext).getResources();
        TrackingProtectionSettingsBridgeJni.setInstanceForTesting(
                mTrackingProtectionSettingsBridgeJni);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

        mController =
                new PrivacySandbox3pcdRollbackMessageController(
                        mContext, mProfile, mActivityTabProvider, mMessageDispatcher);
    }

    @Test
    public void maybeShow_doesNotShowForOtrProfile() {
        when(mPrefService.getBoolean(Pref.SHOW_ROLLBACK_UI_MODE_B)).thenReturn(true);
        when(mProfile.isOffTheRecord()).thenReturn(true);
        Assert.assertFalse(mController.maybeShow());
        verify(mMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    public void maybeShow_doesNotShowWhenPrefFalse() {
        when(mPrefService.getBoolean(Pref.SHOW_ROLLBACK_UI_MODE_B)).thenReturn(false);
        Assert.assertFalse(mController.maybeShow());
        verify(mMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    public void maybeShow_showsOnNavigationWhenCommitted() {
        when(mPrefService.getBoolean(Pref.SHOW_ROLLBACK_UI_MODE_B)).thenReturn(true);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        navigate(/* hasCommitted= */ true);
        verify(mMessageDispatcher, times(1)).enqueueWindowScopedMessage(any(), eq(true));
    }

    @Test
    public void maybeShow_doesNotShowOnNavigationWhenNotCommitted() {
        when(mPrefService.getBoolean(Pref.SHOW_ROLLBACK_UI_MODE_B)).thenReturn(true);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        navigate(/* hasCommitted= */ false);
        verify(mMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    public void maybeShow_verifyMessageProperties() {
        PropertyModel model = showMessage();
        // Verify ID, icon, description, and primary button.
        Assert.assertEquals(
                MessageIdentifier.MODE_B_ROLLBACK_MESSAGE,
                model.get(MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals(
                R.drawable.cookie_24dp, model.get(MessageBannerProperties.ICON_RESOURCE_ID));
        Assert.assertEquals(
                mContext.getString(R.string.mode_b_rollback_description),
                model.get(MessageBannerProperties.DESCRIPTION));
        Assert.assertEquals(
                mContext.getString(R.string.mode_b_rollback_got_it),
                model.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
        Assert.assertEquals(
                Integer.valueOf(PrimaryActionClickBehavior.DISMISS_IMMEDIATELY),
                model.get(MessageBannerProperties.ON_PRIMARY_ACTION).get());
        // Verify settings button icon and behavior (dismisses message and launches 3PC settings).
        Assert.assertEquals(
                R.drawable.ic_settings_gear_24dp,
                model.get(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID));
        model.get(MessageBannerProperties.ON_SECONDARY_ACTION).run();
        verify(mMessageDispatcher).dismissMessage(eq(model), eq(DismissReason.SECONDARY_ACTION));
        verify(mSettingsNavigation)
                .createSettingsIntent(
                        any(),
                        eq(SingleCategorySettings.class),
                        argThat(
                                fragmentArgs -> {
                                    return fragmentArgs
                                            .getString(SingleCategorySettings.EXTRA_CATEGORY)
                                            .equals("third_party_cookies");
                                }));
    }

    @Test
    public void maybeShow_setsPrefWhenMadeVisible() {
        PropertyModel model = showMessage();
        // Does not set pref when not fully visible.
        model.get(MessageBannerProperties.ON_FULLY_VISIBLE).onResult(false);
        verify(mPrefService, never()).setBoolean(eq(Pref.SHOW_ROLLBACK_UI_MODE_B), anyBoolean());
        // Sets pref when fully visible.
        model.get(MessageBannerProperties.ON_FULLY_VISIBLE).onResult(true);
        verify(mPrefService).setBoolean(eq(Pref.SHOW_ROLLBACK_UI_MODE_B), eq(false));
    }

    @Test
    public void dismissalActionHistogram_recordsGotItOnPrimaryAction() {
        PropertyModel model = showMessage();
        verifyHistograms(model, RollBack3pcdNoticeAction.GOT_IT, DismissReason.PRIMARY_ACTION);
    }

    @Test
    public void dismissalActionHistogram_recordsSettingsOnSecondaryAction() {
        PropertyModel model = showMessage();
        verifyHistograms(model, RollBack3pcdNoticeAction.SETTINGS, DismissReason.SECONDARY_ACTION);
    }

    @Test
    public void dismissalActionHistogram_recordsClosedOnGesture() {
        PropertyModel model = showMessage();
        verifyHistograms(model, RollBack3pcdNoticeAction.CLOSED, DismissReason.GESTURE);
    }

    @Test
    public void dismissalActionHistogram_recordsClosedOnCloseButton() {
        PropertyModel model = showMessage();
        verifyHistograms(model, RollBack3pcdNoticeAction.CLOSED, DismissReason.CLOSE_BUTTON);
    }

    @Test
    public void dismissalActionHistogram_recordsNoneOnTimer() {
        PropertyModel model = showMessage();
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Privacy.3PCD.RollbackNotice.AutomaticallyDismissed", true);
        model.get(MessageBannerProperties.ON_DISMISSED).onResult(DismissReason.TIMER);
        watcher.assertExpected();
    }

    @Test
    public void dismissalActionHistogram_recordsNoneOnScopeDestroyed() {
        PropertyModel model = showMessage();
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Privacy.3PCD.RollbackNotice.AutomaticallyDismissed", true);
        model.get(MessageBannerProperties.ON_DISMISSED).onResult(DismissReason.SCOPE_DESTROYED);
        watcher.assertExpected();
    }
}

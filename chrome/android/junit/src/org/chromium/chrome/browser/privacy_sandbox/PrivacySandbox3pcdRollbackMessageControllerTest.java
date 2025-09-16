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
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
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
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link PrivacySandbox3pcdRollbackMessageController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.ROLL_BACK_MODE_B)
public class PrivacySandbox3pcdRollbackMessageControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock UserPrefs.Natives mUserPrefsJniMock;
    @Mock PrefService mPrefService;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private Context mContext;
    @Mock private MessageDispatcher mMessageDispatcher;
    @Mock private Profile mProfile;

    @Before
    public void before() {
        doReturn(Mockito.mock(Resources.class)).when(mContext).getResources();
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
    }

    @Test
    public void maybeShow_doesNotShowForOtrProfile() {
        when(mPrefService.getBoolean(Pref.SHOW_ROLLBACK_UI_MODE_B)).thenReturn(true);
        when(mProfile.isOffTheRecord()).thenReturn(true);
        Assert.assertFalse(
                PrivacySandbox3pcdRollbackMessageController.maybeShow(
                        mContext, mProfile, mMessageDispatcher));
        verify(mMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    public void maybeShow_doesNotShowWhenPrefFalse() {
        when(mPrefService.getBoolean(Pref.SHOW_ROLLBACK_UI_MODE_B)).thenReturn(false);
        Assert.assertFalse(
                PrivacySandbox3pcdRollbackMessageController.maybeShow(
                        mContext, mProfile, mMessageDispatcher));
        verify(mMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    public void maybeShow_verifyMessageProperties() {
        when(mPrefService.getBoolean(Pref.SHOW_ROLLBACK_UI_MODE_B)).thenReturn(true);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);

        Assert.assertTrue(
                PrivacySandbox3pcdRollbackMessageController.maybeShow(
                        mContext, mProfile, mMessageDispatcher));
        verify(mMessageDispatcher).enqueueWindowScopedMessage(modelCaptor.capture(), eq(true));
        PropertyModel model = modelCaptor.getValue();

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
        when(mPrefService.getBoolean(Pref.SHOW_ROLLBACK_UI_MODE_B)).thenReturn(true);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);

        Assert.assertTrue(
                PrivacySandbox3pcdRollbackMessageController.maybeShow(
                        mContext, mProfile, mMessageDispatcher));
        verify(mMessageDispatcher).enqueueWindowScopedMessage(modelCaptor.capture(), eq(true));
        PropertyModel model = modelCaptor.getValue();

        // Does not set pref when not fully visible.
        model.get(MessageBannerProperties.ON_FULLY_VISIBLE).onResult(false);
        verify(mPrefService, never()).setBoolean(eq(Pref.SHOW_ROLLBACK_UI_MODE_B), anyBoolean());
        // Sets pref when fully visible.
        model.get(MessageBannerProperties.ON_FULLY_VISIBLE).onResult(true);
        verify(mPrefService).setBoolean(eq(Pref.SHOW_ROLLBACK_UI_MODE_B), eq(false));
    }
}

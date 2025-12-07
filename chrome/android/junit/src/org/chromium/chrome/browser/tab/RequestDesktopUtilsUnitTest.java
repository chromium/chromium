// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link RequestDesktopUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class RequestDesktopUtilsUnitTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public OverrideContextWrapperTestRule mOverrideContextWrapperTestRule =
            new OverrideContextWrapperTestRule();

    @Mock private WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock private MessageDispatcher mMessageDispatcher;
    @Mock private Activity mActivity;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private UserPrefs.Natives mUserPrefsJni;

    private SharedPreferencesManager mSharedPreferencesManager;
    private Resources mResources;

    @Before
    public void setup() {
        WebsitePreferenceBridgeJni.setInstanceForTesting(mWebsitePreferenceBridgeJniMock);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);

        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        TrackerFactory.setTrackerForTests(mTracker);

        mResources = ApplicationProvider.getApplicationContext().getResources();
        when(mActivity.getResources()).thenReturn(mResources);
    }

    @Test
    public void testMaybeShowDefaultEnableGlobalSettingMessage() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.REQUEST_DESKTOP_SITE_DEFAULT_ON_FEATURE))
                .thenReturn(true);

        // Default-enable the global setting before the message is shown.
        WebsitePreferenceBridge.setCategoryEnabled(
                mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE, true);
        // Preference is set when the setting is default-enabled.
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING, true);

        when(mWebsitePreferenceBridgeJniMock.isContentSettingEnabled(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(true);
        RequestDesktopUtils.maybeShowDefaultEnableGlobalSettingMessage(
                mProfile, mMessageDispatcher, mActivity);

        ArgumentCaptor<PropertyModel> message = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher).enqueueWindowScopedMessage(message.capture(), eq(false));
        Assert.assertEquals(
                "Message identifier should match.",
                MessageIdentifier.DESKTOP_SITE_GLOBAL_DEFAULT_OPT_OUT,
                message.getValue().get(MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals(
                "Message title should match.",
                mResources.getString(R.string.rds_global_default_on_message_title),
                message.getValue().get(MessageBannerProperties.TITLE));
        Assert.assertEquals(
                "Message primary button text should match.",
                mResources.getString(R.string.rds_global_default_on_message_button),
                message.getValue().get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
        Assert.assertEquals(
                "Message icon resource ID should match.",
                R.drawable.ic_desktop_windows,
                message.getValue().get(MessageBannerProperties.ICON_RESOURCE_ID));
    }

    @Test
    public void testMaybeShowDefaultEnableGlobalSettingMessage_DoNotShowIfSettingIsDisabled() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.REQUEST_DESKTOP_SITE_DEFAULT_ON_FEATURE))
                .thenReturn(true);

        // Preference is set when the setting is default-enabled.
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING, true);

        // Simulate disabling of the setting by the user before the message is shown.
        when(mWebsitePreferenceBridgeJniMock.isContentSettingEnabled(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(false);

        boolean shown =
                RequestDesktopUtils.maybeShowDefaultEnableGlobalSettingMessage(
                        mProfile, mMessageDispatcher, mActivity);
        Assert.assertFalse(
                "Message should not be shown if the content setting is disabled.", shown);
    }

    @Test
    public void testMaybeShowDefaultEnableGlobalSettingMessage_DoNotShowIfDesktopAndroid() {
        mOverrideContextWrapperTestRule.setIsDesktop(true);

        boolean shown =
                RequestDesktopUtils.maybeShowDefaultEnableGlobalSettingMessage(
                        mProfile, mMessageDispatcher, mActivity);
        Assert.assertFalse("Message should not be shown for desktop Android.", shown);
    }
}

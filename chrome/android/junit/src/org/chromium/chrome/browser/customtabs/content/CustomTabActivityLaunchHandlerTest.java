// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.INITIAL_URL;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.OTHER_URL;

import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.autofill.AndroidAutofillFeatures;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.Objects;

/**
 * Integration tests involving several classes in Custom Tabs content layer, checking that Launch
 * Handler API works in different conditions.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({
    ChromeFeatureList.CCT_EARLY_NAV,
    ChromeFeatureList.CCT_PREWARM_TAB,
    ChromeFeatureList.ANDROID_WEB_APP_LAUNCH_HANDLER,
    AndroidAutofillFeatures.ANDROID_AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID_IN_CCT_NAME
})
public class CustomTabActivityLaunchHandlerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    protected CustomTabActivityTabController mTabController;
    protected CustomTabIntentHandler mIntentHandler;
    @Mock CustomTabActivityNavigationController mNavigationController;
    @Mock WebAppLaunchHandler.Natives mWebAppLaunchHandlerJniMock;
    @Mock private UserPrefsJni mMockUserPrefsJni;

    @Before
    public void setUp() {
        WebAppLaunchHandlerJni.setInstanceForTesting(mWebAppLaunchHandlerJniMock);
        UserPrefsJni.setInstanceForTesting(mMockUserPrefsJni);
        doReturn(mock(PrefService.class)).when(mMockUserPrefsJni).get(any());

        // Ensure the test can read the Autofill pref. Assume it's turned off by default.
        AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF);

        mTabController = env.createTabController();

        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        clearInvocations(env.tabFromFactory);
        when(env.intentDataProvider.getActivityType())
                .thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
    }

    private void doTestLaunchHandler(
            int expectedNotifyQueueTimes, CustomTabIntentDataProvider dataProvider) {

        if (Objects.equals(dataProvider.getUrlToLoad(), INITIAL_URL)) {
            mIntentHandler = env.createIntentHandler(mNavigationController);
        } else {
            mIntentHandler.onNewIntent(dataProvider);
        }

        shadowOf(Looper.getMainLooper()).idle();
        verify(mNavigationController, times(1)).navigate(any(), any());

        verify(mWebAppLaunchHandlerJniMock, times(expectedNotifyQueueTimes))
                .notifyLaunchQueue(any(), anyBoolean(), any(), any(), any());
    }

    private CustomTabIntentDataProvider createIntentDataProvider() {
        CustomTabIntentDataProvider dataProvider = mock(CustomTabIntentDataProvider.class);
        when(dataProvider.getSession()).thenReturn(env.session);
        when(dataProvider.getUrlToLoad()).thenReturn(OTHER_URL);
        return dataProvider;
    }

    @Test
    public void noTrustedWebActivityNoLaunchHandler() {
        when(env.intentDataProvider.getActivityType()).thenReturn(ActivityType.TABBED);
        doTestLaunchHandler(0, env.intentDataProvider);

        CustomTabIntentDataProvider dataProvider = createIntentDataProvider();
        when(dataProvider.getActivityType()).thenReturn(ActivityType.TABBED);

        doTestLaunchHandler(0, dataProvider);
    }

    @Test
    public void trustedWebActivityLaunchHandler() {
        doTestLaunchHandler(1, env.intentDataProvider);
        CustomTabIntentDataProvider dataProvider = createIntentDataProvider();
        doTestLaunchHandler(1, dataProvider);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.ANDROID_WEB_APP_LAUNCH_HANDLER})
    public void disabledLaunchHandler() {
        doTestLaunchHandler(0, env.intentDataProvider);

        CustomTabIntentDataProvider dataProvider = createIntentDataProvider();

        doTestLaunchHandler(0, dataProvider);
    }
}

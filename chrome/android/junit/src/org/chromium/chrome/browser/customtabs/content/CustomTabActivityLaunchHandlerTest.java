// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.CONTENT_URI;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.INITIAL_URL;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.OTHER_URL;

import android.content.Intent;
import android.net.Uri;
import android.os.Looper;

import androidx.browser.trusted.FileHandlingData;
import androidx.browser.trusted.LaunchHandlerClientMode;

import org.junit.Assert;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.autofill.AndroidAutofillFeatures;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.ArrayList;
import java.util.Arrays;

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
    private CustomTabActivityNavigationController mNavigationController;
    protected CustomTabIntentHandler mIntentHandler;
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
        mNavigationController = env.createNavigationController(mTabController);

        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        clearInvocations(env.tabFromFactory);
    }

    private void checkLaunchHandler(
            Integer clientMode, int expectedLoadUrlNumber, boolean expectedStartNewNavigation) {
        if (clientMode != null) {
            when(env.intentDataProvider.getLaunchHandlerClientMode()).thenReturn(clientMode);
        }
        mIntentHandler = env.createIntentHandler(mNavigationController);

        CustomTabIntentDataProvider intentDataProvider = createDataProviderForNewIntent(clientMode);
        Assert.assertEquals(
                env.intentDataProvider.getLaunchHandlerClientMode(),
                intentDataProvider.getLaunchHandlerClientMode());

        mIntentHandler.onNewIntent(intentDataProvider);
        shadowOf(Looper.getMainLooper()).idle();
        verify(env.tabFromFactory, times(1))
                .loadUrl(argThat(params -> INITIAL_URL.equals(params.getUrl())));
        verify(env.tabFromFactory, times(expectedLoadUrlNumber))
                .loadUrl(argThat(params -> OTHER_URL.equals(params.getUrl())));
        verify(mWebAppLaunchHandlerJniMock, times(1))
                .notifyLaunchQueue(any(), eq(true), eq(INITIAL_URL), eq(null), eq(new String[0]));
        verify(mWebAppLaunchHandlerJniMock, times(1))
                .notifyLaunchQueue(
                        any(),
                        eq(expectedStartNewNavigation),
                        eq(OTHER_URL),
                        eq(null),
                        eq(new String[0]));
    }

    @Test
    public void focusExistingClientMode() {
        checkLaunchHandler(
                LaunchHandlerClientMode.FOCUS_EXISTING,
                /* expectedLoadUrlNumber= */ 0,
                /* expectedStartNewNavigation= */ false);
    }

    @Test
    public void navigateExistingClientMode() {
        checkLaunchHandler(
                LaunchHandlerClientMode.NAVIGATE_EXISTING,
                /* expectedLoadUrlNumber= */ 1,
                /* expectedStartNewNavigation= */ true);
    }

    @Test
    public void autoClientMode() {
        // The user agent(browser) decides what works best for the platform. Currently it's
        // navigate-existing.
        checkLaunchHandler(
                LaunchHandlerClientMode.AUTO,
                /* expectedLoadUrlNumber= */ 1,
                /* expectedStartNewNavigation= */ true);
    }

    @Test
    public void wrongClientMode() {
        // Fallback to auto mode as described in the specification.
        final int wrongClientMode = 98;
        checkLaunchHandler(
                wrongClientMode,
                /* expectedLoadUrlNumber= */ 1,
                /* expectedStartNewNavigation= */ true);
    }

    @Test
    public void navigateNewClientMode() {
        // Treated by IntentHandler as a wrong mode because this mode should be handled earlier by
        // LaunchIntentDispatcher because it require launching of a new task.
        checkLaunchHandler(
                LaunchHandlerClientMode.NAVIGATE_NEW,
                /* expectedLoadUrlNumber= */ 1,
                /* expectedStartNewNavigation= */ true);
    }

    @Test
    public void noClientMode() {
        // According to the specification if not specified, the default client_mode value is auto.
        checkLaunchHandler(
                /* clientMode= */ null,
                /* expectedLoadUrlNumber= */ 1,
                /* expectedStartNewNavigation= */ true);
    }

    private FileHandlingData createFileHandlingData() {
        ArrayList<Uri> sampleList = new ArrayList<>(Arrays.asList(Uri.parse(CONTENT_URI)));
        return new FileHandlingData(sampleList);
    }

    @Test
    public void checkFileHandling() {
        mIntentHandler = env.createIntentHandler(mNavigationController);

        CustomTabIntentDataProvider intentDataProvider = createDataProviderForNewIntent(null);
        when(intentDataProvider.getFileHandlingData()).thenReturn(createFileHandlingData());

        mIntentHandler.onNewIntent(intentDataProvider);
        shadowOf(Looper.getMainLooper()).idle();
        verify(env.tabFromFactory, times(1))
                .loadUrl(argThat(params -> OTHER_URL.equals(params.getUrl())));
        verify(mWebAppLaunchHandlerJniMock, times(1))
                .notifyLaunchQueue(any(), eq(true), eq(INITIAL_URL), eq(null), eq(new String[0]));
        verify(mWebAppLaunchHandlerJniMock, times(1))
                .notifyLaunchQueue(
                        any(), eq(true), eq(OTHER_URL), eq(null), eq(new String[] {CONTENT_URI}));
    }

    private CustomTabIntentDataProvider createDataProviderForNewIntent(Integer clientMode) {
        CustomTabIntentDataProvider dataProvider = mock(CustomTabIntentDataProvider.class);
        when(dataProvider.getUrlToLoad()).thenReturn(OTHER_URL);
        when(dataProvider.getSession()).thenReturn(env.session);
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(OTHER_URL));

        if (clientMode != null) {
            when(dataProvider.getLaunchHandlerClientMode()).thenReturn(clientMode);
        }

        when(dataProvider.getIntent()).thenReturn(intent);
        return dataProvider;
    }
}

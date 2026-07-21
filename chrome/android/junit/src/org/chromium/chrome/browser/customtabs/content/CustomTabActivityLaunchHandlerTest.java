// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.INITIAL_URL;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.OTHER_URL;

import android.net.Uri;
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
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaIntentHandlingStrategy;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.sharing.TwaSharingController;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.Objects;

/**
 * Integration tests involving several classes in Custom Tabs content layer, checking that Launch
 * Handler API works in different conditions.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
        when(env.intentDataProvider.getClientPackageName()).thenReturn("test.package.name");
    }

    private void doTestLaunchHandler(
            int expectedNotifyQueueTimes, CustomTabIntentDataProvider dataProvider) {
        clearInvocations(mWebAppLaunchHandlerJniMock, mNavigationController);

        if (Objects.equals(dataProvider.getUrlToLoad(), INITIAL_URL)) {
            mIntentHandler = env.createIntentHandler(mNavigationController);
        } else {
            mIntentHandler.onNewIntent(dataProvider);
        }

        shadowOf(Looper.getMainLooper()).idle();
        verify(mNavigationController, times(1)).navigate(any(), any());

        String url = dataProvider.getUrlToLoad();
        String expectedScope = ShortcutHelper.getScopeFromUrl(url);
        if (expectedScope == null) {
            expectedScope = Uri.parse(url).buildUpon().path("").clearQuery().build().toString();
        }

        if (expectedNotifyQueueTimes > 0) {
            verify(mWebAppLaunchHandlerJniMock, times(expectedNotifyQueueTimes))
                    .prepareForLaunch(
                            any(),
                            anyLong(),
                            eq(url),
                            eq("test.package.name"),
                            any(),
                            any(),
                            eq(expectedScope),
                            anyBoolean());
            verify(mWebAppLaunchHandlerJniMock, times(expectedNotifyQueueTimes))
                    .onLaunchVerified(any(), anyLong(), eq(true));
        } else {
            verify(mWebAppLaunchHandlerJniMock, times(0))
                    .prepareForLaunch(
                            any(), anyLong(), any(), any(), any(), any(), any(), anyBoolean());
            verify(mWebAppLaunchHandlerJniMock, times(0))
                    .onLaunchVerified(any(), anyLong(), anyBoolean());
        }
    }

    private CustomTabIntentDataProvider createIntentDataProvider() {
        CustomTabIntentDataProvider dataProvider = mock(CustomTabIntentDataProvider.class);
        when(dataProvider.getSession()).thenReturn(env.session);
        when(dataProvider.getUrlToLoad()).thenReturn(OTHER_URL);
        when(dataProvider.getClientPackageName()).thenReturn("test.package.name");
        when(dataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        when(dataProvider.getIntent()).thenReturn(env.mIntent);
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
    public void trustedWebActivityInitialIntent_tabClearedBeforeAsyncCallback() {
        CustomTabIntentHandlingStrategy defaultStrategy =
                new DefaultCustomTabIntentHandlingStrategy(
                        env.tabProvider,
                        mNavigationController,
                        env.customTabObserver,
                        env.verifier,
                        env.currentPageVerifier,
                        env.activity);
        CustomTabIntentHandlingStrategy twaStrategy =
                new TwaIntentHandlingStrategy(
                        defaultStrategy,
                        new TwaSharingController(
                                env.tabProvider, mNavigationController, env.verifier));

        new CustomTabIntentHandler(
                env.tabProvider,
                env.intentDataProvider,
                twaStrategy,
                env.activity,
                env.mMinimizationManagerHolder);
        env.tabProvider.swapTab(null);

        shadowOf(Looper.getMainLooper()).idle();

        verify(mNavigationController, times(0)).navigate(any(), any());
        verify(mWebAppLaunchHandlerJniMock, times(0))
                .prepareForLaunch(
                        any(), anyLong(), any(), any(), any(), any(), any(), anyBoolean());
        verify(mWebAppLaunchHandlerJniMock, times(0))
                .onLaunchVerified(any(), anyLong(), anyBoolean());
    }
}

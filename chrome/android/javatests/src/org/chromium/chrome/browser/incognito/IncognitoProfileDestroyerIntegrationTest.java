// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.util.concurrent.ExecutionException;

/** Integration tests for {@link IncognitoProfileDestroyer}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoProfileDestroyerIntegrationTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private TabModel mIncognitoTabModel;
    private IncognitoNewTabPageStation mIncognitoNtp;
    private WebPageStation mFirstPage;

    @Mock ProfileManager.Observer mMockProfileManagerObserver;

    @Before
    public void setUp() throws InterruptedException {
        mFirstPage = mCtaTestRule.startOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature({"OffTheRecord"})
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void test_switchToRegularModeWithoutAnyTab_profileDestroyed() throws ExecutionException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ProfileManager.addObserver(mMockProfileManagerObserver);
                    mIncognitoTabModel =
                            mCtaTestRule.getActivity().getTabModelSelector().getModel(true);
                });
        // Switch to incognito mode while there is no incognito tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mCtaTestRule.getActivity().getTabModelSelector().selectModel(true));

        // Verify the profile is created when switched to incognito and the TabModel now has an
        // incognito Profile
        assertIncognitoProfileStillAlive();

        // Switch back to regular mode.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mCtaTestRule.getActivity().getTabModelSelector().selectModel(false));

        // Verify the incognito Profile was destroyed
        assertIncognitoProfileDestroyed();
    }

    @Test
    @MediumTest
    @Feature({"OffTheRecord"})
    public void test_closeOnlyTab_profileDestroyed() throws ExecutionException {
        // Open a single incognito tab
        setupIncognitoTab();
        Tab onlyTab = ThreadUtils.runOnUiThreadBlocking(mIncognitoNtp::getTab);

        // Verify the tab is opened and the TabModel now has an incognito Profile
        assertEquals(1, getTabCountOnUiThread(mIncognitoTabModel));
        assertIncognitoProfileStillAlive();

        // Close the incognito tab
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mIncognitoTabModel
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(onlyTab).allowUndo(false).build(),
                                        /* allowDialog= */ false));

        // Verify the incognito Profile was destroyed.
        assertIncognitoProfileDestroyed();
    }

    @Test
    @MediumTest
    @Feature({"OffTheRecord"})
    public void test_closeOneOfTwoTabs_profileNotDestroyed() throws ExecutionException {
        // Open two incognito tabs
        setupIncognitoTab();
        Tab firstTab = ThreadUtils.runOnUiThreadBlocking(mIncognitoNtp::getTab);
        mIncognitoNtp.openNewIncognitoTabFast();

        // Verify the tabs are opened and the TabModel now has an incognito Profile
        assertEquals(2, getTabCountOnUiThread(mIncognitoTabModel));
        assertIncognitoProfileStillAlive();

        // Close one incognito tab
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mIncognitoTabModel
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(firstTab)
                                                .allowUndo(false)
                                                .build(),
                                        /* allowDialog= */ false));

        // Verify the incognito Profile was not destroyed
        assertIncognitoProfileStillAlive();
    }

    @Test
    @MediumTest
    @Feature({"OffTheRecord"})
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void test_switchToRegularModeWithOneTab_profileNotDestroyed() throws ExecutionException {
        // Open a single incognito tab.
        setupIncognitoTab();
        Tab firstTab = ThreadUtils.runOnUiThreadBlocking(mIncognitoNtp::getTab);

        // Verify the tab is opened and the TabModel now has an incognito Profile.
        assertEquals(1, getTabCountOnUiThread(mIncognitoTabModel));
        assertIncognitoProfileStillAlive();

        // Switch to regular mode.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mCtaTestRule.getActivity().getTabModelSelector().selectModel(false));

        // Verify the incognito Profile was not destroyed.
        assertIncognitoProfileStillAlive();
    }

    @Test
    @MediumTest
    @Feature({"OffTheRecord"})
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void test_closeTabsWhenInactive_profileDestroyed() throws ExecutionException {
        // Open a single incognito tab.
        setupIncognitoTab();
        Tab firstTab = ThreadUtils.runOnUiThreadBlocking(mIncognitoNtp::getTab);

        // Verify the tab is opened and the TabModel now has an incognito Profile.
        assertEquals(1, getTabCountOnUiThread(mIncognitoTabModel));
        assertIncognitoProfileStillAlive();

        // Switch to regular mode.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mCtaTestRule.getActivity().getTabModelSelector().selectModel(false));

        // Close the incognito tab.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mIncognitoTabModel
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(firstTab)
                                                .allowUndo(false)
                                                .build(),
                                        /* allowDialog= */ false));

        // Verify the incognito Profile was destroyed.
        assertIncognitoProfileDestroyed();
    }

    @Test
    @MediumTest
    @Feature({"OffTheRecord"})
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void test_ActivateAfterEmpty() throws ExecutionException {
        // Open a single incognito tab.
        setupIncognitoTab();
        Tab firstTab = ThreadUtils.runOnUiThreadBlocking(mIncognitoNtp::getTab);
        assertEquals(1, getTabCountOnUiThread(mIncognitoTabModel));
        assertIncognitoProfileStillAlive();

        // Close the incognito tab. Then set the incognito tab model back to being active back and
        // forth. This should not crash. This can happen due to some UI latency.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile incognitoProfile = mIncognitoTabModel.getProfile();
                    mIncognitoTabModel
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(firstTab).allowUndo(false).build(),
                                    /* allowDialog= */ false);
                    assertNotNull(incognitoProfile);
                    assertTrue(incognitoProfile.shutdownStarted());

                    var tabModelSelector = mCtaTestRule.getActivity().getTabModelSelector();
                    tabModelSelector.selectModel(true);
                    tabModelSelector.selectModel(false);
                });

        // Verify the incognito Profile was destroyed.
        assertIncognitoProfileDestroyed();
    }

    private void assertIncognitoProfileStillAlive() throws ExecutionException {
        Profile incognitoProfile =
                ThreadUtils.runOnUiThreadBlocking(() -> mIncognitoTabModel.getProfile());
        assertNotNull(incognitoProfile);
        verify(mMockProfileManagerObserver, never()).onProfileDestroyed(any());
    }

    private void assertIncognitoProfileDestroyed() throws ExecutionException {
        verify(mMockProfileManagerObserver, atLeastOnce()).onProfileDestroyed(any());
        Profile incognitoProfile =
                ThreadUtils.runOnUiThreadBlocking(() -> mIncognitoTabModel.getProfile());
        assertNull(incognitoProfile);
    }

    private void setupIncognitoTab() {
        mIncognitoNtp = mFirstPage.openNewIncognitoTabOrWindowFast();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ProfileManager.addObserver(mMockProfileManagerObserver);
                    mIncognitoTabModel = mIncognitoNtp.getTabModel();
                });
    }
}

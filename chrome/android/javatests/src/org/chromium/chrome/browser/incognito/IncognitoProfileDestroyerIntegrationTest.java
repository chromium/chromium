// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

import java.util.concurrent.ExecutionException;

/** Integration tests for {@link IncognitoProfileDestroyer}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoProfileDestroyerIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private TabModel mIncognitoTabModel;

    @Mock ProfileManager.Observer mMockProfileManagerObserver;

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ProfileManager.addObserver(mMockProfileManagerObserver);
                    mIncognitoTabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(true);
                });
    }

    @Test
    @MediumTest
    @Feature({"OffTheRecord"})
    public void test_switchToRegularModeWithoutAnyTab_profileDestroyed() throws ExecutionException {
        // Switch to incognito mode while there is no incognito tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getTabModelSelector().selectModel(true));

        // Verify the profile is created when switched to incognito and the TabModel now has an
        // incognito Profile
        assertIncognitoProfileStillAlive();

        // Switch back to regular mode.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getTabModelSelector().selectModel(false));

        // Verify the incognito Profile was destroyed
        assertIncognitoProfileDestroyed();
    }

    @Test
    @MediumTest
    @Feature({"OffTheRecord"})
    public void test_closeOnlyTab_profileDestroyed() throws ExecutionException {
        // Open a single incognito tab
        Tab onlyTab = mActivityTestRule.newIncognitoTabFromMenu();

        // Verify the tab is opened and the TabModel now has an incognito Profile
        assertEquals(1, mIncognitoTabModel.getCount());
        assertIncognitoProfileStillAlive();

        // Close the incognito tab
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mIncognitoTabModel.closeTabs(
                                TabClosureParams.closeTab(onlyTab).allowUndo(false).build()));

        // Verify the incognito Profile was destroyed.
        assertIncognitoProfileDestroyed();
    }

    @Test
    @MediumTest
    @Feature({"OffTheRecord"})
    public void test_closeOneOfTwoTabs_profileNotDestroyed() throws ExecutionException {
        // Open two incognito tabs
        Tab firstTab = mActivityTestRule.newIncognitoTabFromMenu();
        mActivityTestRule.newIncognitoTabFromMenu();

        // Verify the tabs are opened and the TabModel now has an incognito Profile
        assertEquals(2, mIncognitoTabModel.getCount());
        assertIncognitoProfileStillAlive();

        // Close one incognito tab
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mIncognitoTabModel.closeTabs(
                                TabClosureParams.closeTab(firstTab).allowUndo(false).build()));

        // Verify the incognito Profile was not destroyed
        assertIncognitoProfileStillAlive();
    }

    @Test
    @MediumTest
    @Feature({"OffTheRecord"})
    public void test_switchToRegularModeWithOneTab_profileNotDestroyed() throws ExecutionException {
        // Open a single incognito tab.
        mActivityTestRule.newIncognitoTabFromMenu();

        // Verify the tab is opened and the TabModel now has an incognito Profile.
        assertEquals(1, mIncognitoTabModel.getCount());
        assertIncognitoProfileStillAlive();

        // Switch to regular mode.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getTabModelSelector().selectModel(false));

        // Verify the incognito Profile was not destroyed.
        assertIncognitoProfileStillAlive();
    }

    @Test
    @MediumTest
    @Feature({"OffTheRecord"})
    public void test_closeTabsWhenInactive_profileDestroyed() throws ExecutionException {
        // Open a single incognito tab.
        Tab firstTab = mActivityTestRule.newIncognitoTabFromMenu();

        // Verify the tab is opened and the TabModel now has an incognito Profile.
        assertEquals(1, mIncognitoTabModel.getCount());
        assertIncognitoProfileStillAlive();

        // Switch to regular mode.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getTabModelSelector().selectModel(false));

        // Close the incognito tab.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mIncognitoTabModel.closeTabs(
                                TabClosureParams.closeTab(firstTab).allowUndo(false).build()));

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
        verify(mMockProfileManagerObserver).onProfileDestroyed(any());
        Profile incognitoProfile =
                ThreadUtils.runOnUiThreadBlocking(() -> mIncognitoTabModel.getProfile());
        assertNull(incognitoProfile);
    }
}

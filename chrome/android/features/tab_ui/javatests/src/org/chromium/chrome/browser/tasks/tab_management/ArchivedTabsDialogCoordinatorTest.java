// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.SysUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator.Observer;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

/** End-to-end test for ArchivedTabsDialogCoordinator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ArchivedTabsDialogCoordinatorTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private ArchivedTabsDialogCoordinator mCoordinator;
    private Profile mProfile;
    private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private TabModel mArchivedTabModel;
    private ViewGroup mParentView;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mParentView = (ViewGroup) mActivityTestRule.getActivity().findViewById(R.id.coordinator);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            mActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                });

        mArchivedTabModelOrchestrator = ArchivedTabModelOrchestrator.getForProfile(mProfile);
        mArchivedTabModel = mArchivedTabModelOrchestrator.getTabModelSelector().getModel(false);
        mCoordinator =
                new ArchivedTabsDialogCoordinator(
                        mActivityTestRule.getActivity(),
                        mArchivedTabModelOrchestrator,
                        mActivityTestRule.getActivity().getBrowserControlsManager(),
                        mActivityTestRule.getActivity().getTabContentManager(),
                        getMode(),
                        mParentView,
                        mActivityTestRule.getActivity().getSnackbarManager());
        waitForArchivedTabModelsToLoad(mArchivedTabModelOrchestrator);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mArchivedTabModel.closeAllTabs());
    }

    @Test
    @MediumTest
    public void testNoArchivedTabs() throws Exception {
        showDialog();
        onView(withText("0 inactive tabs")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testOneInactiveTab() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        showDialog();
        onView(withText("1 inactive tab")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testTwoInactiveTabs() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        showDialog();
        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));
    }

    private void showDialog() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mCoordinator.show());
    }

    private void hideDialog() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mCoordinator.hide());
    }

    private @TabListCoordinator.TabListMode int getMode() {
        return SysUtils.isLowEndDevice()
                ? TabListCoordinator.TabListMode.LIST
                : TabListCoordinator.TabListMode.GRID;
    }

    private void addArchivedTab(GURL url, String title) {
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> {
                    mArchivedTabModelOrchestrator
                            .getArchivedTabCreatorForTesting()
                            .createNewTab(
                                    new LoadUrlParams(new GURL("https://google.com")),
                                    "google",
                                    TabLaunchType.FROM_RESTORE,
                                    null,
                                    mArchivedTabModel.getCount());
                    return null;
                });
    }

    private void waitForArchivedTabModelsToLoad(
            ArchivedTabModelOrchestrator archivedTabModelOrchestrator) {
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> {
                    CallbackHelper callbackHelper = new CallbackHelper();
                    if (archivedTabModelOrchestrator.isTabModelInitialized()) {
                        callbackHelper.notifyCalled();
                    } else {
                        archivedTabModelOrchestrator.addObserver(
                                new Observer() {
                                    @Override
                                    public void onTabModelCreated(TabModel archivedTabModel) {
                                        archivedTabModelOrchestrator.removeObserver(this);
                                        callbackHelper.notifyCalled();
                                    }
                                });
                    }

                    return null;
                });
    }
}

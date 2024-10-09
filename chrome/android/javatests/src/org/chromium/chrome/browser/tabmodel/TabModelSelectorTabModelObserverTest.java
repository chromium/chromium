// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserverTestRule.TabModelSelectorTestTabModel;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the TabModelSelectorTabModelObserver. See
 * TabModelSelectorTabModelObserverUnitTest.java for unit tests.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TabModelSelectorTabModelObserverTest {
    @ClassRule
    public static final TabModelSelectorObserverTestRule sTestRule =
            new TabModelSelectorObserverTestRule();

    private TabModelSelectorBase mSelector;

    @Before
    public void setUp() {
        mSelector = sTestRule.getSelector();
    }

    @Test
    @SmallTest
    public void testAlreadyInitializedSelector() throws TimeoutException {
        final CallbackHelper registrationCompleteCallback = new CallbackHelper();
        TabModelSelectorTabModelObserver observer =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new TabModelSelectorTabModelObserver(mSelector) {
                                    @Override
                                    protected void onRegistrationComplete() {
                                        registrationCompleteCallback.notifyCalled();
                                    }
                                });
        registrationCompleteCallback.waitForCallback(0);
        assertAllModelsHaveObserver(mSelector, observer);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testUninitializedSelector() throws TimeoutException {
        mSelector =
                new TabModelSelectorBase(null, false) {
                    @Override
                    public void requestToShowTab(Tab tab, int type) {}

                    @Override
                    public boolean isSessionRestoreInProgress() {
                        return false;
                    }

                    @Override
                    public Tab openNewTab(
                            LoadUrlParams loadUrlParams,
                            @TabLaunchType int type,
                            Tab parent,
                            boolean incognito) {
                        return null;
                    }
                };
        final CallbackHelper registrationCompleteCallback = new CallbackHelper();
        TabModelSelectorTabModelObserver observer =
                new TabModelSelectorTabModelObserver(mSelector) {
                    @Override
                    protected void onRegistrationComplete() {
                        registrationCompleteCallback.notifyCalled();
                    }
                };
        mSelector.initialize(sTestRule.getNormalTabModel(), sTestRule.getIncognitoTabModel());
        registrationCompleteCallback.waitForCallback(0);
        assertAllModelsHaveObserver(mSelector, observer);
    }

    private static void assertAllModelsHaveObserver(
            TabModelSelector selector, TabModelObserver observer) {
        List<TabModel> models = selector.getModels();
        for (int i = 0; i < models.size(); i++) {
            Assert.assertTrue(models.get(i) instanceof TabModelSelectorTestTabModel);
            Assert.assertTrue(
                    ((TabModelSelectorTestTabModel) models.get(i))
                            .getObservers()
                            .contains(observer));
        }
    }
}

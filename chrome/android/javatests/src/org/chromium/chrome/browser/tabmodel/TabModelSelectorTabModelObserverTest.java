// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserverTestRule.TabModelSelectorTestTabModel;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the TabModelSelectorTabModelObserver.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class TabModelSelectorTabModelObserverTest {
    // Do not add @Rule to this, it's already added to RuleChain
    private final TabModelSelectorObserverTestRule mTestRule =
            new TabModelSelectorObserverTestRule();

    @Rule
    public final RuleChain mChain = RuleChain.outerRule(mTestRule).around(new UiThreadTestRule());

    private TabModelSelectorBase mSelector;

    @Before
    public void setUp() {
        mSelector = mTestRule.getSelector();
    }

    @Test
    @SmallTest
    public void testAlreadyInitializedSelector() throws TimeoutException {
        final CallbackHelper registrationCompleteCallback = new CallbackHelper();
        TabModelSelectorTabModelObserver observer =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> new TabModelSelectorTabModelObserver(mSelector) {
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
        mSelector = new TabModelSelectorBase(null, false) {
            @Override
            public Tab openNewTab(LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent,
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
        mSelector.initialize(mTestRule.getNormalTabModel(), mTestRule.getIncognitoTabModel());
        registrationCompleteCallback.waitForCallback(0);
        assertAllModelsHaveObserver(mSelector, observer);
    }

    private static void assertAllModelsHaveObserver(
            TabModelSelector selector, TabModelObserver observer) {
        List<TabModel> models = selector.getModels();
        for (int i = 0; i < models.size(); i++) {
            Assert.assertTrue(models.get(i) instanceof TabModelSelectorTestTabModel);
            Assert.assertTrue(((TabModelSelectorTestTabModel) models.get(i))
                                      .getObservers()
                                      .contains(observer));
        }
    }
}

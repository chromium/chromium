// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link HubProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubProviderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Pane mMockPane;
    @Mock private BackPressManager mBackPressManagerMock;
    @Mock private ObservableSupplier<Tab> mTabSupplierMock;

    private Activity mActivity;

    @Before
    public void setUp() {
        when(mTabModelSelector.getCurrentTabSupplier()).thenReturn(mTabSupplierMock);
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
    }

    @Test
    @SmallTest
    public void testHubProvider() {
        HubProvider provider =
                new HubProvider(
                        mActivity,
                        new DefaultPaneOrderController(),
                        mBackPressManagerMock,
                        () -> mTabModelSelector);

        PaneListBuilder builder = provider.getPaneListBuilder();

        var hubManagerSupplier = provider.getHubManagerSupplier();
        assertNotNull(hubManagerSupplier);
        assertFalse(hubManagerSupplier.hasValue());

        builder.registerPane(PaneId.TAB_SWITCHER, LazyOneshotSupplier.fromValue(mMockPane));
        assertFalse(builder.isBuilt());

        HubManager hubManager = hubManagerSupplier.get();
        assertNotNull(hubManager);
        assertTrue(hubManagerSupplier.hasValue());
        assertTrue(builder.isBuilt());

        PaneManager paneManager = hubManager.getPaneManager();
        assertNotNull(paneManager);
        paneManager.focusPane(PaneId.TAB_SWITCHER);
        assertEquals(mMockPane, paneManager.getFocusedPaneSupplier().get());
    }
}

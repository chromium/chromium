// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayerJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** Tests for {@link StripLayoutHelperManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN})
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
public class StripLayoutHelperManagerTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private TabStripSceneLayer.Natives mTabStripSceneMock;
    @Mock
    private LayoutUpdateHost mUpdateHost;
    @Mock
    private LayoutRenderHost mRenderHost;
    @Mock
    private Supplier<LayerTitleCache> mLayerTitleCacheSupplier;
    @Mock
    private ActivityLifecycleDispatcher mLifecycleDispatcher;

    private StripLayoutHelperManager mStripLayoutHelperManager;
    private Context mContext;

    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(TabStripSceneLayerJni.TEST_HOOKS, mTabStripSceneMock);
        mContext = new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        TabStripSceneLayer.setTestFlag(true);
        initializeTest();
    }

    @After
    public void tearDown() {
        TabStripSceneLayer.setTestFlag(false);
        TabUiFeatureUtilities.setTabStripRedesignEnableDetachedForTesting(false);
        TabUiFeatureUtilities.setTabStripRedesignEnableFolioForTesting(false);
    }

    private void initializeTest() {
        mStripLayoutHelperManager = new StripLayoutHelperManager(
                mContext, mUpdateHost, mRenderHost, mLayerTitleCacheSupplier, mLifecycleDispatcher);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testGetBackgroundColorDetached() {
        TabUiFeatureUtilities.setTabStripRedesignEnableDetachedForTesting(true);
        assertEquals(ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_0),
                mStripLayoutHelperManager.getBackgroundColor());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testGetBackgroundColorFolio() {
        TabUiFeatureUtilities.setTabStripRedesignEnableFolioForTesting(true);
        mStripLayoutHelperManager.onContextChanged(mContext);
        assertEquals(ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_2),
                mStripLayoutHelperManager.getBackgroundColor());
    }
}

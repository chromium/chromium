// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.magic_stack.CirclePagerIndicatorDecoration.getItemPerScreen;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.util.DisplayMetrics;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.HashSet;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {HomeModulesCoordinatorUnitTest.ShadowSemanticColorUtils.class})
public class HomeModulesCoordinatorUnitTest {
    @Implements(SemanticColorUtils.class)
    static class ShadowSemanticColorUtils {
        @Implementation
        public static int getDefaultIconColorSecondary(Context context) {
            return Color.LTGRAY;
        }
    }

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private Resources mResources;
    @Mock private ModuleDelegateHost mModuleDelegateHost;
    @Mock private ViewGroup mView;
    @Mock private RecyclerView mRecyclerView;
    @Mock private UiConfig mUiConfig;
    @Mock private Configuration mConfiguration;
    @Mock private ApplicationInfo mApplicationInfo;
    @Mock private DisplayMetrics mDisplayMetrics;
    @Mock private HomeModulesConfigManager mHomeModulesConfigManager;

    @Captor private ArgumentCaptor<DisplayStyleObserver> mDisplayStyleObserver;
    private HomeModulesCoordinator mCoordinator;

    @Captor
    private ArgumentCaptor<HomeModulesConfigManager.HomeModulesStateListener>
            mHomeModulesStateListener;

    @Before
    public void setUp() {
        when(mModuleDelegateHost.getUiConfig()).thenReturn(mUiConfig);
        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getConfiguration()).thenReturn(mConfiguration);
        when(mResources.getDisplayMetrics()).thenReturn(mDisplayMetrics);
        when(mActivity.getApplicationInfo()).thenReturn(mApplicationInfo);
        when(mView.findViewById(R.id.home_modules_recycler_view)).thenReturn(mRecyclerView);
        when(mRecyclerView.getContext()).thenReturn(mActivity);
        when(mHomeModulesConfigManager.getEnabledModuleList())
                .thenReturn(new HashSet<>(List.of(ModuleType.PRICE_CHANGE, ModuleType.SINGLE_TAB)));
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
    }

    @Test
    @SmallTest
    public void testCreate_phones() {
        assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        mCoordinator =
                new HomeModulesCoordinator(
                        mActivity, mModuleDelegateHost, mView, mHomeModulesConfigManager);

        // Verifies that there isn't an observer of UiConfig registered.
        assertTrue(mCoordinator.getIsSnapHelperAttachedForTesting());
        verify(mUiConfig, never()).addObserver(mDisplayStyleObserver.capture());
    }

    @Test
    @SmallTest
    public void testCreate_tablets() {
        setupAndVerifyTablets();
        assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));

        DisplayStyle displayStyle =
                new DisplayStyle(HorizontalDisplayStyle.WIDE, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStyle);

        mCoordinator =
                new HomeModulesCoordinator(
                        mActivity, mModuleDelegateHost, mView, mHomeModulesConfigManager);
        // Verifies that an observer is registered to the mUiConfig on tablets.
        verify(mUiConfig).addObserver(mDisplayStyleObserver.capture());

        // Verifies that the snap scroll helper isn't attached to the recyclerview if there are more
        // than one item shown per screen.
        assertEquals(2, getItemPerScreen(displayStyle));
        assertFalse(mCoordinator.getIsSnapHelperAttachedForTesting());

        // Verifies that the snap scroll helper is attached to the recyclerview if there is only one
        // item shown per screen.
        DisplayStyle newDisplayStyle =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        mDisplayStyleObserver.getValue().onDisplayStyleChanged(newDisplayStyle);

        assertEquals(1, getItemPerScreen(newDisplayStyle));
        assertTrue(mCoordinator.getIsSnapHelperAttachedForTesting());
    }

    @Test
    @SmallTest
    public void testDestroy() {
        setupAndVerifyTablets();
        assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));

        DisplayStyle displayStyle =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStyle);

        mCoordinator =
                new HomeModulesCoordinator(
                        mActivity, mModuleDelegateHost, mView, mHomeModulesConfigManager);
        // Verifies that an observer is registered to the mUiConfig on tablets.
        verify(mUiConfig).addObserver(mDisplayStyleObserver.capture());

        mCoordinator.destroy();
        verify(mUiConfig).removeObserver(mDisplayStyleObserver.capture());
    }

    @Test
    @SmallTest
    public void testGetModuleList() {
        when(mHomeModulesConfigManager.getEnabledModuleList())
                .thenReturn(new HashSet<>(List.of(ModuleType.SINGLE_TAB)));
        assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        mCoordinator =
                new HomeModulesCoordinator(
                        mActivity, mModuleDelegateHost, mView, mHomeModulesConfigManager);
        List<Integer> expectedModuleList = List.of(ModuleType.SINGLE_TAB);
        assertEquals(mCoordinator.getModuleList(), expectedModuleList);
    }

    @Test
    @SmallTest
    public void testOnModuleConfigChanged() {
        assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        mCoordinator =
                new HomeModulesCoordinator(
                        mActivity, mModuleDelegateHost, mView, mHomeModulesConfigManager);

        verify(mHomeModulesConfigManager).addListener(mHomeModulesStateListener.capture());
        List<Integer> expectedModuleListBeforeHidingModule =
                List.of(ModuleType.PRICE_CHANGE, ModuleType.SINGLE_TAB);
        assertEquals(mCoordinator.getModuleList(), expectedModuleListBeforeHidingModule);

        mHomeModulesStateListener.getValue().onModuleConfigChanged(ModuleType.PRICE_CHANGE, false);
        List<Integer> expectedModuleListAfterHidingModule = List.of(ModuleType.SINGLE_TAB);
        assertEquals(mCoordinator.getModuleList(), expectedModuleListAfterHidingModule);

        mHomeModulesStateListener.getValue().onModuleConfigChanged(ModuleType.PRICE_CHANGE, true);
        assertEquals(mCoordinator.getModuleList(), expectedModuleListBeforeHidingModule);

        mCoordinator.destroy();
        verify(mHomeModulesConfigManager).removeListener(mHomeModulesStateListener.capture());
    }

    @Test
    @SmallTest
    public void testRemoveModuleAndDisable() {
        assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        mCoordinator =
                new HomeModulesCoordinator(
                        mActivity, mModuleDelegateHost, mView, mHomeModulesConfigManager);

        mCoordinator.removeModuleAndDisable(ModuleType.PRICE_CHANGE);
        verify(mHomeModulesConfigManager)
                .setPrefModuleTypeEnabled(eq(ModuleType.PRICE_CHANGE), eq(false));
    }

    private void setupAndVerifyTablets() {
        when(mResources.getInteger(org.chromium.ui.R.integer.min_screen_width_bucket))
                .thenReturn(DeviceFormFactor.SCREEN_BUCKET_TABLET);
        assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
    }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.view.ViewGroup;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.hub.HubLayoutDependencyHolder;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link LayoutManagerChrome}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LayoutManagerChromeUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private @Mock LayoutManagerHost mHost;
    private @Mock ViewGroup mContentContainer;
    private @Mock HubLayoutDependencyHolder mHubLayoutDependencyHolder;

    private final SettableNullableObservableSupplier<TabSwitcher> mTabSwitcherSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<TabModelSelector> mTabModelSelectorSupplier =
            ObservableSuppliers.createNullable();
    private final SettableMonotonicObservableSupplier<TabContentManager>
            mTabContentManagerSupplier = ObservableSuppliers.createMonotonic();
    private final SettableNullableObservableSupplier<TopUiThemeColorProvider>
            mTopUiThemeColorProvider = ObservableSuppliers.createNullable();

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (TestActivity activity) -> when(mHost.getContext()).thenReturn(activity));
    }

    @Test
    public void testShowAfterFoo() {
        LayoutManagerChrome layoutManagerChrome =
                new LayoutManagerChrome(
                        mHost,
                        mContentContainer,
                        mTabSwitcherSupplier,
                        mTabModelSelectorSupplier,
                        mTabContentManagerSupplier,
                        mTopUiThemeColorProvider,
                        mHubLayoutDependencyHolder);
        layoutManagerChrome.destroy();
        layoutManagerChrome.showLayout(LayoutType.TAB_SWITCHER, /* animate= */ true);
        verifyNoInteractions(mHubLayoutDependencyHolder);
    }
}

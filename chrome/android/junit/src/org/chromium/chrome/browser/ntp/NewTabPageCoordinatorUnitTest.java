// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.magic_stack.HomeModulesRecyclerView;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tasks.HomeSurfaceTracker;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link NewTabPageCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NewTabPageCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NewTabPageManager mManager;
    @Mock private NewTabPageLayout mNewTabPageLayout;
    @Mock private Tab mTab;
    @Mock private Tab mMostRecentTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private HomeSurfaceTracker mHomeSurfaceTracker;
    @Mock private ModuleRegistry mModuleRegistry;
    @Mock private Profile mProfile;
    @Mock private View mSearchBoxView;
    @Mock private ViewStub mHomeModulesStub;
    @Mock private ViewGroup mHomeModulesContainer;
    @Mock private HomeModulesRecyclerView mHomeModulesRecyclerView;
    @Mock private TabModel mTabModel;

    private Activity mActivity;
    private NewTabPageCoordinator mCoordinator;
    private final OneshotSupplierImpl<ModuleRegistry> mModuleRegistrySupplier =
            new OneshotSupplierImpl<>();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        when(mMostRecentTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab.getProfile()).thenReturn(mProfile);
        mModuleRegistrySupplier.set(mModuleRegistry);

        when(mNewTabPageLayout.findViewById(R.id.search_box)).thenReturn(mSearchBoxView);
        when(mNewTabPageLayout.findViewById(R.id.home_modules_recycler_view_stub))
                .thenReturn(mHomeModulesStub);
        when(mHomeModulesStub.inflate()).thenReturn(mHomeModulesContainer);
        when(mNewTabPageLayout.findViewById(R.id.home_modules_recycler_view))
                .thenReturn(mHomeModulesRecyclerView);
        when(mHomeModulesRecyclerView.getContext()).thenReturn(mActivity);

        mCoordinator =
                new NewTabPageCoordinator(
                        mManager,
                        mActivity,
                        mNewTabPageLayout,
                        mTab,
                        mTabModelSelector,
                        mModuleRegistrySupplier,
                        mHomeSurfaceTracker);
    }

    @Test
    public void testShowHomeSurfaceUiOnNtp() {
        assertFalse(mCoordinator.isHomeSurface());
        assertNull(mCoordinator.getHomeModulesCoordinatorForTesting());

        mCoordinator.showHomeSurfaceUiOnNtp(mMostRecentTab);

        verifyIsHomeSurface(/* isHomeSurface= */ true);
        verify(mHomeModulesContainer, never()).setVisibility(anyInt());
    }

    @Test
    public void testShowHomeSurfaceUiOnNtp_noMostVisitedTab() {
        assertFalse(mCoordinator.isHomeSurface());
        assertNull(mCoordinator.getHomeModulesCoordinatorForTesting());

        mCoordinator.showHomeSurfaceUiOnNtp(null);

        verifyIsHomeSurface(/* isHomeSurface= */ false);
    }

    @Test
    public void testOnHomeModulesShown() {
        mCoordinator.showHomeSurfaceUiOnNtp(mMostRecentTab);

        boolean isVisible = true;
        mCoordinator.onHomeModulesShown(isVisible);
        verify(mHomeModulesContainer).setVisibility(eq(View.VISIBLE));

        isVisible = false;
        mCoordinator.onHomeModulesShown(isVisible);
        verify(mHomeModulesContainer).setVisibility(eq(View.GONE));
    }

    @Test
    public void testOnTabSelected() {
        int tabId = 123;
        TabRemover tabRemover = mock(TabRemover.class);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getTabRemover()).thenReturn(tabRemover);

        mCoordinator.onTabSelected(tabId);

        verify(tabRemover).closeTabs(any(), /* allowDialog= */ eq(false));
        verify(mHomeSurfaceTracker).updateHomeSurfaceAndTrackingTabs(eq(null), eq(null));
    }

    @Test
    public void testInitializeHomeModules_TrackingTabReady() {
        when(mHomeSurfaceTracker.isHomeSurfaceTab(mTab)).thenReturn(true);
        when(mHomeSurfaceTracker.getLastActiveTabToTrack()).thenReturn(mMostRecentTab);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("NewTabPage.AsHomeSurface", true)
                        .build();
        mCoordinator.initializeHomeModules();

        verifyIsHomeSurface(/* isHomeSurface= */ true);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testInitializeHomeModules_NormalNtp() {
        when(mHomeSurfaceTracker.isHomeSurfaceTab(mTab)).thenReturn(false);
        when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_CHROME_UI);

        mCoordinator.initializeHomeModules();

        verifyIsHomeSurface(/* isHomeSurface= */ false);
    }

    @Test
    public void testInitializeHomeModules_StartupNtp() {
        when(mHomeSurfaceTracker.isHomeSurfaceTab(mTab)).thenReturn(false);
        when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_STARTUP);

        mCoordinator.initializeHomeModules();

        // Verifies that the HomeModulesCoordinator isn't created if the Ntp's tracking Tab isn't
        // ready.
        assertNull(mCoordinator.getHomeModulesCoordinatorForTesting());

        mCoordinator.showHomeSurfaceUiOnNtp(mMostRecentTab);
        verifyIsHomeSurface(/* isHomeSurface= */ true);
    }

    private void verifyIsHomeSurface(boolean isHomeSurface) {
        assertEquals(isHomeSurface, mCoordinator.isHomeSurface());
        assertNotNull(mCoordinator.getHomeModulesCoordinatorForTesting());
    }
}

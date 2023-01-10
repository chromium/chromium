// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.text.TextUtils;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/** Unit tests for ToolbarTabControllerImpl. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ToolbarTabControllerImplTest {
    private class LoadUrlParamsMatcher implements ArgumentMatcher<LoadUrlParams> {
        LoadUrlParams mLoadUrlParams;
        public LoadUrlParamsMatcher(LoadUrlParams loadUrlParams) {
            mLoadUrlParams = loadUrlParams;
        }

        @Override
        public boolean matches(LoadUrlParams argument) {
            return argument.getUrl().equals(mLoadUrlParams.getUrl())
                    && argument.getTransitionType() == mLoadUrlParams.getTransitionType();
        }
    }
    @Rule
    public JniMocker mocker = new JniMocker();
    @Mock
    private Supplier<Tab> mTabSupplier;
    @Mock
    private Tab mTab;
    @Mock
    private Supplier<Boolean> mOverrideHomePageSupplier;
    @Mock
    private ObservableSupplier<BottomControlsCoordinator> mBottomControlsCoordinatorSupplier;
    @Mock
    private BottomControlsCoordinator mBottomControlsCoordinator;
    @Mock
    private Tracker mTracker;
    @Mock
    private Supplier<Tracker> mTrackerSupplier;
    @Mock
    private Runnable mRunnable;
    @Mock
    private Profile mProfile;
    @Mock
    public Profile.Natives mMockProfileNatives;
    @Mock
    private NativePage mNativePage;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private ToolbarTabControllerImpl mToolbarTabController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mTab).when(mTabSupplier).get();
        doReturn(false).when(mOverrideHomePageSupplier).get();
        mocker.mock(ProfileJni.TEST_HOOKS, mMockProfileNatives);
        doReturn(mProfile).when(mMockProfileNatives).fromWebContents(any());
        doReturn(mNativePage).when(mTab).getNativePage();
        TrackerFactory.setTrackerForTests(mTracker);
        initToolbarTabController();
    }

    @Test
    public void backForward_NotTriggeredWhenTabCannot() {
        doReturn(false).when(mTab).canGoForward();
        doReturn(false).when(mTab).canGoBack();

        assertFalse(mToolbarTabController.forward());
        assertFalse(mToolbarTabController.back());
        verify(mNativePage, never()).notifyHidingWithBack();
    }

    @Test
    public void backForward_correspondingTabActionsTriggered() {
        doReturn(true).when(mTab).canGoForward();
        doReturn(true).when(mTab).canGoBack();

        assertTrue(mToolbarTabController.forward());
        assertTrue(mToolbarTabController.back());
        verify(mRunnable, times(2)).run();
        verify(mTab).goForward();
        verify(mTab).goBack();
    }

    @Test
    public void back_handledByBottomControls() {
        doReturn(mBottomControlsCoordinator).when(mBottomControlsCoordinatorSupplier).get();
        doReturn(true).when(mBottomControlsCoordinator).onBackPressed();
        Assert.assertTrue(mToolbarTabController.back());

        verify(mBottomControlsCoordinator).onBackPressed();
        verify(mRunnable, never()).run();
        verify(mTab, never()).goBack();
    }

    @Test
    public void back_notifyNativePageHiding() {
        doReturn(null).when(mBottomControlsCoordinatorSupplier).get();
        doReturn(true).when(mTab).canGoBack();

        mToolbarTabController.back();
        verify(mNativePage).notifyHidingWithBack();
    }

    @Test
    public void stopOrReloadCurrentTab() {
        doReturn(false).when(mTab).isLoading();
        mToolbarTabController.stopOrReloadCurrentTab();

        verify(mTab).reload();
        verify(mRunnable).run();

        doReturn(true).when(mTab).isLoading();
        mToolbarTabController.stopOrReloadCurrentTab();

        verify(mTab).stopLoading();
        verify(mRunnable, times(2)).run();
    }

    @Test
    public void openHomepage_handledByStartSurfaceNoProfile() {
        doReturn(true).when(mOverrideHomePageSupplier).get();

        mToolbarTabController.openHomepage();

        verify(mTab, never()).loadUrl(any());
        verify(mTracker, never()).notifyEvent(EventConstants.HOMEPAGE_BUTTON_CLICKED);
    }

    @Test
    public void openHomepage_handledByStartSurfaceWithProfile() {
        doReturn(true).when(mOverrideHomePageSupplier).get();
        doReturn(mTracker).when(mTrackerSupplier).get();

        mToolbarTabController.openHomepage();

        verify(mTab, never()).loadUrl(any());
        verify(mTracker, times(1)).notifyEvent(EventConstants.HOMEPAGE_BUTTON_CLICKED);
    }

    @Test
    public void openHomepage_loadsHomePage() {
        mToolbarTabController.openHomepage();
        String homePageUrl = HomepageManager.getHomepageUri();
        if (TextUtils.isEmpty(homePageUrl)) {
            homePageUrl = UrlConstants.NTP_URL;
        }
        verify(mTab).loadUrl(argThat(new LoadUrlParamsMatcher(
                new LoadUrlParams(homePageUrl, PageTransition.HOME_PAGE))));
    }

    private void initToolbarTabController() {
        mToolbarTabController = new ToolbarTabControllerImpl(mTabSupplier,
                mOverrideHomePageSupplier, mTrackerSupplier, mBottomControlsCoordinatorSupplier,
                ToolbarManager::homepageUrl, mRunnable);
    }
}

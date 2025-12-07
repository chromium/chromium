// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Unit tests for {@link ReaderModeBottomSheetManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReaderModeBottomSheetManagerTest {
    private static final String DISTILLED_URL = "chrome-distiller://fdsa/";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ActivityTabProvider mTabProvider;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private NavigationHandle mNavigationHandle;
    @Mock private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private DomDistillerServiceFactoryJni mDomDistillerServiceFactoryJni;
    @Mock private DistilledPagePrefs mDistilledPagePrefs;
    @Mock private DomDistillerService mDomDistillerService;
    @Mock private ThemeColorProvider mThemeColorProvider;

    @Captor private ArgumentCaptor<Callback<Tab>> mActivityTabObserverCaptor;
    @Captor private ArgumentCaptor<EmptyTabObserver> mEmptyTabObserverCaptor;

    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer> mBrowserControlsObserverCaptor;

    private ReaderModeBottomSheetManager mManager;
    private Activity mActivity;
    private GURL mGurl;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mGurl = new GURL(DISTILLED_URL);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.getUrl()).thenReturn(mGurl);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTabProvider.get()).thenReturn(mTab);
        when(mNavigationHandle.hasCommitted()).thenReturn(true);
        when(mNavigationHandle.isInPrimaryMainFrame()).thenReturn(true);

        DomDistillerServiceFactoryJni.setInstanceForTesting(mDomDistillerServiceFactoryJni);
        when(mDomDistillerService.getDistilledPagePrefs()).thenReturn(mDistilledPagePrefs);
        when(mDomDistillerServiceFactoryJni.getForProfile(any())).thenReturn(mDomDistillerService);

        DomDistillerUrlUtilsJni.setInstanceForTesting(mDomDistillerUrlUtilsJni);
        when(mDomDistillerUrlUtilsJni.isDistilledPage(DISTILLED_URL)).thenReturn(true);
    }

    @After
    public void tearDown() {
        if (mManager != null) {
            mManager.destroy();
            mManager = null;
        }
    }

    private void createManagerAndGetTabObserver() {
        mManager =
                new ReaderModeBottomSheetManager(
                        mActivity,
                        mBottomSheetController,
                        mTabProvider,
                        mBrowserControlsVisibilityManager,
                        mThemeColorProvider);
        verify(mTabProvider).addObserver(mActivityTabObserverCaptor.capture());
        verify(mTab).addObserver(mEmptyTabObserverCaptor.capture());
    }

    @Test
    public void testShowOnDistilledPage() {
        createManagerAndGetTabObserver();
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testHideOnNonDistilledPage() {
        updateUrl("https://www.google.com");

        createManagerAndGetTabObserver();
        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testShowOnNavigationToDistilledPage() {
        updateUrl("https://www.google.com");

        createManagerAndGetTabObserver();
        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());

        updateUrl(DISTILLED_URL);
        mEmptyTabObserverCaptor
                .getValue()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigationHandle);
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testHideOnNavigationFromDistilledPage() {
        createManagerAndGetTabObserver();
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());

        updateUrl("https://www.google.com");
        mEmptyTabObserverCaptor
                .getValue()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigationHandle);
        verify(mBottomSheetController).hideContent(any(), anyBoolean());
    }

    @Test
    public void testHideOnTabClosure() {
        createManagerAndGetTabObserver();
        // The sheet is shown on the distilled page.
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());

        // When the tab is closing, the sheet should be hidden.
        mEmptyTabObserverCaptor.getValue().onClosingStateChanged(mTab, true);
        verify(mBottomSheetController).hideContent(any(), anyBoolean());
    }

    @Test
    public void testHideOnNoActiveTab() {
        createManagerAndGetTabObserver();
        // The sheet is shown on the distilled page.
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());

        // When there's no active tab, the sheet should be hidden.
        mActivityTabObserverCaptor.getValue().onResult(null);
        verify(mBottomSheetController).hideContent(any(), anyBoolean());
    }

    @Test
    public void testBrowserControlsObserverRegistration() {
        createManagerAndGetTabObserver();
        verify(mBrowserControlsVisibilityManager)
                .addObserver(mBrowserControlsObserverCaptor.capture());
    }

    @Test
    public void testShowOnControlsCompletelyShown() {
        updateUrl(DISTILLED_URL);
        createManagerAndGetTabObserver();
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);
        verify(mBrowserControlsVisibilityManager)
                .addObserver(mBrowserControlsObserverCaptor.capture());
        when(mBrowserControlsVisibilityManager.getBrowserControlHiddenRatio()).thenReturn(0f);
        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);
        // show() is called once on initial distilled page load, and once on scroll up.
        verify(mBottomSheetController, times(2)).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testHideOnControlsHiddenEnough() {
        updateUrl(DISTILLED_URL);
        createManagerAndGetTabObserver();

        verify(mBrowserControlsVisibilityManager)
                .addObserver(mBrowserControlsObserverCaptor.capture());
        when(mBrowserControlsVisibilityManager.getBrowserControlHiddenRatio()).thenReturn(0.5f);
        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);
        // show() is called once on initial distilled page load, and once on scroll up.
        verify(mBottomSheetController, times(1)).requestShowContent(any(), anyBoolean());
        verify(mBottomSheetController, times(1)).hideContent(any(), anyBoolean());
    }

    @Test
    public void testShowOnScroll_onlyOnDistilledPages() {
        updateUrl("https://www.google.com");
        createManagerAndGetTabObserver();

        verify(mBrowserControlsVisibilityManager)
                .addObserver(mBrowserControlsObserverCaptor.capture());
        when(mBrowserControlsVisibilityManager.getBrowserControlHiddenRatio()).thenReturn(1f);
        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);
        verify(mBottomSheetController, times(0)).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testObserversRemovedOnTabChange() {
        createManagerAndGetTabObserver();
        verify(mTab).addObserver(any());

        mActivityTabObserverCaptor.getValue().onResult(null);
        verify(mTab).removeObserver(any());
    }

    @Test
    public void testObserversRemovedOnDestroy() {
        createManagerAndGetTabObserver();
        verify(mTab).addObserver(any());

        mManager.destroy();
        verify(mTabProvider).removeObserver(any());
        verify(mTab).removeObserver(any());
        verify(mBrowserControlsVisibilityManager).removeObserver(any());
        mManager = null;
    }

    private void updateUrl(String url) {
        mGurl = new GURL(url);
        when(mTab.getUrl()).thenReturn(mGurl);
    }
}

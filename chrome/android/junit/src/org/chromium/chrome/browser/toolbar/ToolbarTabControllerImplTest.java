// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.function.Supplier;

/** Unit tests for ToolbarTabControllerImpl. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ToolbarTabControllerImplTest {
    private static class LoadUrlParamsMatcher implements ArgumentMatcher<LoadUrlParams> {
        final LoadUrlParams mLoadUrlParams;

        public LoadUrlParamsMatcher(LoadUrlParams loadUrlParams) {
            mLoadUrlParams = loadUrlParams;
        }

        @Override
        public boolean matches(LoadUrlParams argument) {
            return argument.getUrl().equals(mLoadUrlParams.getUrl())
                    && argument.getTransitionType() == mLoadUrlParams.getTransitionType();
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Supplier<Tab> mTabSupplier;
    @Mock private Tab mTab;
    @Mock private Tab mTab2;
    @Mock private BottomControlsCoordinator mBottomControlsCoordinator;
    @Mock private Tracker mTracker;
    @Mock private Supplier<Tracker> mTrackerSupplier;
    @Mock private Runnable mRunnable;
    @Mock private Profile mProfile;
    @Mock private NativePage mNativePage;
    @Mock private Supplier<Tab> mActivityTabProvider;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TabCreator mTabCreator;
    @Mock private MultiInstanceOrchestrator mMultiInstanceOrchestrator;
    @Mock private Supplier<Boolean> mIsOffTheRecordSupplier;

    private final GURL mGURL = new GURL("https://example.com");
    private ToolbarTabControllerImpl mToolbarTabController;

    @Before
    public void setUp() {
        doReturn(mTab).when(mTabSupplier).get();
        doReturn(mTab).when(mActivityTabProvider).get();
        doReturn(mProfile).when(mTab).getProfile();
        doReturn(mNativePage).when(mTab).getNativePage();
        doReturn(mTabCreator).when(mTabCreatorManager).getTabCreator(anyBoolean());
        doReturn(mGURL).when(mTab).getUrl();
        doReturn(BackPressResult.FAILURE).when(mBottomControlsCoordinator).handleBackPress();
        doReturn(ObservableSuppliers.alwaysFalse())
                .when(mBottomControlsCoordinator)
                .getHandleBackPressChangedSupplier();
        doReturn(false).when(mIsOffTheRecordSupplier).get();
        TrackerFactory.setTrackerForTests(mTracker);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMultiInstanceOrchestrator);
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
        doReturn(BackPressResult.SUCCESS).when(mBottomControlsCoordinator).handleBackPress();
        assertTrue(mToolbarTabController.back());

        verify(mBottomControlsCoordinator).handleBackPress();
        verify(mRunnable, never()).run();
        verify(mTab, never()).goBack();
    }

    @Test
    public void back_notifyNativePageHiding() {
        doReturn(true).when(mTab).canGoBack();

        mToolbarTabController.back();
        verify(mNativePage).notifyHidingWithBack();
    }

    @Test
    public void stopOrReloadCurrentTab() {
        doReturn(false).when(mTab).isLoading();
        mToolbarTabController.stopOrReloadCurrentTab(/* ignoreCache= */ false);

        verify(mTab).reload();
        verify(mRunnable).run();

        doReturn(true).when(mTab).isLoading();
        mToolbarTabController.stopOrReloadCurrentTab(/* ignoreCache= */ false);

        verify(mTab).stopLoading();
        verify(mRunnable, times(2)).run();
    }

    @Test
    public void stopOrReloadCurrentTab_ignoreCache() {
        doReturn(false).when(mTab).isLoading();

        mToolbarTabController.stopOrReloadCurrentTab(/* ignoreCache= */ true);

        verify(mTab).reloadIgnoringCache();
    }

    @Test
    public void openHomepage_loadsHomePage() {
        mToolbarTabController.openHomepage();
        GURL homePageGurl = HomepageManager.getInstance().getHomepageGurl(/* isIncognito= */ false);
        if (homePageGurl.isEmpty()) {
            homePageGurl = UrlConstantResolverFactory.getOriginalResolver().getNtpGurl();
        }
        verify(mTab)
                .loadUrl(
                        argThat(
                                new LoadUrlParamsMatcher(
                                        new LoadUrlParams(
                                                homePageGurl, PageTransition.HOME_PAGE))));
    }

    @Test
    public void openHomepageInForegroundTab() {
        mToolbarTabController.openHomepageInNewTab(/* foregroundNewTab= */ true);
        GURL homePageGurl = HomepageManager.getInstance().getHomepageGurl(/* isIncognito= */ false);
        if (homePageGurl.isEmpty()) {
            homePageGurl = UrlConstantResolverFactory.getOriginalResolver().getNtpGurl();
        }
        verify(mTabCreator)
                .createNewTab(
                        argThat(
                                new LoadUrlParamsMatcher(
                                        new LoadUrlParams(homePageGurl, PageTransition.HOME_PAGE))),
                        eq(TabLaunchType.FROM_CHROME_UI),
                        eq(mTab));
    }

    @Test
    public void openHomepageInBackgroundTab() {
        mToolbarTabController.openHomepageInNewTab(/* foregroundNewTab= */ false);
        GURL homePageGurl = HomepageManager.getInstance().getHomepageGurl(/* isIncognito= */ false);
        if (homePageGurl.isEmpty()) {
            homePageGurl = UrlConstantResolverFactory.getOriginalResolver().getNtpGurl();
        }
        verify(mTabCreator)
                .createNewTab(
                        argThat(
                                new LoadUrlParamsMatcher(
                                        new LoadUrlParams(homePageGurl, PageTransition.HOME_PAGE))),
                        eq(TabLaunchType.FROM_LONGPRESS_BACKGROUND),
                        eq(mTab));
    }

    @Test
    public void testUsingCorrectTabSupplier_doesNotUseRegularTabSupplier() {
        setUpUsingCorrectTabSupplier();

        Assert.assertFalse(mToolbarTabController.back());
        Assert.assertFalse(mToolbarTabController.canGoBack());
    }

    @Test
    public void testBackInForegroundTab() {
        // Set up.
        doReturn(true).when(mTab).canGoBack();
        doReturn(mTab2)
                .when(mTabCreator)
                .createTabWithHistory(mTab, TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND);
        InOrder inOrder = inOrder(mTabCreator, mTab2);

        // Call backInNewTab with foregroundNewTab = true.
        mToolbarTabController.backInNewTab(/* foregroundNewTab= */ true);

        // Verify correctness.
        inOrder.verify(mTabCreator)
                .createTabWithHistory(mTab, TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND);
        inOrder.verify(mTab2).goBack();
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void testBackInBackgroundTab() {
        // Set up.
        doReturn(true).when(mTab).canGoBack();
        doReturn(mTab2)
                .when(mTabCreator)
                .createTabWithHistory(mTab, TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND);
        InOrder inOrder = inOrder(mTabCreator, mTab2);

        // Call backInNewTab with foregroundNewTab = false.
        mToolbarTabController.backInNewTab(/* foregroundNewTab= */ false);

        // Verify correctness.
        inOrder.verify(mTabCreator)
                .createTabWithHistory(mTab, TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND);
        inOrder.verify(mTab2).goBack();
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void testBackInNewWindow() {
        // Set up.
        doReturn(true).when(mTab).canGoBack();
        doReturn(mTab2)
                .when(mTabCreator)
                .createTabWithHistory(mTab, TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND);
        InOrder inOrder = inOrder(mTabCreator, mTab2, mMultiInstanceOrchestrator);

        // Call backInNewWindow.
        mToolbarTabController.backInNewWindow();

        // Verify correctness.
        inOrder.verify(mTabCreator)
                .createTabWithHistory(mTab, TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND);
        inOrder.verify(mTab2).goBack();
        inOrder.verify(mMultiInstanceOrchestrator)
                .moveTabsToNewWindow(
                        Collections.singletonList(mTab2),
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void testForwardInForegroundTab() {
        // Set up.
        doReturn(true).when(mTab).canGoForward();
        doReturn(mTab2)
                .when(mTabCreator)
                .createTabWithHistory(mTab, TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND);
        InOrder inOrder = inOrder(mTabCreator, mTab2);

        // Call forwardInNewTab with foregroundNewTab = true.
        mToolbarTabController.forwardInNewTab(/* foregroundNewTab= */ true);

        // Verify correctness.
        inOrder.verify(mTabCreator)
                .createTabWithHistory(mTab, TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND);
        inOrder.verify(mTab2).goForward();
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void testForwardInBackgroundTab() {
        // Set up.
        doReturn(true).when(mTab).canGoForward();
        doReturn(mTab2)
                .when(mTabCreator)
                .createTabWithHistory(mTab, TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND);
        InOrder inOrder = inOrder(mTabCreator, mTab2);

        // Call forwardInNewTab with foregroundNewTab = false.
        mToolbarTabController.forwardInNewTab(/* foregroundNewTab= */ false);

        // Verify correctness.
        inOrder.verify(mTabCreator)
                .createTabWithHistory(mTab, TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND);
        inOrder.verify(mTab2).goForward();
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void testForwardInNewWindow() {
        // Set up.
        doReturn(true).when(mTab).canGoForward();
        doReturn(mTab2)
                .when(mTabCreator)
                .createTabWithHistory(mTab, TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND);
        InOrder inOrder = inOrder(mTabCreator, mTab2, mMultiInstanceOrchestrator);

        // Call forwardInNewWindow.
        mToolbarTabController.forwardInNewWindow();

        // Verify correctness.
        inOrder.verify(mTabCreator)
                .createTabWithHistory(mTab, TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND);
        inOrder.verify(mTab2).goForward();
        inOrder.verify(mMultiInstanceOrchestrator)
                .moveTabsToNewWindow(
                        Collections.singletonList(mTab2),
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void openHomepage_NoTab_IncognitoSelected() {
        doReturn(null).when(mTabSupplier).get();
        doReturn(true).when(mIsOffTheRecordSupplier).get();

        mToolbarTabController.openHomepage();

        GURL homePageGurl = HomepageManager.getInstance().getHomepageGurl(/* isIncognito= */ true);
        if (homePageGurl.isEmpty()) {
            homePageGurl = UrlConstantResolverFactory.getOriginalResolver().getNtpGurl();
        }
        verify(mTabCreatorManager).getTabCreator(true);
        verify(mTabCreator)
                .createNewTab(
                        argThat(
                                new LoadUrlParamsMatcher(
                                        new LoadUrlParams(homePageGurl, PageTransition.HOME_PAGE))),
                        eq(TabLaunchType.FROM_CHROME_UI),
                        eq(null));
    }

    @Test
    public void openHomepageInNewTab_NoTab_IncognitoSelected() {
        doReturn(null).when(mTabSupplier).get();
        doReturn(true).when(mIsOffTheRecordSupplier).get();

        mToolbarTabController.openHomepageInNewTab(/* foregroundNewTab= */ true);

        GURL homePageGurl = HomepageManager.getInstance().getHomepageGurl(/* isIncognito= */ true);
        if (homePageGurl.isEmpty()) {
            homePageGurl = UrlConstantResolverFactory.getOriginalResolver().getNtpGurl();
        }
        verify(mTabCreatorManager).getTabCreator(true);
        verify(mTabCreator)
                .createNewTab(
                        argThat(
                                new LoadUrlParamsMatcher(
                                        new LoadUrlParams(homePageGurl, PageTransition.HOME_PAGE))),
                        eq(TabLaunchType.FROM_CHROME_UI),
                        eq(null));
    }

    private void initToolbarTabController() {
        UrlConstantResolver urlConstantResolver =
                UrlConstantResolverFactory.getForProfile(/* profile= */ null);
        mToolbarTabController =
                new ToolbarTabControllerImpl(
                        mTabSupplier,
                        mTrackerSupplier,
                        () -> mBottomControlsCoordinator,
                        urlConstantResolver::getNtpUrl,
                        mRunnable,
                        mActivityTabProvider,
                        mTabCreatorManager,
                        mIsOffTheRecordSupplier);
    }

    private void setUpUsingCorrectTabSupplier() {
        doReturn(mTab2).when(mActivityTabProvider).get();
        doReturn(false).when(mTab2).canGoBack();
        doReturn(true).when(mTab).canGoBack();
    }
}

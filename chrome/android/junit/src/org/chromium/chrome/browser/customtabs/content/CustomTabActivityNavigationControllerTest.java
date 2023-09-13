// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler.MinimizeAppAndCloseTabType;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishHandler;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.customtabs.shadows.ShadowExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.Features.JUnitProcessor;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.url.GURL;

/**
 * Unit tests for {@link CustomTabActivityNavigationController}.
 *
 * {@link CustomTabActivityNavigationController#navigate} is tested in integration with other
 * classes in {@link CustomTabActivityUrlLoadingTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowExternalNavigationDelegateImpl.class, ShadowPostTask.class})
public class CustomTabActivityNavigationControllerTest {
    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();
    @Rule
    public final JUnitProcessor mFeaturesProcessor = new JUnitProcessor();

    private CustomTabActivityNavigationController mNavigationController;

    @Mock CustomTabActivityTabController mTabController;
    @Mock FinishHandler mFinishHandler;

    @Before
    public void setUp() {
        ShadowPostTask.setTestImpl(new ShadowPostTask.TestImpl() {
            @Override
            public void postDelayedTask(@TaskTraits int taskTraits, Runnable task, long delay) {}
        });
        MockitoAnnotations.initMocks(this);
        mNavigationController = env.createNavigationController(mTabController);
        mNavigationController.setFinishHandler(mFinishHandler);
        Tab tab = env.prepareTab();
        when(tab.getUrl()).thenReturn(new GURL("")); // avoid DomDistillerUrlUtils going to native.
        env.tabProvider.setInitialTab(tab, TabCreationMode.DEFAULT);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void finishes_IfBackNavigationClosesTheOnlyTabWithNoUnloadEvents() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler.getHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.MINIMIZE_APP)
                        .expectIntRecord(BackPressManager.getHistogramForTesting(),
                                BackPressManager.getHistogramValueForTesting(
                                        BackPressHandler.Type.MINIMIZE_APP_AND_CLOSE_TAB))
                        .build();
        when(mTabController.onlyOneTabRemaining()).thenReturn(true);
        when(mTabController.dispatchBeforeUnloadIfNeeded()).thenReturn(false);
        Assert.assertTrue(mNavigationController.getHandleBackPressChangedSupplier().get());

        mNavigationController.navigateOnBack();
        histogramWatcher.assertExpected();
        verify(mFinishHandler).onFinish(eq(FinishReason.USER_NAVIGATION));
        env.tabProvider.removeTab();
        Assert.assertNull(env.tabProvider.getTab());
        Assert.assertFalse(mNavigationController.getHandleBackPressChangedSupplier().get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void finishes_IfBackNavigationClosesTheOnlyTabWithNoUnloadEvents_BackPressRefactor() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler.getHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.MINIMIZE_APP)
                        .expectNoRecords(BackPressManager.getHistogramForTesting())
                        .build();
        when(mTabController.onlyOneTabRemaining()).thenReturn(true);
        when(mTabController.dispatchBeforeUnloadIfNeeded()).thenReturn(false);
        Assert.assertTrue(mNavigationController.getHandleBackPressChangedSupplier().get());

        mNavigationController.navigateOnBack();
        histogramWatcher.assertExpected();
        verify(mFinishHandler).onFinish(eq(FinishReason.USER_NAVIGATION));
        env.tabProvider.removeTab();
        Assert.assertNull(env.tabProvider.getTab());
        Assert.assertFalse(mNavigationController.getHandleBackPressChangedSupplier().get());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void doesntFinish_IfBackNavigationReplacesTabWithPreviousOne() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler.getHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.CLOSE_TAB)
                        .expectIntRecord(BackPressManager.getHistogramForTesting(),
                                BackPressManager.getHistogramValueForTesting(
                                        BackPressHandler.Type.MINIMIZE_APP_AND_CLOSE_TAB))
                        .build();
        doAnswer((Answer<Void>) invocation -> {
            env.tabProvider.swapTab(env.prepareTab());
            return null;
        })
                .when(mTabController)
                .closeTab();
        Assert.assertTrue(mNavigationController.getHandleBackPressChangedSupplier().get());

        mNavigationController.navigateOnBack();
        histogramWatcher.assertExpected();
        verify(mFinishHandler, never()).onFinish(anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void doesntFinish_IfBackNavigationReplacesTabWithPreviousOne_BackPressRefactor() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler.getHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.CLOSE_TAB)
                        .expectNoRecords(BackPressManager.getHistogramForTesting())
                        .build();
        doAnswer((Answer<Void>) invocation -> {
            env.tabProvider.swapTab(env.prepareTab());
            return null;
        }).when(mTabController).closeTab();
        Assert.assertTrue(mNavigationController.getHandleBackPressChangedSupplier().get());

        mNavigationController.navigateOnBack();
        histogramWatcher.assertExpected();
        verify(mFinishHandler, never()).onFinish(anyInt());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void doesntFinish_IfBackNavigationHappensWithBeforeUnloadHandler() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler.getHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.CLOSE_TAB)
                        .expectIntRecord(BackPressManager.getHistogramForTesting(),
                                BackPressManager.getHistogramValueForTesting(
                                        BackPressHandler.Type.MINIMIZE_APP_AND_CLOSE_TAB))
                        .build();

        when(mTabController.dispatchBeforeUnloadIfNeeded()).thenReturn(true);

        mNavigationController.navigateOnBack();
        histogramWatcher.assertExpected();
        verify(mFinishHandler, never()).onFinish(anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void doesntFinish_IfBackNavigationHappensWithBeforeUnloadHandler_BackPressRefactor() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler.getHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.CLOSE_TAB)
                        .expectNoRecords(BackPressManager.getHistogramForTesting())
                        .build();

        when(mTabController.dispatchBeforeUnloadIfNeeded()).thenReturn(true);

        mNavigationController.navigateOnBack();
        histogramWatcher.assertExpected();
        verify(mFinishHandler, never()).onFinish(anyInt());
    }

    @Test
    public void startsReparenting_WhenOpenInBrowserCalled_AndChromeCanHandleIntent() {
        ShadowExternalNavigationDelegateImpl.setWillChromeHandleIntent(true);
        mNavigationController.openCurrentUrlInBrowser();
        verify(env.activity, never()).startActivity(any());
        verify(mTabController).detachAndStartReparenting(any(), any(), any());
    }

    @Test
    public void finishes_whenDoneReparenting() {
        ShadowExternalNavigationDelegateImpl.setWillChromeHandleIntent(true);
        ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        doNothing().when(mTabController).detachAndStartReparenting(any(), any(),
                captor.capture());

        mNavigationController.openCurrentUrlInBrowser();

        verify(mFinishHandler, never()).onFinish(anyInt());
        captor.getValue().run();
        verify(mFinishHandler).onFinish(FinishReason.REPARENTING);
    }

    @Test
    public void startsNewActivity_WhenOpenInBrowserCalled_AndChromeCanNotHandleIntent() {
        ShadowExternalNavigationDelegateImpl.setWillChromeHandleIntent(false);
        mNavigationController.openCurrentUrlInBrowser();
        verify(mTabController, never()).detachAndStartReparenting(any(), any(), any());
        verify(env.activity).startActivity(any(), any());
        verify(mFinishHandler).onFinish(FinishReason.OPEN_IN_BROWSER);
    }

    @Test
    public void startsNewActivity_WhenOpenInBrowserCalled_AndChromeCanHandleIntent_AndIsTwa() {
        ShadowExternalNavigationDelegateImpl.setWillChromeHandleIntent(true);
        when(env.intentDataProvider.getActivityType())
                .thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);

        mNavigationController.openCurrentUrlInBrowser();
        verify(mTabController, never()).detachAndStartReparenting(any(), any(), any());
        verify(env.activity).startActivity(any(), any());
    }

    @After
    public void tearDown() {
        ShadowPostTask.reset();
    }
}

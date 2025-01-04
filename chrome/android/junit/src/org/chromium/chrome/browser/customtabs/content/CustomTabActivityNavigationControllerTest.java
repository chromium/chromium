// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import com.google.common.collect.ImmutableList;

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

import org.chromium.base.ContextUtils;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler.MinimizeAppAndCloseTabType;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishHandler;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.customtabs.shadows.ShadowExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

/**
 * Unit tests for {@link CustomTabActivityNavigationController}.
 *
 * {@link CustomTabActivityNavigationController#navigate} is tested in integration with other
 * classes in {@link CustomTabActivityUrlLoadingTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.CCT_BEFORE_UNLOAD)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowExternalNavigationDelegateImpl.class, ShadowPostTask.class})
public class CustomTabActivityNavigationControllerTest {
    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    private CustomTabActivityNavigationController mNavigationController;
    private TestContext mTestContext;

    @Mock CustomTabActivityTabController mTabController;
    @Mock FinishHandler mFinishHandler;
    @Mock private PackageManager mPackageManager;
    @Mock private ResolveInfo mResolveInfo;

    class TestContext extends ContextWrapper {
        public TestContext(Context base) {
            super(base);
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(mPackageManager);
        }
    }

    @Before
    public void setUp() {
        ShadowPostTask.setTestImpl((@TaskTraits int taskTraits, Runnable task, long delay) -> {});
        MockitoAnnotations.initMocks(this);
        mTestContext = new TestContext(ContextUtils.getApplicationContext());
        ContextUtils.initApplicationContextForTests(mTestContext);
        mNavigationController = env.createNavigationController(mTabController);
        mNavigationController.setFinishHandler(mFinishHandler);
        Tab tab = env.prepareTab();
        when(tab.getUrl()).thenReturn(new GURL("")); // avoid DomDistillerUrlUtils going to native.
        env.tabProvider.setInitialTab(tab, TabCreationMode.DEFAULT);
        doReturn(ImmutableList.of(mResolveInfo))
                .when(mPackageManager)
                .queryIntentActivities(any(), anyInt());
    }

    @Test
    public void finishes_IfBackNavigationClosesTheOnlyTabWithNoUnloadEvents() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler.getHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.MINIMIZE_APP)
                        .expectNoRecords(BackPressManager.getHistogramForTesting())
                        .expectNoRecords(
                                MinimizeAppAndCloseTabBackPressHandler
                                        .getCustomTabSeparateTaskHistogramNameForTesting())
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler
                                        .getCustomTabSameTaskHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.MINIMIZE_APP)
                        .expectNoRecords(
                                BackPressManager.getCustomTabSeparateTaskHistogramForTesting())
                        .expectNoRecords(BackPressManager.getCustomTabSameTaskHistogramForTesting())
                        .build();
        when(mTabController.onlyOneTabRemaining()).thenReturn(true);
        when(mTabController.dispatchBeforeUnloadIfNeeded()).thenReturn(false);
        Assert.assertTrue(mNavigationController.getHandleBackPressChangedSupplier().get());

        mNavigationController.navigateOnBack();
        histogramWatcher.assertExpected();
        verify(mFinishHandler).onFinish(FinishReason.USER_NAVIGATION, true);
        env.tabProvider.removeTab();
        Assert.assertNull(env.tabProvider.getTab());
        Assert.assertFalse(mNavigationController.getHandleBackPressChangedSupplier().get());
    }

    @Test
    public void finishes_IfBackNavigationClosesTheOnlyTabWithUnloadHandler_CctBeforeUnload() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler.getHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.MINIMIZE_APP)
                        .expectNoRecords(BackPressManager.getHistogramForTesting())
                        .expectNoRecords(
                                MinimizeAppAndCloseTabBackPressHandler
                                        .getCustomTabSeparateTaskHistogramNameForTesting())
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler
                                        .getCustomTabSameTaskHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.MINIMIZE_APP)
                        .expectNoRecords(
                                BackPressManager.getCustomTabSeparateTaskHistogramForTesting())
                        .expectNoRecords(BackPressManager.getCustomTabSameTaskHistogramForTesting())
                        .build();
        when(mTabController.onlyOneTabRemaining()).thenReturn(true);
        when(mTabController.dispatchBeforeUnloadIfNeeded()).thenReturn(true);
        Assert.assertTrue(mNavigationController.getHandleBackPressChangedSupplier().get());

        mNavigationController.navigateOnBack();
        histogramWatcher.assertExpected();
        verify(mFinishHandler).onFinish(FinishReason.USER_NAVIGATION, true);
        env.tabProvider.removeTab();
        Assert.assertNull(env.tabProvider.getTab());
        Assert.assertFalse(mNavigationController.getHandleBackPressChangedSupplier().get());
    }

    @Test
    public void doesntFinish_IfBackNavigationReplacesTabWithPreviousOne() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler.getHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.CLOSE_TAB)
                        .expectNoRecords(BackPressManager.getHistogramForTesting())
                        .build();
        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    env.tabProvider.swapTab(env.prepareTab());
                                    return null;
                                })
                .when(mTabController)
                .closeTab();
        Assert.assertTrue(mNavigationController.getHandleBackPressChangedSupplier().get());

        mNavigationController.navigateOnBack();
        histogramWatcher.assertExpected();
        verify(mFinishHandler, never()).onFinish(anyInt(), anyBoolean());
    }

    @Test
    public void doesntFinish_IfBackNavigationHappensWithBeforeUnloadHandler() {
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
        verify(mFinishHandler, never()).onFinish(anyInt(), anyBoolean());
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
        doNothing().when(mTabController).detachAndStartReparenting(any(), any(), captor.capture());

        mNavigationController.openCurrentUrlInBrowser();

        verify(mFinishHandler, never()).onFinish(anyInt(), anyBoolean());
        captor.getValue().run();
        verify(mFinishHandler).onFinish(FinishReason.REPARENTING, false);
    }

    @Test
    public void startsNewActivity_WhenOpenInBrowserCalled_AndChromeCanNotHandleIntent() {
        ShadowExternalNavigationDelegateImpl.setWillChromeHandleIntent(false);
        mNavigationController.openCurrentUrlInBrowser();
        verify(mTabController, never()).detachAndStartReparenting(any(), any(), any());
        verify(env.activity).startActivity(any(), any());
        verify(mFinishHandler).onFinish(FinishReason.OPEN_IN_BROWSER, true);
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
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.window.OnBackInvokedDispatcher;

import com.google.common.collect.ImmutableList;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler.MinimizeAppAndCloseTabType;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishHandler;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManagerImpl;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

/**
 * Unit tests for {@link CustomTabActivityNavigationController}.
 *
 * <p>{@link CustomTabActivityNavigationController#navigate} is tested in integration with other
 * classes in {@link CustomTabActivityUrlLoadingTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowPostTask.class})
public class CustomTabActivityNavigationControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    private CustomTabActivityNavigationController mNavigationController;
    private TestContext mTestContext;

    @Mock CustomTabActivityTabController mTabController;
    @Mock FinishHandler mFinishHandler;
    @Mock OnBackInvokedDispatcher mDispatcher;
    @Mock private PackageManager mPackageManager;
    @Mock private ResolveInfo mResolveInfo;
    @Mock private ChromeTabbedActivity mAdjacentActivity;

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
        mTestContext = new TestContext(ContextUtils.getApplicationContext());
        ContextUtils.initApplicationContextForTests(mTestContext);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            when(env.activity.getOnBackInvokedDispatcher()).thenReturn(mDispatcher);
        }

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
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler.getHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB)
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler
                                        .getCustomTabSameTaskHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB)
                        .expectIntRecord(
                                CustomTabActivityNavigationController.HISTOGRAM_FINISH_REASON,
                                FinishReason.USER_NAVIGATION)
                        .expectNoRecords(
                                BackPressManager.getCustomTabSeparateTaskHistogramForTesting())
                        .expectNoRecords(BackPressManager.getCustomTabSameTaskHistogramForTesting())
                        .build();
        when(mTabController.onlyOneTabRemaining()).thenReturn(true);
        when(mTabController.dispatchBeforeUnloadIfNeeded()).thenReturn(false);
        mNavigationController
                .getTabObserverForTesting()
                .onInitialTabCreated(env.prepareTab(), TabCreationMode.DEFAULT);
        assertTrue(mNavigationController.getHandleBackPressChangedSupplier().get());

        mNavigationController.navigateOnBack(FinishReason.USER_NAVIGATION);
        histogramWatcher.assertExpected();
        verify(mFinishHandler).onFinish(FinishReason.USER_NAVIGATION, true);
        env.tabProvider.removeTab();
        Assert.assertNull(env.tabProvider.getTab());
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
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler.getHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB)
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler
                                        .getCustomTabSameTaskHistogramNameForTesting(),
                                MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB)
                        .expectIntRecord(
                                CustomTabActivityNavigationController.HISTOGRAM_FINISH_REASON,
                                FinishReason.USER_NAVIGATION)
                        .expectNoRecords(
                                BackPressManager.getCustomTabSeparateTaskHistogramForTesting())
                        .expectNoRecords(BackPressManager.getCustomTabSameTaskHistogramForTesting())
                        .build();
        when(mTabController.onlyOneTabRemaining()).thenReturn(true);
        when(mTabController.dispatchBeforeUnloadIfNeeded()).thenReturn(true);
        mNavigationController
                .getTabObserverForTesting()
                .onInitialTabCreated(env.prepareTab(), TabCreationMode.DEFAULT);
        assertTrue(mNavigationController.getHandleBackPressChangedSupplier().get());

        mNavigationController.navigateOnBack(FinishReason.USER_NAVIGATION);
        histogramWatcher.assertExpected();
        verify(mFinishHandler).onFinish(FinishReason.USER_NAVIGATION, true);
        env.tabProvider.removeTab();
        Assert.assertNull(env.tabProvider.getTab());
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
        assertTrue(mNavigationController.getHandleBackPressChangedSupplier().get());

        mNavigationController.navigateOnBack(FinishReason.USER_NAVIGATION);
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

        mNavigationController.navigateOnBack(FinishReason.USER_NAVIGATION);
        histogramWatcher.assertExpected();
        verify(mFinishHandler, never()).onFinish(anyInt(), anyBoolean());
    }

    @Test
    public void startsReparenting_WhenOpenInBrowserCalled_AndChromeCanHandleIntent() {
        ExternalNavigationDelegateImpl.setWillChromeHandleIntentHookForTesting(intent -> true);
        mNavigationController.openCurrentUrlInBrowser();
        verify(env.activity, never()).startActivity(any());
        verify(mTabController).detachAndStartReparenting(any(), any(), any());
    }

    @Test
    public void finishes_whenDoneReparenting() {
        ExternalNavigationDelegateImpl.setWillChromeHandleIntentHookForTesting(intent -> true);
        ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        doNothing().when(mTabController).detachAndStartReparenting(any(), any(), captor.capture());

        mNavigationController.openCurrentUrlInBrowser();

        verify(mFinishHandler, never()).onFinish(anyInt(), anyBoolean());
        captor.getValue().run();
        verify(mFinishHandler).onFinish(FinishReason.REPARENTING, false);
    }

    @Test
    public void finishes_whenDoneReparentingToAdjacentActivity() {
        ExternalNavigationDelegateImpl.setWillChromeHandleIntentHookForTesting(intent -> true);
        MultiInstanceManagerImpl.setAdjacentWindowActivitySupplierForTesting(
                () -> mAdjacentActivity);
        MultiWindowUtils.setActivitySupplierForTesting(() -> mAdjacentActivity);

        mNavigationController.openCurrentUrlInBrowser();

        verify(mAdjacentActivity, times(1)).onNewIntent(any());
        verify(mFinishHandler).onFinish(FinishReason.REPARENTING, false);
    }

    @Test
    public void startsNewActivity_WhenOpenInBrowserCalled_AndChromeCanNotHandleIntent() {
        ExternalNavigationDelegateImpl.setWillChromeHandleIntentHookForTesting(intent -> false);
        mNavigationController.openCurrentUrlInBrowser();
        verify(mTabController, never()).detachAndStartReparenting(any(), any(), any());
        verify(env.activity).startActivity(any(), any());
        verify(mFinishHandler).onFinish(FinishReason.OPEN_IN_BROWSER, true);
    }

    @Test
    public void startsNewActivity_WhenOpenInBrowserCalled_AndChromeCanHandleIntent_AndIsTwa() {
        ExternalNavigationDelegateImpl.setWillChromeHandleIntentHookForTesting(intent -> true);
        when(env.intentDataProvider.getActivityType())
                .thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);

        mNavigationController.openCurrentUrlInBrowser();
        verify(mTabController, never()).detachAndStartReparenting(any(), any(), any());
        verify(env.activity).startActivity(any(), any());
    }

    @Test
    public void observerDefaultsToOS_WhenOnlyOneTabRemains() {
        CustomTabActivityNavigationController.enablePredictiveBackGestureForTesting();
        when(mTabController.onlyOneTabRemaining()).thenReturn(false);
        when(mTabController.getTabCount()).thenReturn(2);
        when(mTabController.dispatchBeforeUnloadIfNeeded()).thenReturn(false);
        mNavigationController.getTabObserverForTesting().onTabSwapped(env.prepareTab());
        // With multiple tabs, predictive back enabled, and initial tab mode set (default),
        // Chrome should handle the back press.
        mNavigationController.getTabObserverForTesting().onTabSwapped(env.prepareTab());
        Assert.assertTrue(
                "Chrome should handle back press when multiple tabs are present.",
                mNavigationController.getHandleBackPressChangedSupplier().get());

        // Now, simulate only one tab remaining.
        mNavigationController.navigateOnBack(FinishReason.HANDLED_BY_OS);
        when(mTabController.onlyOneTabRemaining()).thenReturn(true);
        // When only one tab remains, and predictive back conditions are met,
        // the OS should handle the back press (supplier should be false).
        mNavigationController.getTabObserverForTesting().onTabSwapped(env.prepareTab());
        Assert.assertFalse(
                "OS should handle back press when only one tab remains.",
                mNavigationController.getHandleBackPressChangedSupplier().get());
    }

    @Test
    public void observerDefaultsToOS_WhenInitialTabCountNotReflected() {
        CustomTabActivityNavigationController.enablePredictiveBackGestureForTesting();
        when(mTabController.onlyOneTabRemaining()).thenReturn(false);
        when(mTabController.getTabCount()).thenReturn(0);
        when(mTabController.dispatchBeforeUnloadIfNeeded()).thenReturn(false);
        mNavigationController.getTabObserverForTesting().onTabSwapped(env.prepareTab());
        Assert.assertFalse(mNavigationController.getHandleBackPressChangedSupplier().get());

        mNavigationController.navigateOnBack(FinishReason.HANDLED_BY_OS);
        when(mTabController.onlyOneTabRemaining()).thenReturn(true);
        mNavigationController
                .getTabObserverForTesting()
                .onInitialTabCreated(env.prepareTab(), TabCreationMode.EARLY);
        Assert.assertFalse(mNavigationController.getHandleBackPressChangedSupplier().get());
    }

    @Test
    public void finishes_WhenOnAllTabsClosed() {
        CustomTabActivityNavigationController.enablePredictiveBackGestureForTesting();
        when(mTabController.onlyOneTabRemaining()).thenReturn(true);
        when(mTabController.dispatchBeforeUnloadIfNeeded()).thenReturn(false);
        mNavigationController.getTabObserverForTesting().onTabSwapped(env.prepareTab());
        Assert.assertFalse(mNavigationController.getHandleBackPressChangedSupplier().get());

        mNavigationController.getTabObserverForTesting().onAllTabsClosed();
        Assert.assertFalse(mNavigationController.getHandleBackPressChangedSupplier().get());
        verify(mFinishHandler).onFinish(anyInt(), anyBoolean());
    }

    @Test
    public void observerDoesNotDefaultToOS_WhenPartialCCT() {
        CustomTabActivityNavigationController.enablePredictiveBackGestureForTesting();
        when(mTabController.onlyOneTabRemaining()).thenReturn(true);
        when(mTabController.dispatchBeforeUnloadIfNeeded()).thenReturn(false);
        when(mNavigationController.getIntentDataProviderForTesting().isPartialCustomTab())
                .thenReturn(true);
        mNavigationController
                .getTabObserverForTesting()
                .onInitialTabCreated(env.prepareTab(), TabCreationMode.DEFAULT);
        assertTrue(mNavigationController.getHandleBackPressChangedSupplier().get());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public void predictiveBackGesture_RequiresAndroidBaklava() {
        Assert.assertFalse(CustomTabActivityNavigationController.supportsPredictiveBackGesture());

        // Sets the Android version to Baklava.
        CustomTabActivityNavigationController.enablePredictiveBackGestureForTesting();

        assertTrue(CustomTabActivityNavigationController.supportsPredictiveBackGesture());
    }

    @Test
    public void getVersionForTesting_ReturnsSetVersion() {
        Assert.assertFalse(CustomTabActivityNavigationController.supportsPredictiveBackGesture());

        // Sets the Android version to Baklava.
        CustomTabActivityNavigationController.enablePredictiveBackGestureForTesting();

        Assert.assertEquals(
                "The version should be 36, which is the Android API level for Baklava.",
                /*Android 16 API level*/ 36,
                (int) mNavigationController.getVersionForTesting());
    }

    @Test
    public void whenCallbackInvoked_FinishesWithReasonHandledByOS() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                CustomTabActivityNavigationController.HISTOGRAM_FINISH_REASON,
                                FinishReason.HANDLED_BY_OS)
                        .expectNoRecords(BackPressManager.getHistogramForTesting())
                        .build();
        when(mTabController.onlyOneTabRemaining()).thenReturn(true);
        when(mTabController.dispatchBeforeUnloadIfNeeded()).thenReturn(true);
        CustomTabActivityNavigationController.enablePredictiveBackGestureForTesting();
        mNavigationController.onSystemNavigation();
        histogramWatcher.assertExpected();

        verify(mFinishHandler).onFinish(FinishReason.HANDLED_BY_OS, true);
    }
}

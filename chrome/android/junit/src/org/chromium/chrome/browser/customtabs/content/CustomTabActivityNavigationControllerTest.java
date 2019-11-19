// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
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
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.BackHandler;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishHandler;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.customtabs.shadows.ShadowExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for {@link CustomTabActivityNavigationController}.
 *
 * {@link CustomTabActivityNavigationController#navigate} is tested in integration with other
 * classes in {@link CustomTabActivityUrlLoadingTest}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {
    ShadowExternalNavigationDelegateImpl.class, ShadowPostTask.class})
public class CustomTabActivityNavigationControllerTest {

    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    private CustomTabActivityNavigationController mNavigationController;

    @Mock CustomTabActivityTabController mTabController;
    @Mock FinishHandler mFinishHandler;

    @Before
    public void setUp() {
        ShadowPostTask.setTestImpl(new ShadowPostTask.TestImpl() {
            @Override
            public void postDelayedTask(TaskTraits taskTraits, Runnable task, long delay) {}
        });
        MockitoAnnotations.initMocks(this);
        mNavigationController = env.createNavigationController(mTabController);
        mNavigationController.setFinishHandler(mFinishHandler);
        Tab tab = env.prepareTab();
        when(tab.getUrl()).thenReturn(""); // avoid DomDistillerUrlUtils going to native.
        env.tabProvider.setInitialTab(tab, TabCreationMode.DEFAULT);
    }

    @Test
    public void handlesBackNavigation_IfExternalBackHandlerRejectsSynchronously() {
        mNavigationController.setBackHandler(notHandledRunnable -> false);
        mNavigationController.navigateOnBack();
        verify(mTabController).closeTab();
    }

    @Test
    public void handlesBackNavigation_IfExternalBackHandlerRejectsAsynchronously() {
        ArgumentCaptor<Runnable> notHandledRunnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        BackHandler backHandler = mock(BackHandler.class);
        doReturn(true).when(backHandler).handleBackPressed(notHandledRunnableCaptor.capture());
        mNavigationController.setBackHandler(backHandler);
        mNavigationController.navigateOnBack();
        notHandledRunnableCaptor.getValue().run();
        verify(mTabController).closeTab();
    }

    @Test
    public void doesntHandlesBackNavigation_IfExternalBackHandlerAccepts() {
        mNavigationController.setBackHandler(notHandledRunnable -> true);
        mNavigationController.navigateOnBack();
        verify(mTabController, never()).closeTab();
    }

    @Test
    public void finishes_IfBackNavigationClosesTheOnlyTab() {
        doAnswer((Answer<Void>) invocation -> {
            env.tabProvider.swapTab(null);
            return null;
        }).when(mTabController).closeTab();

        mNavigationController.navigateOnBack();
        verify(mFinishHandler).onFinish(eq(FinishReason.USER_NAVIGATION));
    }

    @Test
    public void doesntFinish_IfBackNavigationReplacesTabWithPreviousOne() {
        doAnswer((Answer<Void>) invocation -> {
            env.tabProvider.swapTab(env.prepareTab());
            return null;
        }).when(mTabController).closeTab();

        mNavigationController.navigateOnBack();
        verify(mFinishHandler, never()).onFinish(anyInt());
    }

    @Test
    public void startsReparenting_WhenOpenInBrowserCalled_AndChromeCanHandleIntent() {
        ShadowExternalNavigationDelegateImpl.setWillChromeHandleIntent(true);
        mNavigationController.openCurrentUrlInBrowser(false);
        verify(env.activity, never()).startActivity(any());
        verify(mTabController).detachAndStartReparenting(any(), any(), any());
    }

    @Test
    public void finishes_whenDoneReparenting() {
        ShadowExternalNavigationDelegateImpl.setWillChromeHandleIntent(true);
        ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        doNothing().when(mTabController).detachAndStartReparenting(any(), any(),
                captor.capture());

        mNavigationController.openCurrentUrlInBrowser(false);

        verify(mFinishHandler, never()).onFinish(anyInt());
        captor.getValue().run();
        verify(mFinishHandler).onFinish(FinishReason.REPARENTING);
    }

    @Test
    public void startsNewActivity_WhenOpenInBrowserCalled_AndChromeCanNotHandleIntent() {
        ShadowExternalNavigationDelegateImpl.setWillChromeHandleIntent(false);
        mNavigationController.openCurrentUrlInBrowser(false);
        verify(mTabController, never()).detachAndStartReparenting(any(), any(), any());
        verify(env.activity).startActivity(any(), any());
    }

    @After
    public void tearDown() {
        ShadowPostTask.reset();
    }
}

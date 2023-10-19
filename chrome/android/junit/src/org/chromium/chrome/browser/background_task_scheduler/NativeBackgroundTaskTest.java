// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_task_scheduler;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.app.Notification;
import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerExternalUma;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.content_public.browser.BrowserStartupController;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link NativeBackgroundTask}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class NativeBackgroundTaskTest {
    private enum InitializerSetup {
        SUCCESS,
        FAILURE,
        EXCEPTION,
    }

    private static class LazyTaskParameters {
        static final TaskParameters INSTANCE = TaskParameters.create(TaskIds.TEST).build();
    }

    private static TaskParameters getTaskParameters() {
        return LazyTaskParameters.INSTANCE;
    }

    private static class TestBrowserStartupController implements BrowserStartupController {
        private boolean mStartupSucceeded;
        private int mCallCount;

        @Override
        public void startBrowserProcessesAsync(
                @LibraryProcessType int libraryProcessType,
                boolean startGpuProcess,
                boolean startMinimalBrowser,
                final StartupCallback callback) {}

        @Override
        public void startBrowserProcessesSync(
                @LibraryProcessType int libraryProcessType,
                boolean singleProcess,
                boolean startGpuProcess) {}

        @Override
        public boolean isFullBrowserStarted() {
            mCallCount++;
            return mStartupSucceeded;
        }

        @Override
        public boolean isNativeStarted() {
            return mStartupSucceeded;
        }

        @Override
        public boolean isRunningInMinimalBrowserMode() {
            return false;
        }

        @Override
        public void addStartupCompletedObserver(StartupCallback callback) {}

        @Override
        public void setContentMainCallbackForTests(Runnable r) {}

        @Override
        public int getStartupMode(boolean startMinimalBrowser) {
            assertFalse(isNativeStarted());
            return 0 /*ServicificationStartupUma.ServicificationStartup.CHROME_COLD*/;
        }

        public void setIsStartupSuccessfullyCompleted(boolean flag) {
            mStartupSucceeded = flag;
        }

        public int completedCallCount() {
            return mCallCount;
        }
    }

    private TestBrowserStartupController mBrowserStartupController;
    private TaskFinishedCallback mCallback;
    private TestNativeBackgroundTask mTask;

    @Rule public final JniMocker mocker = new JniMocker();
    @Mock private ChromeBrowserInitializer mChromeBrowserInitializer;
    @Captor ArgumentCaptor<BrowserParts> mBrowserParts;

    @Mock private BackgroundTaskSchedulerExternalUma mExternalUmaMock;

    private static class TaskFinishedCallback implements BackgroundTask.TaskFinishedCallback {
        private boolean mWasCalled;
        private boolean mNeedsReschedule;
        private CountDownLatch mCallbackLatch;

        TaskFinishedCallback() {
            mCallbackLatch = new CountDownLatch(1);
        }

        @Override
        public void taskFinished(boolean needsReschedule) {
            mNeedsReschedule = needsReschedule;
            mWasCalled = true;
            mCallbackLatch.countDown();
        }

        @Override
        public void setNotification(int notificationId, Notification notification) {}

        boolean wasCalled() {
            return mWasCalled;
        }

        boolean needsRescheduling() {
            return mNeedsReschedule;
        }

        boolean waitOnCallback() {
            return waitOnLatch(mCallbackLatch);
        }
    }

    private static class TestNativeBackgroundTask extends NativeBackgroundTask {
        @StartBeforeNativeResult private int mStartBeforeNativeResult;
        private boolean mWasOnStartTaskWithNativeCalled;
        private boolean mNeedsReschedulingAfterStop;
        private CountDownLatch mStartWithNativeLatch;
        private boolean mWasOnStopTaskWithNativeCalled;
        private boolean mWasOnStopTaskBeforeNativeLoadedCalled;
        private BrowserStartupController mBrowserStartupController;

        public TestNativeBackgroundTask(BrowserStartupController controller) {
            super();
            setDelegate(new ChromeNativeBackgroundTaskDelegate());
            mBrowserStartupController = controller;
            mWasOnStartTaskWithNativeCalled = false;
            mStartBeforeNativeResult = StartBeforeNativeResult.LOAD_NATIVE;
            mNeedsReschedulingAfterStop = false;
            mStartWithNativeLatch = new CountDownLatch(1);
        }

        @Override
        protected int onStartTaskBeforeNativeLoaded(
                Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
            return mStartBeforeNativeResult;
        }

        @Override
        protected void onStartTaskWithNative(
                Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
            assertEquals(ContextUtils.getApplicationContext(), context);
            assertEquals(getTaskParameters(), taskParameters);
            mWasOnStartTaskWithNativeCalled = true;
            mStartWithNativeLatch.countDown();
        }

        @Override
        protected boolean onStopTaskBeforeNativeLoaded(
                Context context, TaskParameters taskParameters) {
            mWasOnStopTaskBeforeNativeLoadedCalled = true;
            return mNeedsReschedulingAfterStop;
        }

        @Override
        protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
            mWasOnStopTaskWithNativeCalled = true;
            return mNeedsReschedulingAfterStop;
        }

        boolean waitOnStartWithNativeCallback() {
            return waitOnLatch(mStartWithNativeLatch);
        }

        boolean wasOnStartTaskWithNativeCalled() {
            return mWasOnStartTaskWithNativeCalled;
        }

        boolean wasOnStopTaskWithNativeCalled() {
            return mWasOnStopTaskWithNativeCalled;
        }

        boolean wasOnStopTaskBeforeNativeLoadedCalled() {
            return mWasOnStopTaskBeforeNativeLoadedCalled;
        }

        void setStartTaskBeforeNativeResult(@StartBeforeNativeResult int result) {
            mStartBeforeNativeResult = result;
        }

        void setNeedsReschedulingAfterStop(boolean needsReschedulingAfterStop) {
            mNeedsReschedulingAfterStop = needsReschedulingAfterStop;
        }

        @Override
        public BrowserStartupController getBrowserStartupController() {
            return mBrowserStartupController;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mBrowserStartupController = new TestBrowserStartupController();
        mCallback = new TaskFinishedCallback();
        mTask = new TestNativeBackgroundTask(mBrowserStartupController);
        BackgroundTaskSchedulerFactory.setUmaReporterForTesting(mExternalUmaMock);
        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);
        ChromeBrowserInitializer.setBrowserStartupControllerForTesting(mBrowserStartupController);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(mExternalUmaMock);
    }

    private void setUpChromeBrowserInitializer(InitializerSetup setup) {
        doNothing()
                .when(mChromeBrowserInitializer)
                .handlePreNativeStartupAndLoadLibraries(any(BrowserParts.class));
        switch (setup) {
            case SUCCESS:
                doAnswer(
                                new Answer<Void>() {
                                    @Override
                                    public Void answer(InvocationOnMock invocation) {
                                        mBrowserParts.getValue().finishNativeInitialization();
                                        return null;
                                    }
                                })
                        .when(mChromeBrowserInitializer)
                        .handlePostNativeStartup(eq(true), mBrowserParts.capture());
                break;
            case FAILURE:
                doAnswer(
                                new Answer<Void>() {
                                    @Override
                                    public Void answer(InvocationOnMock invocation) {
                                        mBrowserParts.getValue().onStartupFailure(null);
                                        return null;
                                    }
                                })
                        .when(mChromeBrowserInitializer)
                        .handlePostNativeStartup(eq(true), mBrowserParts.capture());
                break;
            case EXCEPTION:
                doThrow(new ProcessInitException(LoaderErrors.NATIVE_LIBRARY_LOAD_FAILED))
                        .when(mChromeBrowserInitializer)
                        .handlePostNativeStartup(eq(true), any(BrowserParts.class));
                break;
            default:
                assert false;
        }
    }

    private void verifyStartupCalls(int expectedPreNativeCalls, int expectedPostNativeCalls) {
        verify(mChromeBrowserInitializer, times(expectedPreNativeCalls))
                .handlePreNativeStartupAndLoadLibraries(any(BrowserParts.class));
        verify(mChromeBrowserInitializer, times(expectedPostNativeCalls))
                .handlePostNativeStartup(eq(true), any(BrowserParts.class));
    }

    private static boolean waitOnLatch(CountDownLatch latch) {
        try {
            // All tests are expected to get it done much faster
            return latch.await(5, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            return false;
        }
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStartTask_Done_BeforeNativeLoaded() {
        mTask.setStartTaskBeforeNativeResult(NativeBackgroundTask.StartBeforeNativeResult.DONE);
        assertFalse(
                mTask.onStartTask(
                        ContextUtils.getApplicationContext(), getTaskParameters(), mCallback));

        assertEquals(0, mBrowserStartupController.completedCallCount());
        verifyStartupCalls(0, 0);
        assertFalse(mTask.wasOnStartTaskWithNativeCalled());
        assertFalse(mCallback.wasCalled());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStartTask_Reschedule_BeforeNativeLoaded() {
        mTask.setStartTaskBeforeNativeResult(
                NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE);
        assertTrue(
                mTask.onStartTask(
                        ContextUtils.getApplicationContext(), getTaskParameters(), mCallback));

        assertTrue(mCallback.waitOnCallback());
        assertEquals(0, mBrowserStartupController.completedCallCount());
        verifyStartupCalls(0, 0);
        assertFalse(mTask.wasOnStartTaskWithNativeCalled());
        assertTrue(mCallback.wasCalled());
        assertTrue(mCallback.needsRescheduling());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStartTask_NativeAlreadyLoaded() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(true);
        mTask.onStartTask(ContextUtils.getApplicationContext(), getTaskParameters(), mCallback);

        assertTrue(mTask.waitOnStartWithNativeCallback());
        assertEquals(1, mBrowserStartupController.completedCallCount());
        verifyStartupCalls(0, 0);
        assertTrue(mTask.wasOnStartTaskWithNativeCalled());
        assertFalse(mCallback.wasCalled());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStartTask_NativeInitialization_Success() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(false);
        setUpChromeBrowserInitializer(InitializerSetup.SUCCESS);
        mTask.onStartTask(ContextUtils.getApplicationContext(), getTaskParameters(), mCallback);

        assertTrue(mTask.waitOnStartWithNativeCallback());
        assertEquals(1, mBrowserStartupController.completedCallCount());
        verifyStartupCalls(1, 1);
        assertTrue(mTask.wasOnStartTaskWithNativeCalled());
        assertFalse(mCallback.wasCalled());
        verify(mExternalUmaMock).reportTaskStartedNative(TaskIds.TEST);
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStartTask_NativeInitialization_Failure() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(false);
        setUpChromeBrowserInitializer(InitializerSetup.FAILURE);
        mTask.onStartTask(ContextUtils.getApplicationContext(), getTaskParameters(), mCallback);

        assertTrue(mCallback.waitOnCallback());
        assertEquals(1, mBrowserStartupController.completedCallCount());
        verifyStartupCalls(1, 1);
        assertFalse(mTask.wasOnStartTaskWithNativeCalled());
        assertTrue(mCallback.wasCalled());
        assertTrue(mCallback.needsRescheduling());
        verify(mExternalUmaMock).reportTaskStartedNative(TaskIds.TEST);
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStartTask_NativeInitialization_Throws() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(false);
        setUpChromeBrowserInitializer(InitializerSetup.EXCEPTION);
        mTask.onStartTask(ContextUtils.getApplicationContext(), getTaskParameters(), mCallback);

        assertTrue(mCallback.waitOnCallback());
        assertEquals(1, mBrowserStartupController.completedCallCount());
        verifyStartupCalls(1, 1);
        assertFalse(mTask.wasOnStartTaskWithNativeCalled());
        assertTrue(mCallback.wasCalled());
        assertTrue(mCallback.needsRescheduling());
        verify(mExternalUmaMock).reportTaskStartedNative(TaskIds.TEST);
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStopTask_BeforeNativeLoaded_NeedsRescheduling() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(false);
        mTask.onStartTask(ContextUtils.getApplicationContext(), getTaskParameters(), mCallback);
        mTask.setNeedsReschedulingAfterStop(true);

        assertTrue(mTask.onStopTask(ContextUtils.getApplicationContext(), getTaskParameters()));
        assertTrue(mTask.wasOnStopTaskBeforeNativeLoadedCalled());
        assertFalse(mTask.wasOnStopTaskWithNativeCalled());
        verify(mExternalUmaMock).reportTaskStartedNative(TaskIds.TEST);
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStopTask_BeforeNativeLoaded_DoesntNeedRescheduling() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(false);
        mTask.onStartTask(ContextUtils.getApplicationContext(), getTaskParameters(), mCallback);
        mTask.setNeedsReschedulingAfterStop(false);

        assertFalse(mTask.onStopTask(ContextUtils.getApplicationContext(), getTaskParameters()));
        assertTrue(mTask.wasOnStopTaskBeforeNativeLoadedCalled());
        assertFalse(mTask.wasOnStopTaskWithNativeCalled());
        verify(mExternalUmaMock).reportTaskStartedNative(TaskIds.TEST);
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStopTask_NativeLoaded_NeedsRescheduling() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(true);
        mTask.onStartTask(ContextUtils.getApplicationContext(), getTaskParameters(), mCallback);
        mTask.setNeedsReschedulingAfterStop(true);

        assertTrue(mTask.onStopTask(ContextUtils.getApplicationContext(), getTaskParameters()));
        assertFalse(mTask.wasOnStopTaskBeforeNativeLoadedCalled());
        assertTrue(mTask.wasOnStopTaskWithNativeCalled());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStopTask_NativeLoaded_DoesntNeedRescheduling() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(true);
        mTask.onStartTask(ContextUtils.getApplicationContext(), getTaskParameters(), mCallback);
        mTask.setNeedsReschedulingAfterStop(false);

        assertFalse(mTask.onStopTask(ContextUtils.getApplicationContext(), getTaskParameters()));
        assertFalse(mTask.wasOnStopTaskBeforeNativeLoadedCalled());
        assertTrue(mTask.wasOnStopTaskWithNativeCalled());
    }
}

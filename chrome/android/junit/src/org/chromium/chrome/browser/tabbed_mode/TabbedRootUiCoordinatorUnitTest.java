// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Answers.CALLS_REAL_METHODS;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityResultWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omaha.inline.InAppUpdateController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.loading.LoadingFullscreenCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.BooleanSupplier;

/** Unit tests for {@link TabbedRootUiCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabbedRootUiCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock(answer = CALLS_REAL_METHODS)
    private TabbedRootUiCoordinator mCoordinator;

    @Mock private ScrimManager mScrimManager;
    @Mock private Profile mOriginalProfile;
    @Mock private Profile mCurrentProfile;
    @Mock private UpdateMenuItemHelper mUpdateMenuItemHelper;
    @Mock private InAppUpdateController mMockInAppUpdateController;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private DeferredStartupHandler mDeferredStartupHandler;

    private AppCompatActivity mActivity;
    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableMonotonicObservableSupplier<SnackbarManager> mSnackbarManagerSupplier =
            ObservableSuppliers.createMonotonic();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        DeferredStartupHandler.setInstanceForTests(mDeferredStartupHandler);
        UpdateMenuItemHelper.setInstanceForTesting(mUpdateMenuItemHelper);
        doReturn(mOriginalProfile).when(mCurrentProfile).getOriginalProfile();
        mProfileSupplier.set(mCurrentProfile);
    }

    @After
    public void tearDown() {
        DeferredStartupHandler.setInstanceForTests(null);
        UpdateMenuItemHelper.setInstanceForTesting(null);
    }

    private void initCoordinatorFields() {
        initCoordinatorFields(
                mActivity, mActivityLifecycleDispatcher, mSnackbarManagerSupplier, () -> true);
    }

    private void initCoordinatorFields(
            @Nullable AppCompatActivity activity,
            @Nullable ActivityLifecycleDispatcher lifecycleDispatcher,
            @Nullable MonotonicObservableSupplier<SnackbarManager> snackbarSupplier,
            @Nullable BooleanSupplier isWarmOnResumeSupplier) {
        mCoordinator.setActivityForTesting(activity);
        ReflectionHelpers.setField(
                mCoordinator, "mActivityLifecycleDispatcher", lifecycleDispatcher);
        ReflectionHelpers.setField(mCoordinator, "mSnackbarManagerSupplier", snackbarSupplier);
        ReflectionHelpers.setField(mCoordinator, "mProfileSupplier", mProfileSupplier);
        ReflectionHelpers.setField(mCoordinator, "mIsWarmOnResumeSupplier", isWarmOnResumeSupplier);
    }

    /**
     * The loading fullscreen coordinator is created lazily on the first call to {@link
     * TabbedRootUiCoordinator#getLoadingFullscreenCoordinator()}. Its only client, the
     * collaboration controller delegate factory, can run after the coordinator was destroyed, and
     * the coordinator must not be resurrected then: the activity is gone and the loading view stub
     * was already consumed by the first inflation.
     *
     * <p>After {@link TabbedRootUiCoordinator#onDestroy()}, {@code mLoadingFullscreenCoordinator}
     * and {@code mActivity} (nulled by {@link RootUiCoordinator#onDestroy()}) are both null. The
     * real constructor requires the full activity graph, so those post-destroy field states are
     * simulated via {@link RootUiCoordinator#destroyActivityForTesting()}.
     */
    @Test
    public void testGetLoadingFullscreenCoordinatorAfterDestroy() {
        mCoordinator.destroyActivityForTesting();
        assertNull(mCoordinator.getLoadingFullscreenCoordinator());
    }

    /**
     * The loading fullscreen coordinator is created at most once: the loading view stub is consumed
     * by the first inflation, so every later call must hand back the existing instance instead of
     * inflating the stub a second time.
     */
    @Test
    public void testGetLoadingFullscreenCoordinator_cachesResultAfterFirstCreation() {
        AppCompatActivity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        FrameLayout root = new FrameLayout(activity);
        ViewStub loadingStub = new ViewStub(activity);
        loadingStub.setId(R.id.loading_stub);
        AtomicInteger inflateCount = new AtomicInteger();
        loadingStub.setOnInflateListener((stub, inflated) -> inflateCount.incrementAndGet());
        root.addView(loadingStub);
        activity.setContentView(root);

        mCoordinator.setActivityForTesting(activity);
        doReturn(mScrimManager).when(mCoordinator).getScrimManager();

        LoadingFullscreenCoordinator first = mCoordinator.getLoadingFullscreenCoordinator();
        LoadingFullscreenCoordinator second = mCoordinator.getLoadingFullscreenCoordinator();

        assertNotNull(first);
        assertSame(first, second);
        assertEquals(1, inflateCount.get());
    }

    @Test
    public void testInAppUpdate_handleMenuOrKeyboardAction_updateMenuId_delegatesToHelper() {
        initCoordinatorFields();
        mCoordinator.setInAppUpdateControllerForTesting(mMockInAppUpdateController);

        boolean handled =
                mCoordinator.handleMenuOrKeyboardAction(R.id.update_menu_id, /* fromMenu= */ true);

        assertTrue(handled);
        verify(mCurrentProfile).getOriginalProfile();
        verify(mUpdateMenuItemHelper).onMenuItemClicked(mActivity, mMockInAppUpdateController);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void
            testInAppUpdate_initObserversAndScheduleDeferredStartup_registersObserversAndAddsDeferredTask() {
        initCoordinatorFields();

        mCoordinator.initInAppUpdateObservers();
        mCoordinator.scheduleInAppUpdateDeferredStartup();

        assertNotNull(mCoordinator.getActivityResultObserverForTesting());
        assertNotNull(mCoordinator.getResumeObserverForTesting());
        verify(mActivityLifecycleDispatcher)
                .register(mCoordinator.getActivityResultObserverForTesting());
        verify(mActivityLifecycleDispatcher).register(mCoordinator.getResumeObserverForTesting());
        verify(mDeferredStartupHandler).addDeferredTask(any());

        // Verify idempotency: a second call does not queue another deferred task.
        mCoordinator.scheduleInAppUpdateDeferredStartup();
        verify(mDeferredStartupHandler).addDeferredTask(any());
    }

    @Test
    public void testInAppUpdate_activityResultObserver_forwardsResultToController() {
        initCoordinatorFields();
        mCoordinator.setInAppUpdateControllerForTesting(mMockInAppUpdateController);

        mCoordinator.initInAppUpdateObservers();

        ActivityResultWithNativeObserver observer =
                mCoordinator.getActivityResultObserverForTesting();
        assertNotNull(observer);

        // Matching request code forwards to controller.
        observer.onActivityResultWithNative(
                InAppUpdateController.REQUEST_CODE_IN_APP_UPDATE, Activity.RESULT_OK, null);
        verify(mMockInAppUpdateController)
                .handleActivityResult(
                        InAppUpdateController.REQUEST_CODE_IN_APP_UPDATE, Activity.RESULT_OK);

        // Unrelated request code does not forward.
        observer.onActivityResultWithNative(99999, Activity.RESULT_CANCELED, null);
        verify(mMockInAppUpdateController, never()).handleActivityResult(eq(99999), anyInt());
    }

    @Test
    public void testInAppUpdate_resumeObserver_resumesWarmOnly() {
        AtomicBoolean isWarm = new AtomicBoolean(false);
        initCoordinatorFields(
                mActivity, mActivityLifecycleDispatcher, mSnackbarManagerSupplier, isWarm::get);
        mCoordinator.setInAppUpdateControllerForTesting(mMockInAppUpdateController);

        mCoordinator.initInAppUpdateObservers();

        PauseResumeWithNativeObserver observer = mCoordinator.getResumeObserverForTesting();
        assertNotNull(observer);

        // When cold/not warm on resume, controller.onResume() is not called.
        isWarm.set(false);
        observer.onResumeWithNative();
        verify(mMockInAppUpdateController, never()).onResume();

        // When warm on resume, controller.onResume() is called.
        isWarm.set(true);
        observer.onResumeWithNative();
        verify(mMockInAppUpdateController).onResume();
    }

    @Test
    public void testInAppUpdate_destroy_unregistersObserversAndCleansUpController() {
        initCoordinatorFields();
        mCoordinator.setInAppUpdateControllerForTesting(mMockInAppUpdateController);

        mCoordinator.initInAppUpdateObservers();

        ActivityResultWithNativeObserver activityResultObserver =
                mCoordinator.getActivityResultObserverForTesting();
        PauseResumeWithNativeObserver resumeObserver = mCoordinator.getResumeObserverForTesting();
        assertNotNull(activityResultObserver);
        assertNotNull(resumeObserver);

        mCoordinator.destroyInAppUpdate();

        verify(mActivityLifecycleDispatcher).unregister(activityResultObserver);
        verify(mActivityLifecycleDispatcher).unregister(resumeObserver);
        verify(mMockInAppUpdateController).destroy();
        assertNull(mCoordinator.getInAppUpdateControllerForTesting());
        assertNull(mCoordinator.getActivityResultObserverForTesting());
        assertNull(mCoordinator.getResumeObserverForTesting());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testInAppUpdate_featureDisabled_noControllerCreated() {
        initCoordinatorFields();

        mCoordinator.initInAppUpdateObservers();
        mCoordinator.scheduleInAppUpdateDeferredStartup();

        assertNull(mCoordinator.getInAppUpdateControllerForTesting());
        verify(mDeferredStartupHandler, never()).addDeferredTask(any());

        ActivityResultWithNativeObserver observer =
                mCoordinator.getActivityResultObserverForTesting();
        assertNotNull(observer);
        observer.onActivityResultWithNative(
                InAppUpdateController.REQUEST_CODE_IN_APP_UPDATE, Activity.RESULT_OK, null);
        assertNull(mCoordinator.getInAppUpdateControllerForTesting());

        PauseResumeWithNativeObserver resumeObserver = mCoordinator.getResumeObserverForTesting();
        assertNotNull(resumeObserver);
        resumeObserver.onResumeWithNative();
        assertNull(mCoordinator.getInAppUpdateControllerForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testInAppUpdate_deferredStartupAfterDestroy_doesNotThrowOrResurrectController() {
        initCoordinatorFields();

        mCoordinator.scheduleInAppUpdateDeferredStartup();

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mDeferredStartupHandler).addDeferredTask(runnableCaptor.capture());

        // Simulate activity destruction before deferred startup task executes.
        mCoordinator.destroyActivityForTesting();
        mCoordinator.destroyInAppUpdate();

        // Executing the deferred task must not throw NullPointerException or resurrect the
        // controller.
        runnableCaptor.getValue().run();
        assertNull(mCoordinator.getInAppUpdateControllerForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testGetOrCreateInAppUpdateController_afterDestroy_returnsNull() {
        mCoordinator.destroyActivityForTesting();
        mCoordinator.setInAppUpdateControllerForTesting(null);
        assertNull(mCoordinator.getOrCreateInAppUpdateController());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void
            testGetOrCreateInAppUpdateController_whenActivityFinishingOrDestroyed_returnsNull() {
        initCoordinatorFields();
        mCoordinator.setInAppUpdateControllerForTesting(null);

        // Active activity creates controller.
        assertNotNull(mCoordinator.getOrCreateInAppUpdateController());

        // When finishing, returns null and does not resurrect if cleared.
        mCoordinator.setInAppUpdateControllerForTesting(null);
        mActivity.finish();
        assertNull(mCoordinator.getOrCreateInAppUpdateController());
    }
}

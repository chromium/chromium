// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowProcess;

import org.chromium.base.Callback;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.policy.PolicyService;

/** Unit tests for PolicyLoadListener. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowProcess.class})
// TODO(crbug.com/40182398): Change to use paused loop. See crbug for details.
@LooperMode(LooperMode.Mode.LEGACY)
public class PolicyLoadListenerUnitTest {
    private static final String LOADED_POLICY_READY = "Policy service should be ready to read.";
    private static final String LOADED_NO_POLICY = "Policy should not exist.";
    private static final String LOADING_NOT_FINISHED =
            "Whether policy might exist should be not decided yet.";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Spy
    public OneshotSupplierImpl<PolicyService> mTestPolicyServiceSupplier =
            new OneshotSupplierImpl<>();

    @Spy public Callback<Boolean> mListener;
    @Mock public PolicyService mPolicyService;
    @Mock public FirstRunAppRestrictionInfo mTestAppRestrictionInfo;

    private PolicyService.Observer mPolicyServiceObserver;
    private Callback<Boolean> mAppRestrictionsCallback;
    private PolicyLoadListener mPolicyLoadListener;

    @Before
    public void setUp() {
        doCallback((PolicyService.Observer observer) -> mPolicyServiceObserver = observer)
                .when(mPolicyService)
                .addObserver(any());
        doCallback((Callback<Boolean> callback) -> mAppRestrictionsCallback = callback)
                .when(mTestAppRestrictionInfo)
                .getHasAppRestriction(any());

        mPolicyLoadListener =
                new PolicyLoadListener(mTestAppRestrictionInfo, mTestPolicyServiceSupplier);
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.get());

        Mockito.verify(mTestAppRestrictionInfo).getHasAppRestriction(mAppRestrictionsCallback);
        Mockito.verify(mTestPolicyServiceSupplier).onAvailable(any());
    }

    @Test
    public void testAppRestrictionNotFound() {
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.onAvailable(mListener));

        mAppRestrictionsCallback.onResult(false);
        Assert.assertFalse(LOADED_NO_POLICY, mPolicyLoadListener.get());
        Mockito.verify(mListener).onResult(false);
    }

    @Test
    public void testAppRestrictionFoundAndPolicyLoaded() {
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.onAvailable(mListener));

        mAppRestrictionsCallback.onResult(true);
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.get());
        Mockito.verify(mListener, never()).onResult(any());

        mTestPolicyServiceSupplier.set(mPolicyService);
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.get());
        Mockito.verify(mListener, never()).onResult(any());

        setPolicyServiceInitialized();
        Assert.assertTrue(LOADED_POLICY_READY, mPolicyLoadListener.get());
        Mockito.verify(mListener).onResult(true);
    }

    @Test
    public void testPolicyInitializedWithoutAppRestriction() {
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.onAvailable(mListener));

        mTestPolicyServiceSupplier.set(mPolicyService);
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.get());
        Mockito.verify(mListener, never()).onResult(any());

        setPolicyServiceInitialized();
        Assert.assertTrue(LOADED_POLICY_READY, mPolicyLoadListener.get());
        Mockito.verify(mListener).onResult(true);

        mAppRestrictionsCallback.onResult(true);
        Assert.assertTrue(
                "App restriction arrives after policy initialized should be ignored.",
                mPolicyLoadListener.get());
        Mockito.verify(mListener, never()).onResult(false);
    }

    @Test
    public void testPolicyInitializedBeforeSupplied() {
        setPolicyServiceInitialized();
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.get());

        mTestPolicyServiceSupplier.set(mPolicyService);
        Assert.assertTrue(LOADED_POLICY_READY, mPolicyLoadListener.get());
    }

    @Test
    public void testAddListenerAfterFinished() {
        mAppRestrictionsCallback.onResult(false);
        Assert.assertFalse(LOADED_NO_POLICY, mPolicyLoadListener.get());

        Assert.assertFalse(LOADED_NO_POLICY, mPolicyLoadListener.onAvailable(mListener));
        Mockito.verify(mListener).onResult(false);
    }

    @Test
    public void testDestroyAfterStart_AppRestrictions() {
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.onAvailable(mListener));

        mPolicyLoadListener.destroy();
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.get());
        Mockito.verify(mListener, never()).onResult(anyBoolean());

        // Even when no app restrictions were found, listeners will not be informed after #destroy,
        // and the state for PolicyLoadListener should stay at not finished.
        mAppRestrictionsCallback.onResult(false);
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.get());
        Mockito.verify(mListener, never()).onResult(anyBoolean());
    }

    @Test
    public void testDestroyAfterStart_PolicyInitialized() {
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.onAvailable(mListener));

        mPolicyLoadListener.destroy();
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.get());
        Mockito.verify(mListener, never()).onResult(anyBoolean());

        // Even when policy service is initialized, listeners will not be informed after #destroy,
        // and the state for PolicyLoadListener should stay at not finished.
        mTestPolicyServiceSupplier.set(mPolicyService);
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.get());
        Mockito.verify(mListener, never()).onResult(anyBoolean());
    }

    @Test
    public void testDestroyAfterStart_PolicyInitializedInterleaved() {
        Assert.assertNull(LOADING_NOT_FINISHED, mPolicyLoadListener.onAvailable(mListener));

        // OneshotSupplierImpl will post to a Handler when it runs callbacks. By pausing the main
        // looper, we temporarily stop these from being run. Otherwise Robolectric will run them
        // synchronously.
        ShadowLooper.pauseMainLooper();
        mAppRestrictionsCallback.onResult(false);
        Assert.assertFalse(LOADED_NO_POLICY, mPolicyLoadListener.get());

        // Because #destroy() is called before mListener has been notified, the notification should
        // dropped.
        mPolicyLoadListener.destroy();
        ShadowLooper.unPauseMainLooper();
        Mockito.verify(mListener, never()).onResult(anyBoolean());
    }

    private void setPolicyServiceInitialized() {
        Mockito.when(mPolicyService.isInitializationComplete()).thenReturn(true);
        if (mPolicyServiceObserver != null) mPolicyServiceObserver.onPolicyServiceInitialized();
    }
}

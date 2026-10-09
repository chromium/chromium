// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.content.pm.PackageManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.permissions.PermissionCallback;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link TtcSessionMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TTC)
public class TtcSessionMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TtcKeyedService mService;
    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;

    private PropertyModel mModel;
    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;

    @Before
    public void setUp() {
        mModel = new PropertyModel(TtcSessionProperties.ALL_KEYS);
        mProfileSupplier = ObservableSuppliers.createMonotonic();
        when(mService.isEnabled()).thenReturn(true);
        TtcKeyedServiceFactory.setForTesting(mService);
        when(mLifecycleDispatcher.getCurrentActivityState())
                .thenReturn(ActivityState.RESUMED_WITH_NATIVE);
        when(mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(true);
    }

    /** Creates a mediator and, unless {@code profileAvailable} is false, supplies the profile. */
    private TtcSessionMediator createMediator(boolean profileAvailable) {
        if (profileAvailable) mProfileSupplier.set(mProfile);
        return new TtcSessionMediator(
                mModel, mWindowAndroid, mProfileSupplier, mLifecycleDispatcher);
    }

    private TtcSessionMediator createMediator() {
        return createMediator(/* profileAvailable= */ true);
    }

    // Profile observation.

    @Test
    public void testObservesServiceOnceProfileAvailable() {
        TtcSessionMediator mediator = createMediator(/* profileAvailable= */ false);
        verify(mService, never()).addObserver(any());

        mProfileSupplier.set(mProfile);
        verify(mService).addObserver(mediator);
        verify(mLifecycleDispatcher).register(mediator);
    }

    @Test
    public void testHiddenWhenNoSessionOnCreation() {
        when(mService.isSessionActive()).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        verify(mService).addObserver(mediator);
        assertFalse(mModel.get(TtcSessionProperties.VISIBLE));
    }

    @Test
    public void testConnectingWhenSessionAlreadyActiveOnCreation() {
        when(mService.isSessionActive()).thenReturn(true);
        createMediator();

        assertTrue(mModel.get(TtcSessionProperties.VISIBLE));
        assertEquals(
                R.string.ttc_session_connecting,
                mModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TTC)
    public void testFeatureDisabledIgnoresService() {
        TtcSessionMediator mediator = createMediator();

        verify(mService, never()).addObserver(any());
        mediator.toggleSession();
        verify(mService, never()).startSession();
        assertFalse(mModel.get(TtcSessionProperties.VISIBLE));
    }

    @Test
    public void testServiceNotEnabledIgnoresService() {
        when(mService.isEnabled()).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        verify(mService, never()).addObserver(any());
        mediator.toggleSession();
        verify(mService, never()).startSession();
    }

    // Session lifecycle.

    @Test
    public void testToggleStartsSessionWhenPermissionGranted() {
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        verify(mService).startSession();
        verify(mWindowAndroid, never()).requestPermissions(any(), any());
    }

    @Test
    public void testToggleEndsActiveSession() {
        when(mService.isSessionActive()).thenReturn(true);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        verify(mService).endSession();
        verify(mService, never()).startSession();
    }

    @Test
    public void testToggleRequestsPermissionAndStartsWhenGranted() {
        when(mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        verify(mService, never()).startSession();

        resolvePermissionRequest(PackageManager.PERMISSION_GRANTED);
        verify(mService).startSession();
    }

    @Test
    public void testPermissionDeniedDoesNotStartSession() {
        when(mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        resolvePermissionRequest(PackageManager.PERMISSION_DENIED);
        verify(mService, never()).startSession();
    }

    @Test
    public void testPermissionGrantedAfterActivityStoppedDoesNotStartSession() {
        when(mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        when(mLifecycleDispatcher.getCurrentActivityState())
                .thenReturn(ActivityState.STOPPED_WITH_NATIVE);
        resolvePermissionRequest(PackageManager.PERMISSION_GRANTED);
        verify(mService, never()).startSession();
    }

    @Test
    public void testPermissionGrantedAfterDestroyDoesNotStartSession() {
        when(mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        mediator.destroy();
        resolvePermissionRequest(PackageManager.PERMISSION_GRANTED);
        verify(mService, never()).startSession();
    }

    @Test
    public void testStopWithNativeEndsSession() {
        when(mService.isSessionActive()).thenReturn(true);
        TtcSessionMediator mediator = createMediator();

        mediator.onStopWithNative();
        verify(mService).endSession();
    }

    @Test
    public void testStopWithNativeWithoutSessionIsNoOp() {
        when(mService.isSessionActive()).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.onStopWithNative();
        verify(mService, never()).endSession();
    }

    @Test
    public void testDestroyEndsSessionAndUnregisters() {
        when(mService.isSessionActive()).thenReturn(true);
        TtcSessionMediator mediator = createMediator();

        mediator.destroy();
        verify(mService).endSession();
        verify(mService).removeObserver(mediator);
        verify(mLifecycleDispatcher).unregister(mediator);
    }

    // Model updates from the service.

    @Test
    public void testSessionActiveShowsConnectingThenListening() {
        TtcSessionMediator mediator = createMediator();

        mediator.onServiceStateChanged(ServiceState.SESSION_ACTIVE);
        assertTrue(mModel.get(TtcSessionProperties.VISIBLE));
        assertEquals(
                R.string.ttc_session_connecting,
                mModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));
        assertEquals(0f, mModel.get(TtcSessionProperties.AUDIO_LEVEL), 0f);

        mediator.onSessionInitialized();
        assertEquals(
                R.string.ttc_session_listening,
                mModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));
    }

    @Test
    public void testInitializedBeforeActiveShowsListening() {
        TtcSessionMediator mediator = createMediator();

        // ConversationImpl::Start() can report initialization before the service broadcasts
        // SESSION_ACTIVE; the pill must not regress to "connecting" in that case.
        mediator.onSessionInitialized();
        mediator.onServiceStateChanged(ServiceState.SESSION_ACTIVE);

        assertTrue(mModel.get(TtcSessionProperties.VISIBLE));
        assertEquals(
                R.string.ttc_session_listening,
                mModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));
    }

    @Test
    public void testSessionInactiveHidesAndResetsForNextSession() {
        TtcSessionMediator mediator = createMediator();
        mediator.onServiceStateChanged(ServiceState.SESSION_ACTIVE);
        mediator.onSessionInitialized();

        mediator.onServiceStateChanged(ServiceState.SESSION_INACTIVE);
        assertFalse(mModel.get(TtcSessionProperties.VISIBLE));

        // A new session starts over at "connecting".
        mediator.onServiceStateChanged(ServiceState.SESSION_ACTIVE);
        assertEquals(
                R.string.ttc_session_connecting,
                mModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));
    }

    @Test
    public void testProfileIneligibleHides() {
        TtcSessionMediator mediator = createMediator();
        mediator.onServiceStateChanged(ServiceState.SESSION_ACTIVE);

        mediator.onServiceStateChanged(ServiceState.PROFILE_INELIGIBLE);
        assertFalse(mModel.get(TtcSessionProperties.VISIBLE));
    }

    @Test
    public void testAudioLevelIsClamped() {
        TtcSessionMediator mediator = createMediator();

        mediator.onAudioLevelChanged(0.5f);
        assertEquals(0.5f, mModel.get(TtcSessionProperties.AUDIO_LEVEL), 0f);
        mediator.onAudioLevelChanged(1.7f);
        assertEquals(1f, mModel.get(TtcSessionProperties.AUDIO_LEVEL), 0f);
        mediator.onAudioLevelChanged(-0.2f);
        assertEquals(0f, mModel.get(TtcSessionProperties.AUDIO_LEVEL), 0f);
    }

    @Test
    public void testFatalErrorStaysVisibleAfterSessionEnds() {
        TtcSessionMediator mediator = createMediator();
        mediator.onServiceStateChanged(ServiceState.SESSION_ACTIVE);

        // The native side ends the session right after reporting a fatal error.
        mediator.onError(/* errorCode= */ 0);
        mediator.onServiceStateChanged(ServiceState.SESSION_INACTIVE);
        assertTrue(mModel.get(TtcSessionProperties.VISIBLE));
        assertEquals(
                R.string.ttc_session_error, mModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));

        ShadowLooper.idleMainLooper(TtcSessionMediator.ERROR_DISPLAY_MS, TimeUnit.MILLISECONDS);
        assertFalse(mModel.get(TtcSessionProperties.VISIBLE));
    }

    @Test
    public void testNonFatalErrorReturnsToSessionStatus() {
        when(mService.isSessionActive()).thenReturn(true);
        TtcSessionMediator mediator = createMediator();
        mediator.onSessionInitialized();

        mediator.onError(/* errorCode= */ 0);
        assertEquals(
                R.string.ttc_session_error, mModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));

        ShadowLooper.idleMainLooper(TtcSessionMediator.ERROR_DISPLAY_MS, TimeUnit.MILLISECONDS);
        assertTrue(mModel.get(TtcSessionProperties.VISIBLE));
        assertEquals(
                R.string.ttc_session_listening,
                mModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));
    }

    @Test
    public void testNewSessionClearsError() {
        TtcSessionMediator mediator = createMediator();
        mediator.onServiceStateChanged(ServiceState.SESSION_ACTIVE);
        mediator.onError(/* errorCode= */ 0);
        mediator.onServiceStateChanged(ServiceState.SESSION_INACTIVE);

        mediator.onServiceStateChanged(ServiceState.SESSION_ACTIVE);
        assertEquals(
                R.string.ttc_session_connecting,
                mModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));

        // The pending hide must not fire against the new session.
        ShadowLooper.idleMainLooper(TtcSessionMediator.ERROR_DISPLAY_MS, TimeUnit.MILLISECONDS);
        assertTrue(mModel.get(TtcSessionProperties.VISIBLE));
    }

    @Test
    public void testDestroyCancelsPendingErrorHide() {
        TtcSessionMediator mediator = createMediator();
        mediator.onServiceStateChanged(ServiceState.SESSION_ACTIVE);
        mediator.onError(/* errorCode= */ 0);

        mediator.destroy();
        verify(mService).removeObserver(mediator);
        // Nothing should touch the model once destroyed.
        ShadowLooper.idleMainLooper(TtcSessionMediator.ERROR_DISPLAY_MS, TimeUnit.MILLISECONDS);
        assertTrue(mModel.get(TtcSessionProperties.VISIBLE));
    }

    private void resolvePermissionRequest(int grantResult) {
        ArgumentCaptor<PermissionCallback> captor =
                ArgumentCaptor.forClass(PermissionCallback.class);
        verify(mWindowAndroid)
                .requestPermissions(
                        eq(new String[] {Manifest.permission.RECORD_AUDIO}), captor.capture());
        captor.getValue()
                .onRequestPermissionsResult(
                        new String[] {Manifest.permission.RECORD_AUDIO}, new int[] {grantResult});
    }
}

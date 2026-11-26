// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageButton;

import androidx.core.view.ViewCompat;

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
import org.robolectric.annotation.Config;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.floating_pill.FloatingPillLayer;
import org.chromium.chrome.browser.ui.floating_pill.FloatingPillLayer.PillType;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link TtcSessionCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TTC)
@DisableFeatures(ChromeFeatureList.TTC_FRE)
public class TtcSessionCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TtcKeyedService mService;
    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private FloatingPillLayer mFloatingPillLayer;
    @Mock private TtcShortcutHandler.Natives mShortcutHandlerJni;
    @Captor private ArgumentCaptor<View> mPillCaptor;
    @Captor private ArgumentCaptor<TtcKeyedService.Observer> mServiceObserverCaptor;

    private Activity mActivity;
    private FrameLayout mRoot;
    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;
    private TtcSessionCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mRoot = new FrameLayout(mActivity);
        mActivity.setContentView(mRoot);

        when(mService.isEnabled()).thenReturn(true);
        when(mService.getSessionLifecycle()).thenReturn(SessionLifecycle.LIVE);
        when(mService.getWindowTracker())
                .thenReturn(new TtcSessionWindowTracker(mService::endSession));
        TtcKeyedServiceFactory.setForTesting(mService);
        TtcShortcutHandlerJni.setInstanceForTesting(mShortcutHandlerJni);
        when(mShortcutHandlerJni.init(any(), any(), any())).thenReturn(1L);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mLifecycleDispatcher.getCurrentActivityState())
                .thenReturn(ActivityState.RESUMED_WITH_NATIVE);
        mProfileSupplier = ObservableSuppliers.createMonotonic();
        mProfileSupplier.set(mProfile);
    }

    @After
    public void tearDown() {
        if (mCoordinator != null) mCoordinator.destroy();
    }

    private void createCoordinator() {
        mCoordinator =
                new TtcSessionCoordinator(
                        mRoot,
                        mFloatingPillLayer,
                        mWindowAndroid,
                        mProfileSupplier,
                        mLifecycleDispatcher);
    }

    @Test
    public void testSmallScreenUsesMobileSessionUi() {
        assertTrue(TtcSessionCoordinator.usesMobileSessionUi(mActivity));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testLargeScreenShowsVoicePlateDuringSession() {
        assertFalse(TtcSessionCoordinator.usesMobileSessionUi(mActivity));
        when(mService.isSessionActive()).thenReturn(true);
        createCoordinator();

        // Shown in the window's floating pill container, on the bottom edge of the toolbar.
        verify(mFloatingPillLayer).addPill(mPillCaptor.capture(), eq(PillType.TTC_VOICE_PLATE));
        View plate = mPillCaptor.getValue();
        assertEquals(R.id.ttc_voice_plate, plate.getId());
        assertEquals(View.VISIBLE, plate.getVisibility());

        mCoordinator.destroy();
        mCoordinator = null;
        verify(mFloatingPillLayer).removePill(plate);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testVoicePlateInFloatingPillLayerOnlyDuringSession() {
        when(mService.isSessionActive()).thenReturn(true);
        createCoordinator();
        verify(mFloatingPillLayer).addPill(mPillCaptor.capture(), eq(PillType.TTC_VOICE_PLATE));
        View plate = mPillCaptor.getValue();
        verify(mService).addObserver(mServiceObserverCaptor.capture());

        // When the session ends, the plate leaves the layer, so the layer no longer counts as a
        // visible top control; it's back for the next session.
        mServiceObserverCaptor.getValue().onServiceStateChanged(ServiceState.SESSION_INACTIVE);
        verify(mFloatingPillLayer).removePill(plate);

        mServiceObserverCaptor.getValue().onServiceStateChanged(ServiceState.SESSION_ACTIVE);
        verify(mFloatingPillLayer, times(2)).addPill(plate, PillType.TTC_VOICE_PLATE);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testVoicePlateFollowsSession() {
        when(mService.isSessionActive()).thenReturn(true);
        createCoordinator();
        verify(mFloatingPillLayer).addPill(mPillCaptor.capture(), eq(PillType.TTC_VOICE_PLATE));
        View plate = mPillCaptor.getValue();
        View waveform = plate.findViewById(R.id.ttc_voice_plate_waveform);
        ImageButton pause = plate.findViewById(R.id.ttc_voice_plate_pause_button);
        verify(mService).addObserver(mServiceObserverCaptor.capture());
        TtcKeyedService.Observer observer = mServiceObserverCaptor.getValue();

        // The live session's status is read out, and the microphone can be paused.
        assertEquals(
                mActivity.getString(R.string.ttc_session_listening),
                ViewCompat.getStateDescription(waveform));
        assertTrue(pause.isEnabled());
        pause.performClick();
        verify(mService).setMicMuted(true);

        // Muted, the button resumes.
        observer.onMicMutedChanged(true);
        assertEquals(
                mActivity.getString(R.string.ttc_voice_plate_resume),
                pause.getContentDescription());

        // Until the session is live, it can't be paused.
        observer.onSessionLifecycleChanged(SessionLifecycle.INITIALIZING);
        assertEquals(
                mActivity.getString(R.string.ttc_session_connecting),
                ViewCompat.getStateDescription(waveform));
        assertFalse(pause.isEnabled());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testLargeScreenBuildsVoicePlateOnlyOnceSessionShows() {
        createCoordinator();
        verify(mFloatingPillLayer, never()).addPill(any(), anyInt());
    }
}

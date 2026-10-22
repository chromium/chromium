// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.content.pm.PackageManager;

import androidx.test.core.app.ApplicationProvider;

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
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.permissions.PermissionCallback;

import java.lang.ref.WeakReference;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link TtcSessionMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TTC)
@DisableFeatures(ChromeFeatureList.TTC_FRE)
public class TtcSessionMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TtcKeyedService mService;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private ActivityLifecycleDispatcher mOtherLifecycleDispatcher;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private PrefService mPrefService;

    private PropertyModel mModel;
    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;

    @Before
    public void setUp() {
        mModel = new PropertyModel(TtcSessionProperties.ALL_KEYS);
        mProfileSupplier = ObservableSuppliers.createMonotonic();
        when(mService.isEnabled()).thenReturn(true);
        // A session that becomes active starts out connecting, as the native one does.
        when(mService.getSessionLifecycle()).thenReturn(SessionLifecycle.INITIALIZING);
        // Matches the real service, which owns one tracker that ends its session.
        TtcSessionWindowTracker windowTracker = new TtcSessionWindowTracker(mService::endSession);
        when(mService.getWindowTracker()).thenReturn(windowTracker);
        TtcKeyedServiceFactory.setForTesting(mService);
        when(mLifecycleDispatcher.getCurrentActivityState())
                .thenReturn(ActivityState.RESUMED_WITH_NATIVE);
        when(mOtherLifecycleDispatcher.getCurrentActivityState())
                .thenReturn(ActivityState.RESUMED_WITH_NATIVE);
        when(mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(true);
        when(mWindowAndroid.getModalDialogManager()).thenReturn(mModalDialogManager);
        when(mWindowAndroid.getContext())
                .thenReturn(new WeakReference<>(ApplicationProvider.getApplicationContext()));
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);
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

    /**
     * Creates the mediator for a second window on the same profile, sharing {@link #mService}. It
     * has its own model and lifecycle, and its profile is supplied by {@code profileSupplier}.
     */
    private TtcSessionMediator createOtherWindowMediator(
            PropertyModel model, SettableMonotonicObservableSupplier<Profile> profileSupplier) {
        if (profileSupplier.get() == null) profileSupplier.set(mProfile);
        return new TtcSessionMediator(
                model, mWindowAndroid, profileSupplier, mOtherLifecycleDispatcher);
    }

    private TtcSessionMediator createOtherWindowMediator(
            SettableMonotonicObservableSupplier<Profile> profileSupplier) {
        return createOtherWindowMediator(
                new PropertyModel(TtcSessionProperties.ALL_KEYS), profileSupplier);
    }

    private TtcSessionMediator createOtherWindowMediator() {
        return createOtherWindowMediator(ObservableSuppliers.createMonotonic());
    }

    /**
     * Simulates the native session moving to {@code lifecycle}: the service starts reporting it and
     * notifies {@code mediators}, i.e. the windows observing it.
     */
    private void setSessionLifecycle(
            @SessionLifecycle int lifecycle, TtcSessionMediator... mediators) {
        when(mService.getSessionLifecycle()).thenReturn(lifecycle);
        for (TtcSessionMediator mediator : mediators) {
            mediator.onSessionLifecycleChanged(lifecycle);
        }
    }

    /**
     * Switches the window whose profile is supplied by {@code profileSupplier} to incognito. The
     * factory returns {@link #mService} for every profile, so the off-the-record profile having no
     * service is modeled by the service not being enabled for it.
     */
    private void switchToIncognito(SettableMonotonicObservableSupplier<Profile> profileSupplier) {
        when(mService.isEnabled()).thenReturn(false);
        profileSupplier.set(mIncognitoProfile);
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
    public void testListeningWhenLiveSessionAlreadyActiveOnCreation() {
        when(mService.isSessionActive()).thenReturn(true);
        setSessionLifecycle(SessionLifecycle.LIVE);
        createMediator();

        assertTrue(mModel.get(TtcSessionProperties.VISIBLE));
        assertEquals(
                R.string.ttc_session_listening,
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
    public void testPillClickEndsSession() {
        when(mService.isSessionActive()).thenReturn(true);
        createMediator();

        mModel.get(TtcSessionProperties.ON_CLICK_LISTENER).onClick(null);
        verify(mService).endSession();
    }

    @Test
    public void testPillClickWithoutServiceIsNoOp() {
        when(mService.isEnabled()).thenReturn(false);
        createMediator();

        mModel.get(TtcSessionProperties.ON_CLICK_LISTENER).onClick(null);
        verify(mService, never()).endSession();
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
    public void testPermissionGrantedWhilePausedStartsSession() {
        when(mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        // The permission dialog pauses the activity, which is still visible behind it.
        when(mLifecycleDispatcher.getCurrentActivityState())
                .thenReturn(ActivityState.PAUSED_WITH_NATIVE);
        resolvePermissionRequest(PackageManager.PERMISSION_GRANTED);
        verify(mService).startSession();
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
    public void testDestroyEndsSessionAndUnregisters() {
        when(mService.isSessionActive()).thenReturn(true);
        TtcSessionMediator mediator = createMediator();

        mediator.destroy();
        verify(mService).endSession();
        verify(mService).removeObserver(mediator);
        verify(mLifecycleDispatcher).unregister(mediator);
    }

    @Test
    public void testSwitchingToIncognitoInOnlyWindowEndsSession() {
        when(mService.isSessionActive()).thenReturn(true);
        createMediator();

        switchToIncognito(mProfileSupplier);
        verify(mService).endSession();
        assertFalse(mModel.get(TtcSessionProperties.VISIBLE));
    }

    @Test
    public void testSameServiceAgainDoesNotEndSession() {
        when(mService.isSessionActive()).thenReturn(true);
        createMediator();

        // A different profile object that maps to the same service.
        mProfileSupplier.set(mock(Profile.class));
        verify(mService, never()).endSession();
        assertTrue(mModel.get(TtcSessionProperties.VISIBLE));
    }

    // First run experience.

    /** Returns the model of the FRE dialog shown via {@link #mModalDialogManager}. */
    private PropertyModel getShownFreDialog() {
        ArgumentCaptor<PropertyModel> captor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(captor.capture(), eq(ModalDialogManager.ModalDialogType.APP));
        return captor.getValue();
    }

    /** Simulates the user tapping {@code buttonType} on {@code dialog}, which dismisses it. */
    private void clickFreDialogButton(
            PropertyModel dialog, @ModalDialogProperties.ButtonType int buttonType) {
        ModalDialogProperties.Controller controller = dialog.get(ModalDialogProperties.CONTROLLER);
        @DialogDismissalCause
        int cause =
                buttonType == ModalDialogProperties.ButtonType.POSITIVE
                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED;
        controller.onClick(dialog, buttonType);
        verify(mModalDialogManager).dismissDialog(dialog, cause);
        controller.onDismiss(dialog, cause);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TTC_FRE)
    public void testFreDialogShownWhenFreNotCompleted() {
        when(mPrefService.getBoolean(Pref.TTC_FRE_COMPLETED)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        verify(mService, never()).startSession();
        PropertyModel dialog = getShownFreDialog();
        assertEquals("Placeholder", dialog.get(ModalDialogProperties.TITLE));
        assertEquals("Enable TTC?", dialog.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TTC_FRE)
    public void testFreDialogAcceptedMarksFreCompletedAndStartsSession() {
        when(mPrefService.getBoolean(Pref.TTC_FRE_COMPLETED)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        clickFreDialogButton(getShownFreDialog(), ModalDialogProperties.ButtonType.POSITIVE);
        verify(mPrefService).setBoolean(Pref.TTC_FRE_COMPLETED, true);
        verify(mService).startSession();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TTC_FRE)
    public void testFreDialogAcceptedRequestsPermissionBeforeStarting() {
        when(mPrefService.getBoolean(Pref.TTC_FRE_COMPLETED)).thenReturn(false);
        when(mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        clickFreDialogButton(getShownFreDialog(), ModalDialogProperties.ButtonType.POSITIVE);
        verify(mPrefService).setBoolean(Pref.TTC_FRE_COMPLETED, true);
        verify(mService, never()).startSession();

        resolvePermissionRequest(PackageManager.PERMISSION_GRANTED);
        verify(mService).startSession();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TTC_FRE)
    public void testFreDialogCancelledLeavesFreIncompleteAndDoesNotStart() {
        when(mPrefService.getBoolean(Pref.TTC_FRE_COMPLETED)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        clickFreDialogButton(getShownFreDialog(), ModalDialogProperties.ButtonType.NEGATIVE);
        verify(mPrefService, never()).setBoolean(eq(Pref.TTC_FRE_COMPLETED), anyBoolean());
        // The permission is already granted in setUp(), so a session would have started if the
        // dialog had been skipped or accepted.
        verify(mWindowAndroid, never()).requestPermissions(any(), any());
        verify(mService, never()).startSession();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TTC_FRE)
    public void testFreDialogDismissedLeavesFreIncompleteAndDoesNotStart() {
        when(mPrefService.getBoolean(Pref.TTC_FRE_COMPLETED)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        PropertyModel dialog = getShownFreDialog();
        dialog.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(dialog, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        verify(mPrefService, never()).setBoolean(eq(Pref.TTC_FRE_COMPLETED), anyBoolean());
        // The permission is already granted in setUp(), so a session would have started if the
        // dialog had been skipped or accepted.
        verify(mWindowAndroid, never()).requestPermissions(any(), any());
        verify(mService, never()).startSession();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TTC_FRE)
    public void testFreDialogAcceptedWhileActivityStoppedDoesNotStartSession() {
        when(mPrefService.getBoolean(Pref.TTC_FRE_COMPLETED)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        when(mLifecycleDispatcher.getCurrentActivityState())
                .thenReturn(ActivityState.STOPPED_WITH_NATIVE);
        clickFreDialogButton(getShownFreDialog(), ModalDialogProperties.ButtonType.POSITIVE);
        // The FRE is still marked completed; only the session start is skipped.
        verify(mPrefService).setBoolean(Pref.TTC_FRE_COMPLETED, true);
        verify(mService, never()).startSession();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TTC_FRE)
    public void testDestroyDismissesFreDialog() {
        when(mPrefService.getBoolean(Pref.TTC_FRE_COMPLETED)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        PropertyModel dialog = getShownFreDialog();
        mediator.destroy();
        verify(mModalDialogManager).dismissDialog(dialog, DialogDismissalCause.DISMISSED_BY_NATIVE);

        // The presenter reports the dismissal back; nothing is recorded or started.
        dialog.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(dialog, DialogDismissalCause.DISMISSED_BY_NATIVE);
        verify(mPrefService, never()).setBoolean(eq(Pref.TTC_FRE_COMPLETED), anyBoolean());
        verify(mService, never()).startSession();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TTC_FRE)
    public void testSwitchingToIncognitoDismissesFreDialog() {
        when(mPrefService.getBoolean(Pref.TTC_FRE_COMPLETED)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        PropertyModel dialog = getShownFreDialog();
        switchToIncognito(mProfileSupplier);
        verify(mModalDialogManager).dismissDialog(dialog, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TTC_FRE)
    public void testFreDialogAcceptedAfterProfileSwitchDoesNotMarkFreCompleted() {
        when(mPrefService.getBoolean(Pref.TTC_FRE_COMPLETED)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        PropertyModel dialog = getShownFreDialog();
        switchToIncognito(mProfileSupplier);
        // Even if the dialog somehow outlived the switch, accepting it must not mark the FRE
        // completed for the new profile.
        clickFreDialogButton(dialog, ModalDialogProperties.ButtonType.POSITIVE);
        verify(mPrefService, never()).setBoolean(eq(Pref.TTC_FRE_COMPLETED), anyBoolean());
        verify(mUserPrefsJni, never()).get(mIncognitoProfile);
        verify(mService, never()).startSession();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TTC_FRE)
    public void testToggleWhileFreDialogShowingDoesNotShowAnother() {
        when(mPrefService.getBoolean(Pref.TTC_FRE_COMPLETED)).thenReturn(false);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        mediator.toggleSession();
        verify(mModalDialogManager).showDialog(any(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TTC_FRE)
    public void testFreDialogNotShownWhenFreCompleted() {
        when(mPrefService.getBoolean(Pref.TTC_FRE_COMPLETED)).thenReturn(true);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
        verify(mService).startSession();
    }

    @Test
    public void testFreDisabledSkipsDialogAndResetsFreCompleted() {
        when(mPrefService.getBoolean(Pref.TTC_FRE_COMPLETED)).thenReturn(true);
        TtcSessionMediator mediator = createMediator();

        mediator.toggleSession();
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
        verify(mService).startSession();
        // Testing stopgap until there is a settings UI: lets the dialog show again when the FRE
        // is re-enabled.
        verify(mPrefService).clearPref(Pref.TTC_FRE_COMPLETED);
    }

    // Multiple windows sharing the profile's session.

    @Test
    public void testStopInOneWindowKeepsSessionWhileAnotherIsVisible() {
        when(mService.isSessionActive()).thenReturn(true);
        TtcSessionMediator mediator = createMediator();
        TtcSessionMediator otherMediator = createOtherWindowMediator();

        otherMediator.onStopWithNative();
        verify(mService, never()).endSession();

        mediator.onStopWithNative();
        verify(mService).endSession();
    }

    @Test
    public void testDestroyInOneWindowKeepsSessionWhileAnotherIsVisible() {
        when(mService.isSessionActive()).thenReturn(true);
        TtcSessionMediator mediator = createMediator();
        TtcSessionMediator otherMediator = createOtherWindowMediator();

        // Closing a window stops it and then destroys it.
        otherMediator.onStopWithNative();
        otherMediator.destroy();
        verify(mService, never()).endSession();
        assertTrue(mModel.get(TtcSessionProperties.VISIBLE));

        mediator.onStopWithNative();
        verify(mService).endSession();
    }

    @Test
    public void testSwitchingToIncognitoInOneWindowKeepsSessionWhileAnotherIsVisible() {
        when(mService.isSessionActive()).thenReturn(true);
        TtcSessionMediator mediator = createMediator();
        SettableMonotonicObservableSupplier<Profile> otherProfileSupplier =
                ObservableSuppliers.createMonotonic();
        createOtherWindowMediator(otherProfileSupplier);

        switchToIncognito(otherProfileSupplier);
        verify(mService, never()).endSession();
        assertTrue(mModel.get(TtcSessionProperties.VISIBLE));

        mediator.onStopWithNative();
        verify(mService).endSession();
    }

    @Test
    public void testRestartedWindowKeepsSessionAlive() {
        when(mService.isSessionActive()).thenReturn(true);
        TtcSessionMediator mediator = createMediator();
        TtcSessionMediator otherMediator = createOtherWindowMediator();

        mediator.onStopWithNative();
        mediator.onStartWithNative();
        otherMediator.onStopWithNative();
        verify(mService, never()).endSession();

        mediator.onStopWithNative();
        verify(mService).endSession();
    }

    @Test
    public void testWindowStartedBeforeProfileAvailableKeepsSessionAlive() {
        when(mService.isSessionActive()).thenReturn(true);
        // The window is already resumed when its profile becomes available.
        TtcSessionMediator mediator = createMediator(/* profileAvailable= */ false);
        mProfileSupplier.set(mProfile);
        TtcSessionMediator otherMediator = createOtherWindowMediator();

        otherMediator.onStopWithNative();
        verify(mService, never()).endSession();

        mediator.onStopWithNative();
        verify(mService).endSession();
    }

    @Test
    public void testPausedWindowKeepsSessionAlive() {
        when(mService.isSessionActive()).thenReturn(true);
        // A paused window, e.g. an unfocused one in multi-window mode, is still visible.
        when(mOtherLifecycleDispatcher.getCurrentActivityState())
                .thenReturn(ActivityState.PAUSED_WITH_NATIVE);
        TtcSessionMediator mediator = createMediator();
        TtcSessionMediator otherMediator = createOtherWindowMediator();

        mediator.onStopWithNative();
        verify(mService, never()).endSession();

        otherMediator.onStopWithNative();
        verify(mService).endSession();
    }

    @Test
    public void testStoppedWindowDoesNotKeepSessionAlive() {
        when(mService.isSessionActive()).thenReturn(true);
        when(mOtherLifecycleDispatcher.getCurrentActivityState())
                .thenReturn(ActivityState.STOPPED_WITH_NATIVE);
        TtcSessionMediator mediator = createMediator();
        createOtherWindowMediator();

        mediator.onStopWithNative();
        verify(mService).endSession();
    }

    @Test
    public void testWindowOpenedAfterSessionLiveShowsListening() {
        when(mService.isSessionActive()).thenReturn(true);
        TtcSessionMediator mediator = createMediator();
        setSessionLifecycle(SessionLifecycle.LIVE, mediator);

        // A window opened once the session is live must not show "connecting".
        PropertyModel otherModel = new PropertyModel(TtcSessionProperties.ALL_KEYS);
        createOtherWindowMediator(otherModel, ObservableSuppliers.createMonotonic());

        assertTrue(otherModel.get(TtcSessionProperties.VISIBLE));
        assertEquals(
                R.string.ttc_session_listening,
                otherModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));
        assertEquals(
                R.string.ttc_session_listening,
                mModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));
    }

    @Test
    public void testWindowSwitchingToSessionProfileAfterLiveShowsListening() {
        when(mService.isSessionActive()).thenReturn(true);
        TtcSessionMediator mediator = createMediator();
        PropertyModel otherModel = new PropertyModel(TtcSessionProperties.ALL_KEYS);
        SettableMonotonicObservableSupplier<Profile> otherProfileSupplier =
                ObservableSuppliers.createMonotonic();
        createOtherWindowMediator(otherModel, otherProfileSupplier);
        switchToIncognito(otherProfileSupplier);
        assertFalse(otherModel.get(TtcSessionProperties.VISIBLE));

        // The session goes live while the other window isn't observing it.
        setSessionLifecycle(SessionLifecycle.LIVE, mediator);

        when(mService.isEnabled()).thenReturn(true);
        otherProfileSupplier.set(mProfile);
        assertTrue(otherModel.get(TtcSessionProperties.VISIBLE));
        assertEquals(
                R.string.ttc_session_listening,
                otherModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));
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

        setSessionLifecycle(SessionLifecycle.LIVE, mediator);
        assertEquals(
                R.string.ttc_session_listening,
                mModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));
    }

    @Test
    public void testFinishedKeepsStatusUntilSessionEnds() {
        TtcSessionMediator mediator = createMediator();
        mediator.onServiceStateChanged(ServiceState.SESSION_ACTIVE);
        setSessionLifecycle(SessionLifecycle.LIVE, mediator);

        // The session finishes a task before the service reports it inactive.
        setSessionLifecycle(SessionLifecycle.FINISHED, mediator);
        assertTrue(mModel.get(TtcSessionProperties.VISIBLE));
        assertEquals(
                R.string.ttc_session_listening,
                mModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));

        mediator.onServiceStateChanged(ServiceState.SESSION_INACTIVE);
        assertFalse(mModel.get(TtcSessionProperties.VISIBLE));
    }

    @Test
    public void testLifecycleChangeDoesNotReplaceError() {
        TtcSessionMediator mediator = createMediator();
        mediator.onServiceStateChanged(ServiceState.SESSION_ACTIVE);

        mediator.onError(/* errorCode= */ 0);
        setSessionLifecycle(SessionLifecycle.LIVE, mediator);
        assertEquals(
                R.string.ttc_session_error, mModel.get(TtcSessionProperties.STATUS_TEXT_RES_ID));
    }

    @Test
    public void testSessionInactiveHidesAndResetsForNextSession() {
        TtcSessionMediator mediator = createMediator();
        mediator.onServiceStateChanged(ServiceState.SESSION_ACTIVE);
        setSessionLifecycle(SessionLifecycle.LIVE, mediator);

        setSessionLifecycle(SessionLifecycle.FINISHED, mediator);
        mediator.onServiceStateChanged(ServiceState.SESSION_INACTIVE);
        assertFalse(mModel.get(TtcSessionProperties.VISIBLE));

        // A new session starts over at "connecting".
        setSessionLifecycle(SessionLifecycle.INITIALIZING, mediator);
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
        setSessionLifecycle(SessionLifecycle.LIVE, mediator);

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

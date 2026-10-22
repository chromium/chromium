// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Handler;

import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Runs TTC sessions for one activity and keeps the session pill's {@link PropertyModel} in sync
 * with the {@link TtcKeyedService}: follows the active profile, starts and ends sessions (showing
 * the first run dialog and asking for the microphone permission first), and mirrors the service's
 * state into the model.
 *
 * <p>The session belongs to the profile and is shared by every window. This mediator reports its
 * window's visibility to the profile's {@link TtcSessionWindowTracker}, which ends the session once
 * no window is visible, rather than ending it when this window alone stops.
 */
@NullMarked
class TtcSessionMediator implements TtcKeyedService.Observer, StartStopWithNativeObserver {
    /**
     * How long the pill keeps showing an error after the session ends. The native side ends the
     * session as soon as it reports a fatal error, so without this the error would never be seen.
     */
    static final long ERROR_DISPLAY_MS = 3000;

    private final PropertyModel mModel;
    private final WindowAndroid mWindowAndroid;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final Callback<Profile> mProfileObserver = this::onProfileAvailable;
    private final Handler mHandler = ThreadUtils.getUiThreadHandler();
    private final Runnable mHideAfterErrorTask = this::hideAfterError;

    // The current profile, or null until one is available or after destroy().
    private @Nullable Profile mProfile;

    // The service for the current profile, or null when no session can run against it (the
    // feature is off, or the profile has no service / isn't enabled for it).
    private @Nullable TtcKeyedService mService;

    // Whether the pill is showing an error and should stay visible until the hide task runs,
    // even if the session has already ended.
    private boolean mShowingError;

    // The first run dialog while it is showing, so it can be dismissed if the service goes away.
    private @Nullable PropertyModel mFreDialog;

    /**
     * @param model The pill's model; {@link TtcSessionProperties#VISIBLE} is false until a session
     *     starts.
     * @param windowAndroid The activity's window, used for the first run dialog and the microphone
     *     permission.
     * @param profileSupplier Supplies the profile sessions run against.
     * @param lifecycleDispatcher Used to report this window's visibility to the profile's {@link
     *     TtcSessionWindowTracker}, which ends the session once no window is visible, since
     *     microphone capture can't continue in the background.
     */
    TtcSessionMediator(
            PropertyModel model,
            WindowAndroid windowAndroid,
            MonotonicObservableSupplier<Profile> profileSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mModel = model;
        mWindowAndroid = windowAndroid;
        mProfileSupplier = profileSupplier;
        mLifecycleDispatcher = lifecycleDispatcher;
        // Tapping the pill ends the session. TtcKeyedService#endSession() is a no-op when no
        // session is active.
        mModel.set(
                TtcSessionProperties.ON_CLICK_LISTENER,
                v -> {
                    if (mService != null) mService.endSession();
                });
        mLifecycleDispatcher.register(this);
        mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);
    }

    void destroy() {
        mLifecycleDispatcher.unregister(this);
        mProfileSupplier.removeObserver(mProfileObserver);
        // Only ends the session if no other window is still showing it.
        detachFromService();
        mProfile = null;
    }

    /**
     * Ends the session if one is active, otherwise starts one, first showing the first run dialog
     * if the user hasn't completed it yet, and then asking for the microphone permission if it
     * hasn't been granted.
     */
    void toggleSession() {
        if (mService == null) return;
        if (mService.isSessionActive()) {
            mService.endSession();
            return;
        }
        if (needsFre()) {
            showFreDialog();
            return;
        }
        startSessionWithPermission();
    }

    /**
     * Whether the first run dialog must be shown before a session can start: the FRE is enabled and
     * the user hasn't completed it yet.
     */
    private boolean needsFre() {
        PrefService prefs = getPrefs();
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TTC_FRE)) {
            // TODO(crbug.com/575529047): for testing only. Until there is a settings UI to reset
            // the FRE, starting TTC with the FRE disabled resets the pref so that the dialog shows
            // again once the FRE is re-enabled.
            prefs.clearPref(Pref.TTC_FRE_COMPLETED);
            return false;
        }
        return !prefs.getBoolean(Pref.TTC_FRE_COMPLETED);
    }

    private void showFreDialog() {
        if (mFreDialog != null) return;
        // Every Chrome activity has a dialog manager, and the activity (the window's context) is
        // alive while this mediator is, since the coordinator is destroyed with it.
        ModalDialogManager modalDialogManager =
                assumeNonNull(mWindowAndroid.getModalDialogManager());
        Context context = assumeNonNull(mWindowAndroid.getContext().get());
        mFreDialog =
                TtcFreDialog.show(
                        modalDialogManager, context.getResources(), this::onFreDialogDismissed);
    }

    private void onFreDialogDismissed(boolean accepted) {
        mFreDialog = null;
        // The dialog is asynchronous. detachFromService() dismisses it if the service goes away,
        // but check anyway so that the FRE is only marked completed for a profile that can use it.
        if (!accepted || mService == null) return;
        getPrefs().setBoolean(Pref.TTC_FRE_COMPLETED, true);
        startSessionWithPermission();
    }

    /**
     * Dismisses the first run dialog if it is showing; {@link #onFreDialogDismissed} sees false.
     */
    private void dismissFreDialog() {
        if (mFreDialog == null) return;
        ModalDialogManager modalDialogManager = mWindowAndroid.getModalDialogManager();
        if (modalDialogManager != null) {
            modalDialogManager.dismissDialog(mFreDialog, DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
        mFreDialog = null;
    }

    /** Returns the current profile's prefs. Only valid while {@link #mService} is non-null. */
    private PrefService getPrefs() {
        // mService is only ever set for a profile that has been supplied to onProfileAvailable().
        assert mProfile != null;
        return UserPrefs.get(mProfile);
    }

    /**
     * Starts a session, prompting for the microphone permission first if it hasn't been granted.
     */
    private void startSessionWithPermission() {
        if (mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO)) {
            startSession();
            return;
        }
        mWindowAndroid.requestPermissions(
                new String[] {Manifest.permission.RECORD_AUDIO},
                (permissions, grantResults) -> {
                    if (grantResults.length > 0
                            && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                        startSession();
                    }
                });
    }

    // StartStopWithNativeObserver:

    @Override
    public void onStartWithNative() {
        if (mService != null) mService.getWindowTracker().addVisibleWindow(this);
    }

    @Override
    public void onStopWithNative() {
        // Only ends the session if no other window is still showing it.
        if (mService != null) mService.getWindowTracker().removeVisibleWindow(this);
    }

    // TtcKeyedService.Observer:

    @Override
    public void onServiceStateChanged(@ServiceState int state) {
        boolean active = state == ServiceState.SESSION_ACTIVE;
        if (active) {
            clearError();
            mModel.set(TtcSessionProperties.STATUS_TEXT_RES_ID, getSessionStatusTextResId());
            mModel.set(TtcSessionProperties.AUDIO_LEVEL, 0f);
        } else {
            // Leave the error on screen; hideAfterError() hides the pill.
            if (mShowingError) return;
        }
        mModel.set(TtcSessionProperties.VISIBLE, active);
    }

    @Override
    public void onSessionLifecycleChanged(@SessionLifecycle int lifecycle) {
        // FINISHED is reported just before SESSION_INACTIVE hides the pill; keep the current text
        // (or error) until then rather than flashing "connecting".
        if (mShowingError || lifecycle == SessionLifecycle.FINISHED) return;
        mModel.set(TtcSessionProperties.STATUS_TEXT_RES_ID, getStatusTextResId(lifecycle));
    }

    @Override
    public void onAudioLevelChanged(float level) {
        mModel.set(TtcSessionProperties.AUDIO_LEVEL, Math.max(0f, Math.min(1f, level)));
    }

    @Override
    public void onError(int errorCode) {
        mShowingError = true;
        mModel.set(TtcSessionProperties.STATUS_TEXT_RES_ID, R.string.ttc_session_error);
        mModel.set(TtcSessionProperties.AUDIO_LEVEL, 0f);
        mHandler.removeCallbacks(mHideAfterErrorTask);
        mHandler.postDelayed(mHideAfterErrorTask, ERROR_DISPLAY_MS);
    }

    private void onProfileAvailable(Profile profile) {
        mProfile = profile;
        TtcKeyedService service = TtcSessionCoordinator.getServiceIfAvailable(profile);
        if (service == mService) return;
        // This window stops hosting the previous profile's session. That only ends the session if
        // no other window is still showing it.
        detachFromService();
        mService = service;
        if (mService == null) {
            mModel.set(TtcSessionProperties.VISIBLE, false);
            return;
        }
        mService.addObserver(this);
        onServiceStateChanged(
                mService.isSessionActive()
                        ? ServiceState.SESSION_ACTIVE
                        : ServiceState.SESSION_INACTIVE);
        // The window may have started before the profile was available.
        if (isActivityVisible()) mService.getWindowTracker().addVisibleWindow(this);
    }

    /**
     * Stops observing the current service and removes this window from its {@link
     * TtcSessionWindowTracker}, which ends the session if no other window is still showing it.
     */
    private void detachFromService() {
        clearError();
        // A pending first run dialog is for the old profile's service.
        dismissFreDialog();
        if (mService == null) return;
        mService.getWindowTracker().removeVisibleWindow(this);
        mService.removeObserver(this);
        mService = null;
    }

    private void startSession() {
        // Reached from the asynchronous first run and permission prompts; by the time they resolve
        // the mediator may have been destroyed (mService is null) or the activity stopped, in which
        // case a session would run with no UI and nothing to end it.
        if (mService == null || mService.isSessionActive() || !isActivityVisible()) return;
        mService.startSession();
    }

    /**
     * Whether the activity is on screen. A paused activity is still visible, e.g. an unfocused
     * window in multi-window mode or a window behind the microphone permission dialog.
     */
    private boolean isActivityVisible() {
        @ActivityState int state = mLifecycleDispatcher.getCurrentActivityState();
        return state == ActivityState.STARTED_WITH_NATIVE
                || state == ActivityState.RESUMED_WITH_NATIVE
                || state == ActivityState.PAUSED_WITH_NATIVE;
    }

    private void hideAfterError() {
        mShowingError = false;
        if (mService != null && mService.isSessionActive()) {
            // A non-fatal error: the session is still running, so go back to its status.
            mModel.set(TtcSessionProperties.STATUS_TEXT_RES_ID, getSessionStatusTextResId());
            return;
        }
        mModel.set(TtcSessionProperties.VISIBLE, false);
    }

    /**
     * Returns the status to show for the active session. Read from the native session rather than
     * tracked per window, so that every window agrees, including one that started showing the
     * session after it went live.
     */
    private @StringRes int getSessionStatusTextResId() {
        return getStatusTextResId(
                mService != null ? mService.getSessionLifecycle() : SessionLifecycle.FINISHED);
    }

    private static @StringRes int getStatusTextResId(@SessionLifecycle int lifecycle) {
        return lifecycle == SessionLifecycle.LIVE
                ? R.string.ttc_session_listening
                : R.string.ttc_session_connecting;
    }

    private void clearError() {
        if (!mShowingError) return;
        mShowingError = false;
        mHandler.removeCallbacks(mHideAfterErrorTask);
    }
}

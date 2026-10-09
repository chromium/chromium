// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Handler;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Runs TTC sessions for one activity and keeps the session pill's {@link PropertyModel} in sync
 * with the {@link TtcKeyedService}: follows the active profile, starts and ends sessions (prompting
 * for the microphone permission first), ends the session when the activity is stopped, and mirrors
 * the service's state into the model.
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

    // The service for the current profile, or null when no session can run against it (the
    // feature is off, or the profile has no service / isn't enabled for it).
    private @Nullable TtcKeyedService mService;

    // Whether onSessionInitialized() has fired for the current session. The native side can
    // signal initialization before it broadcasts SESSION_ACTIVE (ConversationImpl::Start()
    // calls OnSessionInitialized() synchronously), so this is tracked independently of the
    // service state.
    private boolean mSessionInitialized;

    // Whether the pill is showing an error and should stay visible until the hide task runs,
    // even if the session has already ended.
    private boolean mShowingError;

    /**
     * @param model The pill's model; {@link TtcSessionProperties#VISIBLE} is false until a session
     *     starts.
     * @param windowAndroid The activity's window, used for the microphone permission.
     * @param profileSupplier Supplies the profile sessions run against.
     * @param lifecycleDispatcher Used to end the session when the activity is stopped, since
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
        mLifecycleDispatcher.register(this);
        mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);
    }

    void destroy() {
        endSession();
        mLifecycleDispatcher.unregister(this);
        mProfileSupplier.removeObserver(mProfileObserver);
        detachFromService();
    }

    /**
     * Ends the session if one is active, otherwise starts one, prompting for the microphone
     * permission first if it hasn't been granted.
     */
    void toggleSession() {
        if (mService == null) return;
        if (mService.isSessionActive()) {
            mService.endSession();
            return;
        }
        if (mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO)) {
            startSession();
            return;
        }
        mWindowAndroid.requestPermissions(
                new String[] {Manifest.permission.RECORD_AUDIO},
                (permissions, grantResults) -> {
                    // The prompt is asynchronous; by the time it resolves the mediator may have
                    // been destroyed (mService is null) or the activity stopped, in which case a
                    // session would run with no UI and nothing to end it.
                    if (grantResults.length > 0
                            && grantResults[0] == PackageManager.PERMISSION_GRANTED
                            && isActivityStarted()) {
                        startSession();
                    }
                });
    }

    /** Ends the session if one is active. */
    void endSession() {
        if (mService == null || !mService.isSessionActive()) return;
        mService.endSession();
    }

    // StartStopWithNativeObserver:

    @Override
    public void onStartWithNative() {}

    @Override
    public void onStopWithNative() {
        endSession();
    }

    // TtcKeyedService.Observer:

    @Override
    public void onServiceStateChanged(@ServiceState int state) {
        boolean active = state == ServiceState.SESSION_ACTIVE;
        if (active) {
            clearError();
            mModel.set(
                    TtcSessionProperties.STATUS_TEXT_RES_ID,
                    mSessionInitialized
                            ? R.string.ttc_session_listening
                            : R.string.ttc_session_connecting);
            mModel.set(TtcSessionProperties.AUDIO_LEVEL, 0f);
        } else {
            mSessionInitialized = false;
            // Leave the error on screen; hideAfterError() hides the pill.
            if (mShowingError) return;
        }
        mModel.set(TtcSessionProperties.VISIBLE, active);
    }

    @Override
    public void onSessionInitialized() {
        mSessionInitialized = true;
        if (mShowingError) return;
        mModel.set(TtcSessionProperties.STATUS_TEXT_RES_ID, R.string.ttc_session_listening);
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
        // Stop anything running against the previous profile before switching.
        endSession();
        detachFromService();
        mService = TtcSessionCoordinator.getServiceIfAvailable(profile);
        if (mService == null) {
            mModel.set(TtcSessionProperties.VISIBLE, false);
            return;
        }
        mService.addObserver(this);
        onServiceStateChanged(
                mService.isSessionActive()
                        ? ServiceState.SESSION_ACTIVE
                        : ServiceState.SESSION_INACTIVE);
    }

    private void detachFromService() {
        clearError();
        if (mService == null) return;
        mService.removeObserver(this);
        mService = null;
    }

    private void startSession() {
        if (mService == null || mService.isSessionActive()) return;
        mService.startSession();
    }

    private boolean isActivityStarted() {
        @ActivityState int state = mLifecycleDispatcher.getCurrentActivityState();
        return state == ActivityState.STARTED_WITH_NATIVE
                || state == ActivityState.RESUMED_WITH_NATIVE;
    }

    private void hideAfterError() {
        mShowingError = false;
        if (mService != null && mService.isSessionActive()) {
            // A non-fatal error: the session is still running, so go back to its status.
            mModel.set(
                    TtcSessionProperties.STATUS_TEXT_RES_ID,
                    mSessionInitialized
                            ? R.string.ttc_session_listening
                            : R.string.ttc_session_connecting);
            return;
        }
        mModel.set(TtcSessionProperties.VISIBLE, false);
    }

    private void clearError() {
        if (!mShowingError) return;
        mShowingError = false;
        mHandler.removeCallbacks(mHideAfterErrorTask);
    }
}

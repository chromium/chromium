// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import android.content.Context;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.floating_pill.FloatingPillLayer;
import org.chromium.chrome.browser.ui.floating_pill.FloatingPillLayer.PillType;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Owns the TTC session UI for one activity. The session itself (start / end, first run experience,
 * microphone permission, following the active profile) lives in {@link TtcSessionMediator} and is
 * form-factor agnostic; this class wires the mediator's model to a view, built the first time a
 * session shows. On small screens that view is a floating pill shown over the content area, and on
 * large screens it's the {@link TtcVoicePlateView} on the bottom edge of the toolbar, see {@link
 * #usesMobileSessionUi(Context)}. The mediator also keeps the window's {@link TtcEntryPoint}, the
 * toolbar button where the toolbar offers one, in sync.
 */
@NullMarked
public class TtcSessionCoordinator {
    private final View mAnchorView;
    private final FloatingPillLayer mFloatingPillLayer;
    private final TtcSessionMediator mMediator;

    private @Nullable TtcSessionPillView mPillView;
    private @Nullable TtcVoicePlateView mVoicePlate;
    private boolean mVoicePlateInLayer;

    /**
     * Returns the service to run sessions against for {@code profile}, or null when the TTC entry
     * point shouldn't be offered: the feature is disabled, the profile has no service (i.e. it's
     * off-the-record), or the service isn't enabled for the profile.
     */
    public static @Nullable TtcKeyedService getServiceIfAvailable(@Nullable Profile profile) {
        if (profile == null || !ChromeFeatureList.isEnabled(ChromeFeatureList.TTC)) return null;
        // The factory only builds the service for regular profiles; checked here too so that tests
        // with a fake factory see the same.
        if (profile.isOffTheRecord()) return null;
        TtcKeyedService service = TtcKeyedServiceFactory.getForProfile(profile);
        return service != null && service.isEnabled() ? service : null;
    }

    /**
     * Whether this window gets the small-screen session UI, the floating pill. Large screens, the
     * ones with the tablet toolbar, get the voice plate instead. Every window offers TTC in the app
     * menu; large screens can also have it as their toolbar shortcut.
     */
    static boolean usesMobileSessionUi(Context context) {
        return !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    /**
     * @param anchorView The activity's content area; the pill is pinned to its bottom-end corner.
     * @param floatingPillLayer Shows the voice plate in the window's floating pill container, on
     *     the bottom edge of the toolbar, on large screens.
     * @param windowAndroid The activity's window, used for the first run dialog and the microphone
     *     permission.
     * @param profileSupplier Supplies the profile sessions run against.
     * @param lifecycleDispatcher Used to report this window's visibility to the profile's {@link
     *     TtcSessionWindowTracker}, which ends the session once no window is visible, since
     *     microphone capture can't continue in the background.
     */
    public TtcSessionCoordinator(
            View anchorView,
            FloatingPillLayer floatingPillLayer,
            WindowAndroid windowAndroid,
            MonotonicObservableSupplier<Profile> profileSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mAnchorView = anchorView;
        mFloatingPillLayer = floatingPillLayer;
        PropertyModel model =
                new PropertyModel.Builder(TtcSessionProperties.ALL_KEYS)
                        .with(TtcSessionProperties.VISIBLE, false)
                        .with(
                                TtcSessionProperties.STATUS_TEXT_RES_ID,
                                R.string.ttc_session_connecting)
                        .with(TtcSessionProperties.AUDIO_LEVEL, 0f)
                        .with(TtcSessionProperties.MIC_MUTED, false)
                        .with(TtcSessionProperties.PAUSE_ENABLED, false)
                        .with(TtcSessionProperties.WAVEFORM_MODE, TtcWaveformView.Mode.IDLE)
                        .build();
        // The view is only built the first time a session makes the model visible. Attached
        // before the mediator, which may show a session that is already running.
        if (usesMobileSessionUi(anchorView.getContext())) {
            LazyConstructionPropertyMcp.create(
                    model,
                    TtcSessionProperties.VISIBLE,
                    new PillViewProvider(),
                    TtcSessionViewBinder::bind);
        } else {
            LazyConstructionPropertyMcp.create(
                    model,
                    TtcSessionProperties.VISIBLE,
                    new VoicePlateViewProvider(),
                    TtcVoicePlateViewBinder::bind);
            // After the view is built, so it's there when the session first shows.
            model.addObserver(
                    (source, key) -> {
                        if (key == TtcSessionProperties.VISIBLE) {
                            updateVoicePlateInLayer(model.get(TtcSessionProperties.VISIBLE));
                        }
                    });
        }
        mMediator =
                new TtcSessionMediator(
                        model,
                        windowAndroid,
                        profileSupplier,
                        lifecycleDispatcher,
                        TtcEntryPoint.from(windowAndroid));
    }

    public void destroy() {
        mMediator.destroy();
        if (mPillView != null) {
            mPillView.destroy();
            mPillView = null;
        }
        updateVoicePlateInLayer(false);
        mVoicePlate = null;
    }

    /**
     * Keeps the voice plate in the window's floating pill layer only while it shows, so the layer
     * only counts as a visible top control then.
     */
    private void updateVoicePlateInLayer(boolean show) {
        if (mVoicePlate == null || show == mVoicePlateInLayer) return;
        mVoicePlateInLayer = show;
        if (show) {
            mFloatingPillLayer.addPill(mVoicePlate.getView(), PillType.TTC_VOICE_PLATE);
        } else {
            mFloatingPillLayer.removePill(mVoicePlate.getView());
        }
    }

    /**
     * Ends the session if one is active, otherwise starts one, first showing the first run dialog
     * if the user hasn't completed it yet, and then asking for the microphone permission if it
     * hasn't been granted.
     */
    public void toggleSession() {
        mMediator.toggleSession();
    }

    /** Builds the pill on demand for {@link LazyConstructionPropertyMcp}. */
    private class PillViewProvider implements ViewProvider<TtcSessionPillView> {
        private final List<Callback<TtcSessionPillView>> mCallbacks = new ArrayList<>();

        @Override
        public void inflate() {
            if (mPillView != null) return;
            mPillView = new TtcSessionPillView(mAnchorView);
            for (Callback<TtcSessionPillView> callback : mCallbacks) {
                callback.onResult(mPillView);
            }
            mCallbacks.clear();
        }

        @Override
        public void whenLoaded(Callback<TtcSessionPillView> callback) {
            if (mPillView != null) {
                callback.onResult(mPillView);
            } else {
                mCallbacks.add(callback);
            }
        }
    }

    /** Builds the voice plate on demand for {@link LazyConstructionPropertyMcp}. */
    private class VoicePlateViewProvider implements ViewProvider<TtcVoicePlateView> {
        private final List<Callback<TtcVoicePlateView>> mCallbacks = new ArrayList<>();

        @Override
        public void inflate() {
            if (mVoicePlate != null) return;
            mVoicePlate = TtcVoicePlateView.create(mAnchorView.getContext());
            for (Callback<TtcVoicePlateView> callback : mCallbacks) {
                callback.onResult(mVoicePlate);
            }
            mCallbacks.clear();
        }

        @Override
        public void whenLoaded(Callback<TtcVoicePlateView> callback) {
            if (mVoicePlate != null) {
                callback.onResult(mVoicePlate);
            } else {
                mCallbacks.add(callback);
            }
        }
    }
}

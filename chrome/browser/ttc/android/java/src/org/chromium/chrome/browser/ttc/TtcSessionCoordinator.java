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
import org.chromium.ui.ViewProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Owns the TTC session UI for one activity. The session itself (start / end, microphone permission,
 * following the active profile) lives in {@link TtcSessionMediator} and is form-factor agnostic;
 * this class wires the mediator's model to a view. On small screens that view is a floating pill
 * shown over the content area while a session is live, see {@link #usesMobileSessionUi(Context)}.
 */
@NullMarked
public class TtcSessionCoordinator {
    private final View mAnchorView;
    private final TtcSessionMediator mMediator;

    private @Nullable TtcSessionPillView mPillView;

    /**
     * Returns the service to run sessions against for {@code profile}, or null when the TTC entry
     * point shouldn't be offered: the feature is disabled, the profile has no service (i.e. it's
     * off-the-record), or the service isn't enabled for the profile.
     */
    public static @Nullable TtcKeyedService getServiceIfAvailable(@Nullable Profile profile) {
        if (profile == null || !ChromeFeatureList.isEnabled(ChromeFeatureList.TTC)) return null;
        TtcKeyedService service = TtcKeyedServiceFactory.getForProfile(profile);
        return service != null && service.isEnabled() ? service : null;
    }

    /**
     * Whether this window gets the small-screen session UI: the app-menu entry point and the
     * floating pill. Both the view and the entry point branch on this, so flipping it is the single
     * switch for a different large-screen presentation.
     *
     * <p>TODO(b/571214745): the large-screen / desktop Android presentation hasn't been designed
     * yet, so every form factor gets the small-screen UI for now.
     */
    public static boolean usesMobileSessionUi(Context context) {
        return true;
    }

    /**
     * @param anchorView The activity's content area; the pill is pinned to its bottom-end corner.
     * @param windowAndroid The activity's window, used for the microphone permission.
     * @param profileSupplier Supplies the profile sessions run against.
     * @param lifecycleDispatcher Used to end the session when the activity is stopped, since
     *     microphone capture can't continue in the background.
     */
    public TtcSessionCoordinator(
            View anchorView,
            WindowAndroid windowAndroid,
            MonotonicObservableSupplier<Profile> profileSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mAnchorView = anchorView;
        PropertyModel model =
                new PropertyModel.Builder(TtcSessionProperties.ALL_KEYS)
                        .with(TtcSessionProperties.VISIBLE, false)
                        .with(
                                TtcSessionProperties.STATUS_TEXT_RES_ID,
                                R.string.ttc_session_connecting)
                        .with(TtcSessionProperties.AUDIO_LEVEL, 0f)
                        .with(TtcSessionProperties.ON_CLICK_LISTENER, v -> endSession())
                        .build();
        if (usesMobileSessionUi(anchorView.getContext())) {
            // The pill is only built the first time a session makes the model visible. Attached
            // before the mediator, which may show a session that is already running.
            LazyConstructionPropertyMcp.create(
                    model,
                    TtcSessionProperties.VISIBLE,
                    new PillViewProvider(),
                    TtcSessionViewBinder::bind);
        }
        mMediator =
                new TtcSessionMediator(model, windowAndroid, profileSupplier, lifecycleDispatcher);
    }

    public void destroy() {
        mMediator.destroy();
        if (mPillView != null) {
            mPillView.destroy();
            mPillView = null;
        }
    }

    /**
     * Ends the session if one is active, otherwise starts one, prompting for the microphone
     * permission first if it hasn't been granted.
     */
    public void toggleSession() {
        mMediator.toggleSession();
    }

    private void endSession() {
        mMediator.endSession();
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
}

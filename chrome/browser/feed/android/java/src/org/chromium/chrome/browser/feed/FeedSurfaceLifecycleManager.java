// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Manages the lifecycle of a feed surface represented by {@link FeedSurfaceCoordinator} associated
 * with an Activity.
 */
public class FeedSurfaceLifecycleManager implements ApplicationStatus.ActivityStateListener {
    /** The different states that the Stream can be in its lifecycle. */
    // TODO(chili): Clean up unused SHOWN/HIDDEN states.
    @IntDef({
        SurfaceState.NOT_SPECIFIED,
        SurfaceState.CREATED,
        SurfaceState.SHOWN,
        SurfaceState.ACTIVE,
        SurfaceState.INACTIVE,
        SurfaceState.HIDDEN,
        SurfaceState.DESTROYED
    })
    @Retention(RetentionPolicy.SOURCE)
    protected @interface SurfaceState {
        int NOT_SPECIFIED = -1;
        int CREATED = 0;
        int SHOWN = 1;
        int ACTIVE = 2;
        int INACTIVE = 3;
        int HIDDEN = 4;
        int DESTROYED = 5;
    }

    /** The {@link FeedSurfaceCoordinator} that this class updates. */
    protected final SurfaceCoordinator mCoordinator;

    /** The current state the feed is in its lifecycle. */
    protected @SurfaceState int mSurfaceState = SurfaceState.NOT_SPECIFIED;

    /** The {@link Activity} that {@link #mCoordinator} is attached to. */
    private final Activity mActivity;

    /**
     * @param activity The {@link Activity} that the {@link FeedSurfaceCoordinator} is attached to.
     * @param coordinator The coordinator managing the feed surface.
     */
    public FeedSurfaceLifecycleManager(Activity activity, SurfaceCoordinator coordinator) {
        mActivity = activity;
        mCoordinator = coordinator;
    }

    /** Notifies the feed that it should show if it can. */
    protected void start() {
        mSurfaceState = SurfaceState.CREATED;
        show();

        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        switch (newState) {
            case ActivityState.STARTED:
            case ActivityState.RESUMED:
                mCoordinator.onActivityResumed();
                show();
                break;
            case ActivityState.STOPPED:
                hide();
                break;
            case ActivityState.DESTROYED:
                destroy();
                break;
            case ActivityState.PAUSED:
                mCoordinator.onActivityPaused();
                break;
            case ActivityState.CREATED:
            default:
                assert false : "Unhandled activity state change: " + newState;
        }
    }

    /** @return Whether the {@link FeedSurfaceCoordinator} can be shown. */
    protected boolean canShow() {
        final int state = ApplicationStatus.getStateForActivity(mActivity);
        return (mSurfaceState == SurfaceState.CREATED || mSurfaceState == SurfaceState.HIDDEN)
                && (state == ActivityState.STARTED || state == ActivityState.RESUMED);
    }

    /** Calls {@link FeedSurfaceCoordinator#onSurfaceOpened()} ()}. */
    protected void show() {
        if (!canShow()) return;

        mSurfaceState = SurfaceState.SHOWN;

        mCoordinator.restoreInstanceState(restoreInstanceState());
        mCoordinator.onSurfaceOpened();
    }

    /** Calls {@link FeedSurfaceCoordinator#onSurfaceClosed()} ()}. */
    protected void hide() {
        if (mSurfaceState == SurfaceState.HIDDEN
                || mSurfaceState == SurfaceState.CREATED
                || mSurfaceState == SurfaceState.DESTROYED) {
            return;
        }

        // Make sure the feed is inactive before setting it to hidden state.
        mSurfaceState = SurfaceState.HIDDEN;
        // Save instance state as the feed begins to hide. This matches the activity lifecycle
        // that instance state is saved as the activity begins to stop.
        saveInstanceState();
        mCoordinator.onSurfaceClosed();
    }

    /** Clears any dependencies. The coordinator will be destroyed by its owner. */
    protected void destroy() {
        if (mSurfaceState == SurfaceState.DESTROYED) return;

        // Make sure the feed is hidden before setting it to destroyed state.
        hide();
        mSurfaceState = SurfaceState.DESTROYED;
        ApplicationStatus.unregisterActivityStateListener(this);
    }

    /** Save the feed instance state if necessary. */
    protected void saveInstanceState() {}

    /**
     * @return The saved feed instance state, or null if it is not previously
     *         saved.
     */
    protected @Nullable String restoreInstanceState() {
        return null;
    }
}

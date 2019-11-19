// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import com.google.android.libraries.feed.api.client.stream.Stream;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Manages the lifecycle of a {@link Stream} associated with an Activity.
 */
public class StreamLifecycleManager implements ApplicationStatus.ActivityStateListener {
    /** The different states that the Stream can be in its lifecycle. */
    @IntDef({StreamState.NOT_SPECIFIED, StreamState.CREATED, StreamState.SHOWN, StreamState.ACTIVE,
            StreamState.INACTIVE, StreamState.HIDDEN, StreamState.DESTROYED})
    @Retention(RetentionPolicy.SOURCE)
    protected @interface StreamState {
        int NOT_SPECIFIED = -1;
        int CREATED = 0;
        int SHOWN = 1;
        int ACTIVE = 2;
        int INACTIVE = 3;
        int HIDDEN = 4;
        int DESTROYED = 5;
    }

    /** The {@link Stream} that this class manages. */
    protected final Stream mStream;

    /** The current state the Stream is in its lifecycle. */
    protected @StreamState int mStreamState = StreamState.NOT_SPECIFIED;

    /** The {@link Activity} that {@link #mStream} is attached to. */
    private final Activity mActivity;

    /**
     * @param stream The {@link Stream} that this class manages.
     * @param activity The {@link Activity} that the {@link Stream} is attached to.
     */
    public StreamLifecycleManager(Stream stream, Activity activity) {
        mStream = stream;
        mActivity = activity;
    }

    protected void start() {
        mStreamState = StreamState.CREATED;
        mStream.onCreate(restoreInstanceState());
        show();
        activate();

        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        switch (newState) {
            case ActivityState.STARTED:
                show();
                break;
            case ActivityState.RESUMED:
                activate();
                break;
            case ActivityState.PAUSED:
                deactivate();
                break;
            case ActivityState.STOPPED:
                hide();
                break;
            case ActivityState.DESTROYED:
                destroy();
                break;
            case ActivityState.CREATED:
            default:
                assert false : "Unhandled activity state change: " + newState;
        }
    }

    /** @return Whether the {@link Stream} can be shown. */
    protected boolean canShow() {
        final int state = ApplicationStatus.getStateForActivity(mActivity);
        return (mStreamState == StreamState.CREATED || mStreamState == StreamState.HIDDEN)
                && (state == ActivityState.STARTED || state == ActivityState.RESUMED);
    }

    /** Calls {@link Stream#onShow()}. */
    protected void show() {
        if (!canShow()) return;

        mStreamState = StreamState.SHOWN;
        mStream.onShow();
    }

    /** @return Whether the {@link Stream} can be activated. */
    protected boolean canActivate() {
        return (mStreamState == StreamState.SHOWN || mStreamState == StreamState.INACTIVE)
                && ApplicationStatus.getStateForActivity(mActivity) == ActivityState.RESUMED;
    }

    /** Calls {@link Stream#onActive()}. */
    protected void activate() {
        // Make sure the Stream can be shown and is set shown before setting it to active state.
        show();
        if (!canActivate()) return;

        mStreamState = StreamState.ACTIVE;
        mStream.onActive();
    }

    /** Calls {@link Stream#onInactive()}. */
    protected void deactivate() {
        if (mStreamState != StreamState.ACTIVE) return;

        mStreamState = StreamState.INACTIVE;
        mStream.onInactive();
    }

    /** Calls {@link Stream#onHide()}. */
    protected void hide() {
        if (mStreamState == StreamState.HIDDEN || mStreamState == StreamState.CREATED
                || mStreamState == StreamState.DESTROYED) {
            return;
        }

        // Make sure the Stream is inactive before setting it to hidden state.
        deactivate();
        mStreamState = StreamState.HIDDEN;
        // Save instance state as the Stream begins to hide. This matches the activity lifecycle
        // that instance state is saved as the activity begins to stop.
        saveInstanceState();
        mStream.onHide();
    }

    /**
     * Clears any dependencies and calls {@link Stream#onDestroy()} when this class is not needed
     * anymore.
     */
    protected void destroy() {
        if (mStreamState == StreamState.DESTROYED) return;

        // Make sure the Stream is hidden before setting it to destroyed state.
        hide();
        mStreamState = StreamState.DESTROYED;
        ApplicationStatus.unregisterActivityStateListener(this);
        mStream.onDestroy();
    }

    /** Save the Stream instance state if necessary. */
    protected void saveInstanceState() {}

    /**
     * @return The saved Stream instance state, or null if it is not previously
     *         saved.
     */
    @Nullable
    protected String restoreInstanceState() {
        return null;
    }
}

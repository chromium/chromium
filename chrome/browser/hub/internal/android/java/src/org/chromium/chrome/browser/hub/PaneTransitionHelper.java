// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.NonNull;

import org.chromium.base.ThreadUtils;

import java.util.LinkedList;
import java.util.Optional;
import java.util.Queue;

/**
 * Coordinates transitions between pane {@link LoadHint}s. Transitions are scheduled so as to
 * minimize jank if notifying of a {@link LoadHint} results in significant work for a Pane. This
 * class should only be interacted with on the UI thread.
 */
public class PaneTransitionHelper {
    /** Information about a transition. */
    private static class TransitionData {
        /**
         * @param paneId The {@link PaneId} of the Pane to update.
         * @param loadHint The {@link loadHint} to use.
         */
        TransitionData(@PaneId int paneId, @LoadHint int loadHint) {
            this.paneId = paneId;
            this.loadHint = loadHint;
        }

        /** The {@link PaneId} to update. */
        public final @PaneId int paneId;

        /** The {@link LoadHint} to use. */
        public final @LoadHint int loadHint;
    }

    private final Queue<TransitionData> mTransitions = new LinkedList<>();
    private final PaneLookup mPaneLookup;

    private boolean mIsRunning;
    private boolean mIsDestroyed;

    /**
     * @param paneLookup The {@link PaneLookup} to operate on.
     */
    public PaneTransitionHelper(@NonNull PaneLookup paneLookup) {
        ThreadUtils.assertOnUiThread();
        mPaneLookup = paneLookup;
    }

    /** Destroys and stops any transitions. */
    public void destroy() {
        assert !mIsDestroyed;
        mIsDestroyed = true;
    }

    /**
     * Processes a transition immediately. This removes any existing scheduled transition for {@link
     * PaneId}.
     *
     * @param paneId The {@link PaneId} of the pane to transition.
     * @param loadHint The {@link LoadHint} to set on the pane.
     */
    public void processTransition(@PaneId int paneId, @LoadHint int loadHint) {
        removeTransition(paneId);
        processTransitionInternal(paneId, loadHint);
    }

    /**
     * Queue a transition to happen later on the UI thread. This posts a task to process the next
     * transition if one is not already posted. If there is an existing transition queued for {@link
     * PaneId} it will be replaced unless it is for the same {@link LoadHint}.
     *
     * @param paneId The {@link PaneId} of the pane to transition.
     * @param loadHint The {@link LoadHint} to set on the pane.
     */
    public void queueTransition(@PaneId int paneId, @LoadHint int loadHint) {
        ThreadUtils.assertOnUiThread();
        Optional<TransitionData> transition = findTransitionForPaneId(paneId);
        if (transition.isPresent()) {
            if (transition.get().loadHint == loadHint) return;

            mTransitions.remove(transition.get());
        }

        mTransitions.add(new TransitionData(paneId, loadHint));

        if (!mIsRunning) {
            mIsRunning = true;
            ThreadUtils.postOnUiThread(this::processNextTransition);
        }
    }

    /**
     * Removes a transition from the deferred transitions if there is one.
     *
     * @param paneId The {@link PaneId} of the pane to remove.
     */
    public void removeTransition(@PaneId int paneId) {
        ThreadUtils.assertOnUiThread();
        Optional<TransitionData> transition = findTransitionForPaneId(paneId);
        if (transition.isPresent()) {
            mTransitions.remove(transition.get());
        }
    }

    private Optional<TransitionData> findTransitionForPaneId(@PaneId int paneId) {
        return mTransitions.stream().filter(data -> data.paneId == paneId).findFirst();
    }

    private void processNextTransition() {
        ThreadUtils.assertOnUiThread();
        if (mIsDestroyed) return;

        TransitionData transition = mTransitions.poll();
        if (transition != null) {
            processTransitionInternal(transition.paneId, transition.loadHint);
            ThreadUtils.postOnUiThread(this::processNextTransition);
        } else {
            mIsRunning = false;
        }
    }

    private void processTransitionInternal(@PaneId int paneId, @LoadHint int hint) {
        Pane pane = mPaneLookup.getPaneForId(paneId);
        if (pane != null) {
            pane.notifyLoadHint(hint);
        }
    }
}

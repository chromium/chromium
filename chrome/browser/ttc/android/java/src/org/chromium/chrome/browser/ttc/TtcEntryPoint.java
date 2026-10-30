// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import org.chromium.base.ObserverList;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/**
 * Window-scoped state of the TTC toolbar button, shared between the {@link TtcSessionCoordinator}
 * that publishes it and the toolbar that shows it. Either side can be created first, so the state
 * lives on the window rather than on either of them. The toolbar decides which form factors show
 * the button.
 */
@NullMarked
public class TtcEntryPoint {
    /** Observes changes to the entry point's state. */
    public interface Observer {
        /** Called when {@link #shouldShow()} or {@link #isActive()} changed. */
        void onEntryPointStateChanged();
    }

    private static final UnownedUserDataKey<TtcEntryPoint> KEY = new UnownedUserDataKey<>();

    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private @Nullable Runnable mToggleHandler;
    private boolean mShouldShow;
    private boolean mActive;

    /** Returns the entry point of {@code window}, creating it on first use. */
    public static TtcEntryPoint from(WindowAndroid window) {
        UnownedUserDataHost host = window.getUnownedUserDataHost();
        TtcEntryPoint entryPoint = KEY.retrieveDataFromHost(host);
        if (entryPoint == null) {
            entryPoint = new TtcEntryPoint();
            KEY.attachToHost(host, entryPoint);
        }
        return entryPoint;
    }

    /** Whether the toolbar should show the TTC button. */
    public boolean shouldShow() {
        return mShouldShow && mToggleHandler != null;
    }

    /** Whether a session is in progress, which the button reflects by being highlighted. */
    public boolean isActive() {
        return mActive;
    }

    /** Starts a session if none is active, otherwise ends it. */
    public void toggleSession() {
        if (mToggleHandler != null) mToggleHandler.run();
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    void setToggleHandler(@Nullable Runnable toggleHandler) {
        boolean wasShown = shouldShow();
        mToggleHandler = toggleHandler;
        if (wasShown != shouldShow()) notifyObservers();
    }

    void setState(boolean shouldShow, boolean active) {
        if (mShouldShow == shouldShow && mActive == active) return;
        mShouldShow = shouldShow;
        mActive = active;
        notifyObservers();
    }

    private void notifyObservers() {
        for (Observer observer : mObservers) {
            observer.onEntryPointStateChanged();
        }
    }
}

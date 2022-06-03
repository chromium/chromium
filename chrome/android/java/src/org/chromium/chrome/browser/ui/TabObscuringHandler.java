// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import org.chromium.base.ObserverList;
import org.chromium.ui.util.TokenHolder;

/**
 * Passes around the ability to set a view that is obscuring all tabs.
 */
public class TabObscuringHandler {
    /**
     * Interface for the observers of the tab-obscuring state change.
     */
    public interface Observer {
        /**
         * @param isObscured {@code true} if the observer is obscured by another view.
         */
        void updateObscured(boolean isObscured);
    }

    /** A mechanism for distributing unique tokens to users of this system. */
    private final TokenHolder mTokenHolder;

    private final ObserverList<Observer> mVisibilityObservers = new ObserverList<>();

    /** Default constructor */
    public TabObscuringHandler() {
        mTokenHolder = new TokenHolder(this::notifyUpdate);
    }

    /**
     * Notify the system that there is a feature obscuring all visible tabs for accessibility. As
     * long as this set is nonempty, all tabs should be hidden from the accessibility tree.
     *
     * @return A token to hold while the feature is obscuring all tabs. This token is required to
     *         un-obscure the tabs.
     */
    public int obscureAllTabs() {
        return mTokenHolder.acquireToken();
    }

    /**
     * Remove a feature that previously obscured the content of all tabs.
     *
     * @param token The unique token that identified the feature (acquired in
     *              {@link #obscureAllTabs()}.
     */
    public void unobscureAllTabs(int token) {
        assert token != TokenHolder.INVALID_TOKEN;
        mTokenHolder.releaseToken(token);
    }

    /** @return Whether or not any features obscure all tabs. */
    public boolean areAllTabsObscured() {
        return mTokenHolder.hasTokens();
    }

    /**
     * Add {@link Observer} object.
     * @param observer Observer object monitoring tab visibility.
     */
    public void addObserver(Observer observer) {
        mVisibilityObservers.addObserver(observer);
    }

    /**
     * Remove {@link Observer} object.
     * @param observer Observer object monitoring tab visibility.
     */
    public void removeObserver(Observer observer) {
        mVisibilityObservers.removeObserver(observer);
    }

    /**
     * Notify all the observers of the visibility update.
     */
    private void notifyUpdate() {
        for (Observer observer : mVisibilityObservers) {
            observer.updateObscured(mTokenHolder.hasTokens());
        }
    }
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.ObserverList;
import org.chromium.ui.util.TokenHolder;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Passes around the ability to set a view that is obscuring all tabs and optionally also the
 * toolbar.
 */
public class TabObscuringHandler {
    @Retention(RetentionPolicy.SOURCE)
    public @interface Target {
        // Specifies that only the web content is covered or obscured.
        int TAB_CONTENT = 1;
        // Specifies that the whole screen including the top toolbar container is covered or
        // obscured.
        int ALL_TABS_AND_TOOLBAR = 2;
    }

    /** Represents a view that obscured a tab. */
    public static final class Token {
        private final @Target int mTarget;
        private final int mToken;

        private Token(@Target int target, int token) {
            assert token != TokenHolder.INVALID_TOKEN;
            this.mTarget = target;
            this.mToken = token;
        }
    }

    /** Interface for the observers of the tab-obscuring state change. */
    public interface Observer {
        /**
         * @param obscureTabContent {@code true} if tabs are obscured by another view.
         * @param obscureToolbar {@code true} if the top toolbar is obscured by another view.
         */
        void updateObscured(boolean obscureTabContent, boolean obscureToolbar);
    }

    /** A mechanism for distributing unique tokens to users of this system. */
    private final TokenHolder mTabContentTokenHolder;

    private final TokenHolder mAllTabsAndToolbarTokenHolder;

    private final ObserverList<Observer> mVisibilityObservers = new ObserverList<>();

    /** Default constructor */
    public TabObscuringHandler() {
        mTabContentTokenHolder = new TokenHolder(this::notifyUpdate);
        mAllTabsAndToolbarTokenHolder = new TokenHolder(this::notifyUpdate);
    }

    /**
     * Notify the system that there is a feature obscuring all visible tabs for accessibility. As
     * long as this set is nonempty, all tabs should be hidden from the accessibility tree.
     *
     * @return A token to hold while the feature is obscuring all tabs. This token is required to
     *         un-obscure the tabs.
     */
    public Token obscure(@Target int target) {
        int token = TokenHolder.INVALID_TOKEN;
        switch (target) {
            case Target.TAB_CONTENT:
                token = mTabContentTokenHolder.acquireToken();
                break;
            case Target.ALL_TABS_AND_TOOLBAR:
                token = mAllTabsAndToolbarTokenHolder.acquireToken();
                break;
        }
        return new Token(target, token);
    }

    /**
     * Remove a feature that previously obscured the content of all tabs.
     *
     * @param token The unique token that identified the feature (acquired in
     *              {@link #obscure(int)} ()}.
     */
    public void unobscure(Token token) {
        assert token != null;
        switch (token.mTarget) {
            case Target.TAB_CONTENT:
                mTabContentTokenHolder.releaseToken(token.mToken);
                break;
            case Target.ALL_TABS_AND_TOOLBAR:
                mAllTabsAndToolbarTokenHolder.releaseToken(token.mToken);
                break;
        }
    }

    /** @return Whether or not any features obscure all tabs. */
    public boolean isTabContentObscured() {
        return mTabContentTokenHolder.hasTokens() || mAllTabsAndToolbarTokenHolder.hasTokens();
    }

    /** @return Whether or not any features is obscuring the toolbar. */
    public boolean isToolbarObscured() {
        return mAllTabsAndToolbarTokenHolder.hasTokens();
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

    /** Notify all the observers of the visibility update. */
    private void notifyUpdate() {
        for (Observer observer : mVisibilityObservers) {
            observer.updateObscured(isTabContentObscured(), isToolbarObscured());
        }
    }
}

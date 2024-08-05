// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/**
 * A provider for PasswordManagerHandler implementations, handling the choice of the proper one
 * (production vs. testing), its lifetime and multiple observers.
 *
 * <p>This class is used by the code responsible for Chrome's passwords settings. There can only be
 * one instance of Chrome's passwords settings opened at a time (although more clients of
 * PasswordManagerHandler can live as nested settings pages).
 */
public class PasswordManagerHandlerProvider implements PasswordListObserver, Destroyable {
    private static ProfileKeyedMap<PasswordManagerHandlerProvider> sProfileMap;

    /** Return the {@link PasswordManagerHandlerProvider} for the given {@link Profile}. */
    public static PasswordManagerHandlerProvider getForProfile(Profile profile) {
        if (sProfileMap == null) {
            sProfileMap =
                    ProfileKeyedMap.createMapOfDestroyables(
                            ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
        }
        return sProfileMap.getForProfile(profile, PasswordManagerHandlerProvider::new);
    }

    private final Profile mProfile;

    // The production implementation of PasswordManagerHandler is |sPasswordUIView|, instantiated on
    // demand. Tests might want to override that by providing a fake implementation through
    // setPasswordManagerHandlerForTest, which is then kept in |mTestPasswordManagerHandler|.
    private PasswordUIView mPasswordUIView;
    private PasswordManagerHandler mTestPasswordManagerHandler;

    // This class is itself a PasswordListObserver, listening directly to a PasswordManagerHandler
    // implementation. But it also keeps a list of other observers, to which it forwards the events.
    private final ObserverList<PasswordListObserver> mObservers =
            new ObserverList<PasswordListObserver>();

    private PasswordManagerHandlerProvider(Profile profile) {
        mProfile = profile;
    }

    /**
     * Sets a testing implementation of PasswordManagerHandler to be used. It overrides the
     * production one even if it exists. The caller needs to ensure that |this| becomes an observer
     * of |passwordManagerHandler|. Also, this must not be called when there are already some
     * observers in |mObservers|, because of special handling of the production implementation of
     * PasswordManagerHandler on removing the last observer.
     */
    public void setPasswordManagerHandlerForTest(PasswordManagerHandler passwordManagerHandler) {
        ThreadUtils.assertOnUiThread();
        assert mObservers.isEmpty();
        mTestPasswordManagerHandler = passwordManagerHandler;
    }

    /**
     * Resets the testing implementation of PasswordManagerHandler, clears all observers and ensures
     * that the view is cleaned up properly.
     */
    public void resetPasswordManagerHandlerForTest() {
        ThreadUtils.assertOnUiThread();
        mObservers.clear();
        mTestPasswordManagerHandler = null;
        if (mPasswordUIView != null) {
            mPasswordUIView.destroy();
            mPasswordUIView = null;
        }
    }

    @Override
    public void destroy() {
        if (mPasswordUIView != null) {
            mPasswordUIView.destroy();
            mPasswordUIView = null;
        }
    }

    /**
     * A convenience function to choose between the production and test PasswordManagerHandler
     * implementation.
     */
    public PasswordManagerHandler getPasswordManagerHandler() {
        ThreadUtils.assertOnUiThread();
        if (mTestPasswordManagerHandler != null) return mTestPasswordManagerHandler;
        return mPasswordUIView;
    }

    /**
     * This method creates the production implementation of PasswordManagerHandler and saves it into
     * mPasswordUIView.
     */
    private void createPasswordManagerHandler() {
        ThreadUtils.assertOnUiThread();
        assert mPasswordUIView == null;
        mPasswordUIView = new PasswordUIView(this, mProfile);
    }

    /** Starts forwarding events from the PasswordManagerHandler implementation to |observer|. */
    public void addObserver(PasswordListObserver observer) {
        ThreadUtils.assertOnUiThread();
        if (getPasswordManagerHandler() == null) createPasswordManagerHandler();
        mObservers.addObserver(observer);
    }

    public void removeObserver(PasswordListObserver observer) {
        ThreadUtils.assertOnUiThread();
        mObservers.removeObserver(observer);
        // If this was the last observer of the production implementation of PasswordManagerHandler,
        // call destroy on it to close the connection to the native C++ code.
        if (mObservers.isEmpty() && mTestPasswordManagerHandler == null) {
            mPasswordUIView.destroy();
            mPasswordUIView = null;
        }
    }

    @Override
    public void passwordListAvailable(int count) {
        ThreadUtils.assertOnUiThread();
        for (PasswordListObserver observer : mObservers) {
            observer.passwordListAvailable(count);
        }
    }

    @Override
    public void passwordExceptionListAvailable(int count) {
        ThreadUtils.assertOnUiThread();
        for (PasswordListObserver observer : mObservers) {
            observer.passwordExceptionListAvailable(count);
        }
    }
}

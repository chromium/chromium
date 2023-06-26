// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;

/**
 * A provider for PasswordManagerHandler implementations, handling the choice of the proper one
 * (production vs. testing), its lifetime and multiple observers.
 *
 * This class is used by the code responsible for Chrome's passwords settings. There can only be one
 * instance of Chrome's passwords settings opened at a time (although more clients of
 * PasswordManagerHandler can live as nested settings pages). Therefore, the provider can be just
 * one. However, it cannot be just a collection of static data members and methods, because the
 * managed PasswordManagerHandler instances need to refer to it as an observer. For that reason, the
 * provider is a singleton.
 */
public class PasswordManagerHandlerProvider implements PasswordListObserver {
    private static final class LazyHolder {
        private static final PasswordManagerHandlerProvider INSTANCE =
                new PasswordManagerHandlerProvider();
    }

    /** Private constructor, use GetInstance() instead. */
    private PasswordManagerHandlerProvider() {}

    public static PasswordManagerHandlerProvider getInstance() {
        return LazyHolder.INSTANCE;
    }

    // The production implementation of PasswordManagerHandler is |sPasswordUIView|, instantiated on
    // demand. Tests might want to override that by providing a fake implementation through
    // setPasswordManagerHandlerForTest, which is then kept in |mTestPasswordManagerHandler|.
    private PasswordUIView mPasswordUIView;
    private PasswordManagerHandler mTestPasswordManagerHandler;

    // This class is itself a PasswordListObserver, listening directly to a PasswordManagerHandler
    // implementation. But it also keeps a list of other observers, to which it forwards the events.
    private final ObserverList<PasswordListObserver> mObservers =
            new ObserverList<PasswordListObserver>();

    /**
     * Sets a testing implementation of PasswordManagerHandler to be used. It overrides the
     * production one even if it exists. The caller needs to ensure that |this| becomes an observer
     * of |passwordManagerHandler|. Also, this must not be called when there are already some
     * observers in |mObservers|, because of special handling of the production implementation of
     * PasswordManagerHandler on removing the last observer.
     */
    @VisibleForTesting
    public void setPasswordManagerHandlerForTest(PasswordManagerHandler passwordManagerHandler) {
        ThreadUtils.assertOnUiThread();
        assert mObservers.isEmpty();
        mTestPasswordManagerHandler = passwordManagerHandler;
    }

    /**
     * Resets the testing implementation of PasswordManagerHandler, clears all observers and ensures
     * that the view is cleaned up properly.
     */
    @VisibleForTesting
    public void resetPasswordManagerHandlerForTest() {
        ThreadUtils.assertOnUiThread();
        mObservers.clear();
        mTestPasswordManagerHandler = null;
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
        mPasswordUIView = new PasswordUIView(this);
    }

    /**
     * Starts forwarding events from the PasswordManagerHandler implementation to |observer|.
     */
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

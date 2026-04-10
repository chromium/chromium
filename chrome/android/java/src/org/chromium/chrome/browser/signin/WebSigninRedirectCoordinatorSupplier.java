// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.WindowAndroid;

/**
 * A class that manages the supplier and UnownedUserData for a {@link WebSigninRedirectCoordinator}.
 */
@NullMarked
public class WebSigninRedirectCoordinatorSupplier {
    private static final UnownedUserDataKey<
                    SettableMonotonicObservableSupplier<WebSigninRedirectCoordinator>>
            KEY = new UnownedUserDataKey<>();

    /**
     * Retrieves an {@link MonotonicObservableSupplier} from the given host.
     *
     * <p>TODO(crbug.com/496831850): Attach to the UnownedUserDataHost associated with this tab
     * instead of using the window.
     */
    public static WebSigninRedirectCoordinator getOrCreateCoordinatorFrom(
            WindowAndroid windowAndroid) {
        SettableMonotonicObservableSupplier<WebSigninRedirectCoordinator> supplier =
                KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());

        assert supplier != null;

        WebSigninRedirectCoordinator val = supplier.get();
        if (val == null) {
            val = new WebSigninRedirectCoordinator();
            supplier.set(val);
        }
        assert val != null;
        return val;
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     * @param supplier The supplier to attach.
     */
    public static void attach(
            UnownedUserDataHost host,
            SettableMonotonicObservableSupplier<WebSigninRedirectCoordinator> supplier) {
        KEY.attachToHost(host, supplier);
    }

    /** Detach from all hosts. */
    public static void destroy(
            SettableMonotonicObservableSupplier<WebSigninRedirectCoordinator> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    private WebSigninRedirectCoordinatorSupplier() {}
}

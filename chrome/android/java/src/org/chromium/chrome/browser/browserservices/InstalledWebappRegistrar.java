// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;

import android.content.Context;

import dagger.Lazy;

import org.chromium.chrome.browser.browserservices.permissiondelegation.PermissionUpdater;
import org.chromium.components.embedder_support.util.Origin;

import java.util.HashSet;
import java.util.Set;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * Records in all the appropriate places that an installed webapp (TWA or WebAPK) has successfully
 * been verified.
 */
public class InstalledWebappRegistrar {
    private final Context mAppContext;
    private final PermissionUpdater mPermissionUpdater;
    private final Lazy<InstalledWebappDataRecorder> mDataRecorder;

    // These origins have already been registered so we don't need to do so again.
    private final Set<Origin> mRegisteredOrigins = new HashSet<>();

    @Inject
    public InstalledWebappRegistrar(
            @Named(APP_CONTEXT) Context appContext,
            PermissionUpdater permissionUpdater,
            Lazy<InstalledWebappDataRecorder> dataRecorder) {
        mAppContext = appContext;
        mPermissionUpdater = permissionUpdater;
        mDataRecorder = dataRecorder;
    }

    /**
     * Registers to various stores that the app is linked with the origin.
     *
     * We do this here, when the Trusted Web Activity UI is shown instead of in ChromeOriginVerifier
     * when verification completes because when an origin is being verified, we don't know whether
     * it is for the purposes of Trusted Web Activities or for Post Message (where this behaviour is
     * not required).
     *
     * Additionally we do it on every page navigation because an app can be verified for more than
     * one Origin, eg:
     * 1) App verifies with https://www.myfirsttwa.com/.
     * 2) App verifies with https://www.mysecondtwa.com/.
     * 3) App launches a TWA to https://www.myfirsttwa.com/.
     * 4) App navigates to https://www.mysecondtwa.com/.
     *
     * At step 2, we don't know why the app is verifying with that origin (it could be for TWAs or
     * for PostMessage). Only at step 4 do we know that Chrome should associate the browsing data
     * for that origin with that app.
     */
    public void registerClient(String packageName, Origin origin, String pageUrl) {
        if (mRegisteredOrigins.contains(origin)) return;

        // Register that we should wipe data for this origin when the client app is uninstalled.
        mDataRecorder.get().register(packageName, origin);
        // Register that we trust the client app to handle permission for this origin and update
        // Chrome's permission for the origin.
        mPermissionUpdater.onOriginVerified(origin, pageUrl, packageName);

        mRegisteredOrigins.add(origin);
    }
}

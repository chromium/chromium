// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.dom_distiller.core.DomDistillerService;

import java.util.HashMap;

/**
 * DomDistillerServiceFactory maps Profiles to instances of
 * {@link DomDistillerService} instances. Each {@link Profile} will at most
 * have one instance of this service. If the service does not already exist,
 * it will be created on the first access.
 */
@JNINamespace("dom_distiller::android")
public class DomDistillerServiceFactory {

    private static final HashMap<Profile, DomDistillerService> sServiceMap =
            new HashMap<Profile, DomDistillerService>();

    /**
     * Returns Java DomDistillerService for given Profile.
     */
    public static DomDistillerService getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        DomDistillerService service = sServiceMap.get(profile);
        if (service == null) {
            service = DomDistillerServiceFactoryJni.get().getForProfile(profile);
            sServiceMap.put(profile, service);
        }
        return service;
    }

    @NativeMethods
    interface Natives {
        DomDistillerService getForProfile(Profile profile);
    }
}

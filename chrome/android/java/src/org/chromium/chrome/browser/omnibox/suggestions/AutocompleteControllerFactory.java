// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.HashMap;
import java.util.Map;

/**
 * The factory building/retrieving the AutocompleteController objects.
 *
 * Facilitates construction or retrieval of AutocompleteController instances associated with
 * particular user Profiles.
 * The factory may be shared between multiple individual activities, typically Search and regular
 * Chrome activities. To prevent the instances from being separately constructed, this factory
 * operates within singleton realms, replicating the KeyedServiceFactory used on the Native (C++)
 * side.
 */
public class AutocompleteControllerFactory {
    /** A map associating individual Profile objects with their corresponding Controllers */
    private static Map<Profile, AutocompleteController> sControllers = new HashMap<>();

    /** AutocompleteController instance returned for testing purposes. */
    private static AutocompleteController sAutocompleteControllerForTesting;

    /**
     * Retrieve the AutocompleteController associated with the supplied profile.
     *
     * @param profile Profile to request AutocompleteController for.
     * @return AutocompleteController for supplied profile.
     */
    static AutocompleteController getController(Profile profile) {
        if (sAutocompleteControllerForTesting != null) return sAutocompleteControllerForTesting;

        AutocompleteController controller = sControllers.get(profile);
        if (controller != null) return controller;

        controller = new AutocompleteController(profile,
                WarmupManager.getInstance()::createSpareRenderProcessHost,
                () -> removeController(profile));

        sControllers.put(profile, controller);
        return controller;
    }

    /**
     * Remove the controller associated with supplied profile.
     *
     * @param profile Profile for which the controller should be dropped.
     */
    private static void removeController(@NonNull Profile profile) {
        sControllers.remove(profile);
    }

    /**
     * Set the instance of AutocompleteController that will be used for testing purposes.
     * Supplied instance will always be returned, regardless of profile information.
     *
     * @param controller The controller to return, or null to remove test instance.
     */
    public static void setControllerForTesting(@Nullable AutocompleteController controller) {
        sAutocompleteControllerForTesting = controller;
    }
}

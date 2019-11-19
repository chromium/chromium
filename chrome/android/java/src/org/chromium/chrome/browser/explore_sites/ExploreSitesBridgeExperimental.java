// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * The model and controller for a group of explore options.
 */
@JNINamespace("explore_sites")
public class ExploreSitesBridgeExperimental {
    /**
     * Fetches a JSON string from URL, returning the parsed JSONobject in a callback.
     * This will cancel any pending JSON fetches.
     */
    public static void getNtpCategories(
            Profile profile, final Callback<List<ExploreSitesCategoryTile>> callback) {
        List<ExploreSitesCategoryTile> result = new ArrayList<>();
        ExploreSitesBridgeExperimentalJni.get().getNtpCategories(profile, result, callback);
    }

    /**
     * Fetches an icon from a url and returns in a Bitmap image. The callback argument will be null
     * if the operation fails.
     */
    public static void getIcon(
            Profile profile, final String iconUrl, final Callback<Bitmap> callback) {
        ExploreSitesBridgeExperimentalJni.get().getIcon(profile, iconUrl, callback);
    }

    @NativeMethods
    interface Natives {
        /* UX prototype methods. */
        void getNtpCategories(Profile profile, List<ExploreSitesCategoryTile> result,
                Callback<List<ExploreSitesCategoryTile>> callback);

        String getCatalogUrl();
        void getIcon(Profile profile, String iconUrl, Callback<Bitmap> callback);
    }
}

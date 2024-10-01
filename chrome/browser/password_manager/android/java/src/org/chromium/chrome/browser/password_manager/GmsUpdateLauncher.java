// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import com.google.android.gms.common.GoogleApiAvailability;

/** Launches the UI through which the user can update Google Play Services/ */
public class GmsUpdateLauncher {
    // Referrer string for the Google Play Store when installing GMS Core package
    private static final String STORE_REFERER = "chrome_upm";

    /**
     * Opens the store on the Google Play Services page to allow the user to easily update.
     *
     * @param context to start the new store Activity from.
     */
    public static void launch(Context context) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        String deepLinkUrl =
                "market://details?id="
                        + GoogleApiAvailability.GOOGLE_PLAY_SERVICES_PACKAGE
                        + "&referrer="
                        + STORE_REFERER;

        intent.setPackage("com.android.vending");
        intent.setData(Uri.parse(deepLinkUrl));
        intent.putExtra("callerId", context.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Request for overlay flow, Play Store will fallback to the default
        // behaviour if overlay is not available.
        // TODO(crbug.com/40855336): Use AlleyOop v3 overlay UI after fixing Chrome restart
        // during the GMS Core installation.
        // intent.putExtra("overlay", true);
        try {
            context.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            // In case that Google Play Store isn't present on the device, its activity could not
            // have been started.
            // TODO: b/334051261 - Instead of silently failing to open Google Play Store to offer
            // updating GMS Core, either don't offer the option at all or indicate why the update
            // button didn't work.
        }
    }
}

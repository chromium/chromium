// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.instantapps;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/**
 * Used to allow the final intent recipient to verify the caller (startActivityForResult allows
 * them to do so) without launching the final activity in the same task as Chrome.
 */
public class AuthenticatedProxyActivity extends Activity {
    /**
     * The intent extra we expect to receive with the intent that we want to forward.
     */
    public static final String AUTHENTICATED_INTENT_EXTRA =
            "org.chromium.chrome.browser.instantapps.AUTH_INTENT";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Intent forwardIntent = (Intent) getIntent().getParcelableExtra(AUTHENTICATED_INTENT_EXTRA);
        if (forwardIntent != null) {
            // Ensure that the forwardIntent doesn't have FLAG_ACTIVITY_NEW_TASK or
            // FLAG_ACTIVITY_NEW_DOCUMENT set as those don't work with startActivityForResult().
            forwardIntent.setFlags(forwardIntent.getFlags() & ~Intent.FLAG_ACTIVITY_NEW_TASK);
            forwardIntent.setFlags(forwardIntent.getFlags() & ~Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
            // Non-negative result code ensures that startActivityForResult doesn't degrade to
            // startActivity() behaviour.
            startActivityForResult(forwardIntent, 0);
        }
        finish();
    }
}

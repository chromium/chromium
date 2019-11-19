// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import org.chromium.chrome.browser.util.UrlConstants;

/**
* Controls how Google uses Chrome data to personalize Search and other Google services.
*/
public class GoogleActivityController {
    /**
    * Opens the "Web & App Activity" settings that allows the user to control how Google uses Chrome
    * browsing history.
    * @param activity The activity to open the settings.
    * @param accountName The account for which is requested.
    */
    public void openWebAndAppActivitySettings(Activity activity, String accountName) {
        Intent intent = new Intent(
                Intent.ACTION_VIEW, Uri.parse(UrlConstants.GOOGLE_ACCOUNT_ACTIVITY_CONTROLS_URL));
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, activity.getPackageName());
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        intent.setPackage(activity.getPackageName());
        activity.startActivity(intent);
    }
}

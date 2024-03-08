// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityConstants;
import org.chromium.url.GURL;

/** Class facilitating interactions with the SearchActivity and the Omnibox. */
public interface SearchActivityUtils {
    @VisibleForTesting
    /* package */ static final int OMNIBOX_REQUEST_CODE = 'O' << 24 | 'M' << 16 | 'N' << 8 | 'I';

    @VisibleForTesting /* package */ static final String EXTRA_CURRENT_URL = "current-url";

    /**
     * Call up SearchActivity/Omnibox on behalf of the current Activity.
     *
     * <p>Allows the caller to instantiate the Omnibox and retrieve Suggestions for the supplied
     * webpage. Response will be delivered via {@link Activity#onActivityResult}.
     *
     * @param activity the current activity; may be {@code null}, in which case intent will not be
     *     issued
     * @param url the URL of the page to retrieve suggestions for
     */
    public static void requestOmniboxForResult(
            @Nullable Activity activity, @NonNull GURL currentUrl) {
        if (activity == null) return;

        var intent =
                buildTrustedIntent(
                                activity, SearchActivityConstants.ACTION_START_EXTENDED_TEXT_SEARCH)
                        .addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY)
                        .putExtra(EXTRA_CURRENT_URL, currentUrl.getSpec());
        activity.startActivityForResult(intent, OMNIBOX_REQUEST_CODE);
    }

    /**
     * Create a trusted intent that can be used to start the Search Activity.
     *
     * @param context current context
     * @param action action to be associated with the intent
     */
    @VisibleForTesting
    /* package */ static Intent buildTrustedIntent(Context context, String action) {
        var intent =
                new Intent(action).setComponent(new ComponentName(context, SearchActivity.class));
        IntentUtils.addTrustedIntentExtras(intent);
        return intent;
    }
}

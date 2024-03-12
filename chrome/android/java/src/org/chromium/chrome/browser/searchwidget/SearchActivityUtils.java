// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityConstants;
import org.chromium.url.GURL;

/** Class facilitating interactions with the SearchActivity and the Omnibox. */
public class SearchActivityUtils {
    @VisibleForTesting
    /* package */ static final int OMNIBOX_REQUEST_CODE = 'O' << 24 | 'M' << 16 | 'N' << 8 | 'I';

    @VisibleForTesting /* package */ static final String EXTRA_CURRENT_URL = "current-url";
    @VisibleForTesting /* package */ static final String EXTRA_URL_TO_NAVIGATE = "url-to-navigate";

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
                        .putExtra(EXTRA_CURRENT_URL, currentUrl.getSpec())
                        .addFlags(
                                Intent.FLAG_ACTIVITY_NO_HISTORY
                                        | Intent.FLAG_ACTIVITY_PREVIOUS_IS_TOP);

        activity.startActivityForResult(
                intent,
                OMNIBOX_REQUEST_CODE,
                ActivityOptions.makeCustomAnimation(
                                activity, android.R.anim.fade_in, R.anim.no_anim)
                        .toBundle());
    }

    /**
     * @return true if the intent represents request for Omnibox for current activity.
     */
    /* package */ static boolean isOmniboxRequestForResult(@NonNull Intent intent) {
        return IntentUtils.isTrustedIntentFromSelf(intent)
                && IntentUtils.safeHasExtra(intent, EXTRA_CURRENT_URL);
    }

    /**
     * Resolve the {@link requestOmniboxForResult}.
     *
     * @param activity the activity resolving the request
     * @param url optional URL dictating how to resolve the request: null/invalid/empty value
     *     results with canceled request; anything else resolves request successfully
     */
    /* package */ static void resolveOmniboxRequestForResult(
            @NonNull Activity activity, @Nullable GURL url) {
        if (GURL.isEmptyOrInvalid(url)) {
            activity.setResult(Activity.RESULT_CANCELED);
            return;
        }

        var intent = new Intent().setPackage(activity.getCallingPackage());

        // Do not pass any of these information if the calling package is something we did not
        // expect, but somehow it managed to fabricate a trust token.
        if (IntentUtils.intentTargetsSelf(activity, intent)) {
            intent.putExtra(EXTRA_URL_TO_NAVIGATE, url.getSpec());
            IntentUtils.addTrustedIntentExtras(intent);
        }

        activity.setResult(Activity.RESULT_OK, intent);
    }

    /**
     * Utility method to determine whether the {@link Activity#onActivityResult} payload carries the
     * response to {@link requestOmniboxForResult}.
     *
     * @param requestCode the request code received in {@link Activity#onActivityResult}
     * @param intent the intent data received in {@link Activity#onActivityResult}
     * @return true if the response captures legitimate Omnibox result.
     */
    public static boolean isOmniboxResult(int requestCode, @NonNull Intent intent) {
        return requestCode == OMNIBOX_REQUEST_CODE
                && IntentUtils.isTrustedIntentFromSelf(intent)
                && IntentUtils.safeHasExtra(intent, EXTRA_URL_TO_NAVIGATE);
    }

    /**
     * Process the {@link Activity#onActivityResult} payload for Omnibox navigation result.
     *
     * @param requestCode the request code received in {@link Activity#onActivityResult}
     * @param resultCode the result code received in {@link Activity#onActivityResult}
     * @param intent the intent data received in {@link Activity#onActivityResult}
     * @return null, if result is not a valid Omnibox result, otherwise a GURL object; empty GURL
     *     indicates no navigation
     */
    public static @Nullable GURL getOmniboxResult(
            int requestCode, int resultCode, @NonNull Intent intent) {
        if (!isOmniboxResult(requestCode, intent)) return null;
        if (resultCode != Activity.RESULT_OK) return GURL.emptyGURL();
        return new GURL(IntentUtils.safeGetStringExtra(intent, EXTRA_URL_TO_NAVIGATE));
    }

    /**
     * Create a trusted intent that can be used to start the Search Activity.
     *
     * @param context current context
     * @param action action to be associated with the intent
     */
    @VisibleForTesting
    /* package */ static Intent buildTrustedIntent(
            @NonNull Context context, @NonNull String action) {
        var intent =
                new Intent(action).setComponent(new ComponentName(context, SearchActivity.class));
        IntentUtils.addTrustedIntentExtras(intent);
        return intent;
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.annotation.SuppressLint;
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
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.url.GURL;

/** Class facilitating interactions with the SearchActivity and the Omnibox. */
public class SearchActivityUtils implements SearchActivityClient {
    @VisibleForTesting
    /* package */ static final int OMNIBOX_REQUEST_CODE = 'O' << 24 | 'M' << 16 | 'N' << 8 | 'I';

    @VisibleForTesting /* package */ static final String EXTRA_ORIGIN = "origin";
    @VisibleForTesting /* package */ static final String EXTRA_SEARCH_TYPE = "search-type";
    @VisibleForTesting /* package */ static final String EXTRA_CURRENT_URL = "current-url";
    @VisibleForTesting /* package */ static final String EXTRA_URL_TO_NAVIGATE = "url-to-navigate";

    // Note: while we don't rely on Actions, PendingIntents do require them to be Unique.
    // Responsibility to define values for PendingIntents could be offset to Caller; meantime we
    // offer complimentary default values.
    @VisibleForTesting
    /* package */ static final String ACTION_SEARCH_FORMAT =
            "org.chromium.chrome.browser.ui.searchactivityutils.ACTION_SEARCH:%d:%d";

    @Override
    public Intent createIntent(
            @NonNull Context context,
            @IntentOrigin int origin,
            @Nullable GURL url,
            @SearchType int searchType) {
        // Ensure `action` is unique especially across different Widget implementations.
        // Otherwise, a QuickActionSearchWidget action may override the SearchActivity widget,
        // triggering functionality we might not want to activate.
        @SuppressLint("DefaultLocale")
        String action = String.format(ACTION_SEARCH_FORMAT, origin, searchType);

        var intent = buildTrustedIntent(context, action);
        intent.putExtra(EXTRA_ORIGIN, origin)
                .putExtra(EXTRA_SEARCH_TYPE, searchType)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT)
                .putExtra(EXTRA_CURRENT_URL, GURL.isEmptyOrInvalid(url) ? null : url.getSpec());

        return intent;
    }

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

        @SuppressLint("DefaultLocale")
        var intent =
                buildTrustedIntent(
                                activity,
                                String.format(
                                        ACTION_SEARCH_FORMAT,
                                        IntentOrigin.CUSTOM_TAB,
                                        SearchType.TEXT))
                        .putExtra(
                                EXTRA_CURRENT_URL,
                                GURL.isEmptyOrInvalid(currentUrl) ? null : currentUrl.getSpec())
                        .putExtra(EXTRA_ORIGIN, IntentOrigin.CUSTOM_TAB)
                        .putExtra(EXTRA_SEARCH_TYPE, SearchType.TEXT)
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
     * Retrieve the intent origin.
     *
     * @param intent intent received by SearchActivity
     * @return the origin of an intent
     */
    /* package */ static @IntentOrigin int getIntentOrigin(@NonNull Intent intent) {
        if (IntentUtils.isTrustedIntentFromSelf(intent)) {
            return IntentUtils.safeGetIntExtra(intent, EXTRA_ORIGIN, IntentOrigin.UNKNOWN);
        }

        return IntentOrigin.UNKNOWN;
    }

    /**
     * @return the document url associated with the intent, if the intent is trusted and carries
     *     valid URL.
     */
    /* package */ static @Nullable GURL getIntentUrl(@NonNull Intent intent) {
        if (IntentUtils.isTrustedIntentFromSelf(intent)) {
            var gurl = new GURL(IntentUtils.safeGetStringExtra(intent, EXTRA_CURRENT_URL));
            if (!GURL.isEmptyOrInvalid(gurl)) return gurl;
        }
        return null;
    }

    /**
     * Retrieve the intent search type.
     *
     * @param intent intent received by SearchActivity
     * @return the requested search type
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static @SearchType int getIntentSearchType(@NonNull Intent intent) {
        if (IntentUtils.isTrustedIntentFromSelf(intent)) {
            return IntentUtils.safeGetIntExtra(intent, EXTRA_SEARCH_TYPE, SearchType.TEXT);
        }

        return SearchType.TEXT;
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

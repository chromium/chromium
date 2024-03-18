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
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;

/** Class facilitating interactions with the SearchActivity and the Omnibox. */
public class SearchActivityUtils implements SearchActivityClient {
    @VisibleForTesting
    /* package */ static final int OMNIBOX_REQUEST_CODE = 'O' << 24 | 'M' << 16 | 'N' << 8 | 'I';

    @VisibleForTesting /* package */ static final String EXTRA_ORIGIN = "origin";
    @VisibleForTesting /* package */ static final String EXTRA_SEARCH_TYPE = "search-type";
    @VisibleForTesting /* package */ static final String EXTRA_CURRENT_URL = "current-url";

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
            @NonNull Activity activity, @NonNull OmniboxLoadUrlParams params) {
        var intent = createLoadUrlIntent(activity, activity.getCallingActivity(), params);
        if (intent != null) {
            activity.setResult(Activity.RESULT_OK, intent);
        } else {
            activity.setResult(Activity.RESULT_CANCELED);
        }
    }

    /**
     * Creates an intent that can be used to launch Chrome.
     *
     * @param context current context
     * @param params information about what url to load and what additional data to pass
     * @return the intent will be passed to ChromeLauncherActivity, or null if page cannot be loaded
     */
    /* package */ static @Nullable Intent createIntentForStartActivity(
            Context context, OmniboxLoadUrlParams params) {
        var intent =
                createLoadUrlIntent(
                        context, new ComponentName(context, ChromeLauncherActivity.class), params);
        if (intent == null) return null;

        intent.setAction(Intent.ACTION_VIEW);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);

        return intent;
    }

    /**
     * Create a base intent that can be further expanded to request URL loading.
     *
     * @param context the current context
     * @param recipient the activity being targeted
     * @param params the OmniboxLoadUrlParams describing what URL to load and what extra data to
     *     pass
     * @return Intent, if all the supplied data is valid, otherwise null
     */
    @VisibleForTesting
    /* package */ static @Nullable Intent createLoadUrlIntent(
            Context context, ComponentName recipient, OmniboxLoadUrlParams params) {
        // Don't do anything if the input was empty.
        if (params == null || TextUtils.isEmpty(params.url)) return null;

        // Fix up the URL and send it to the full browser.
        GURL fixedUrl = UrlFormatter.fixupUrl(params.url);
        if (GURL.isEmptyOrInvalid(fixedUrl)) return null;

        var intent =
                new Intent()
                        .putExtra(SearchActivity.EXTRA_FROM_SEARCH_ACTIVITY, true)
                        .setComponent(recipient)
                        .setData(Uri.parse(fixedUrl.getSpec()));

        // Do not pass any of these information if the calling package is something we did not
        // expect, but somehow it managed to fabricate a trust token.
        if (!IntentUtils.intentTargetsSelf(context, intent)) {
            return null;
        }

        if (!TextUtils.isEmpty(params.postDataType)
                && params.postData != null
                && params.postData.length != 0) {
            intent.putExtra(IntentHandler.EXTRA_POST_DATA_TYPE, params.postDataType)
                    .putExtra(IntentHandler.EXTRA_POST_DATA, params.postData);
        }
        IntentUtils.addTrustedIntentExtras(intent);

        return intent;
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
                && !TextUtils.isEmpty(intent.getDataString());
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
        return new GURL(intent.getDataString());
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

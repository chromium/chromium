// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.Activity;
import android.app.SearchManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;

/** Class facilitating interactions with the SearchActivity and the Omnibox. */
public class SearchActivityUtils {
    private static final String TAG = "SAUtils";

    /**
     * Retrieve the intent origin.
     *
     * @param intent intent received by SearchActivity
     * @return the origin of an intent
     */
    /* package */ static @IntentOrigin int getIntentOrigin(@NonNull Intent intent) {
        if (IntentUtils.isTrustedIntentFromSelf(intent)) {
            return IntentUtils.safeGetIntExtra(
                    intent, SearchActivityExtras.EXTRA_ORIGIN, IntentOrigin.UNKNOWN);
        }

        return IntentOrigin.UNKNOWN;
    }

    /**
     * @return the document url associated with the intent, if the intent is trusted and carries
     *     valid URL.
     */
    /* package */ static @Nullable GURL getIntentUrl(@NonNull Intent intent) {
        if (IntentUtils.isTrustedIntentFromSelf(intent)) {
            var gurl =
                    new GURL(
                            IntentUtils.safeGetStringExtra(
                                    intent, SearchActivityExtras.EXTRA_CURRENT_URL));
            if (!GURL.isEmptyOrInvalid(gurl)) return gurl;
        }
        return null;
    }

    /**
     * @return the package name on behalf of which the intent was issued.
     */
    /* package */ static @Nullable String getReferrer(@NonNull Intent intent) {
        String referrer = null;
        if (IntentUtils.isTrustedIntentFromSelf(intent)) {
            referrer = IntentUtils.safeGetStringExtra(intent, SearchActivityExtras.EXTRA_REFERRER);
            if (referrer != null
                    && !referrer.matches(SearchActivityExtras.REFERRER_VALIDATION_REGEX)) {
                Log.e(
                        TAG,
                        String.format(
                                "Invalid referrer: '%s' found. Referrer will be removed.",
                                referrer));
                referrer = null;
            }
        }
        return TextUtils.isEmpty(referrer) ? null : referrer;
    }

    /**
     * @return the incognito status of the associated launching activity.
     */
    /* package */ static boolean getIntentIncognitoStatus(@NonNull Intent intent) {
        boolean isIncognito = false;
        if (IntentUtils.isTrustedIntentFromSelf(intent)) {
            isIncognito =
                    IntentUtils.safeGetBooleanExtra(
                            intent, SearchActivityExtras.EXTRA_IS_INCOGNITO, false);
        }
        return isIncognito;
    }

    /** Returns the caller-supplied initial search query. */
    /* package */ static @Nullable String getIntentQuery(@NonNull Intent intent) {
        // Unlike most other intents, this does not require trusted extras.
        return IntentUtils.safeGetStringExtra(intent, SearchManager.QUERY);
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
            return IntentUtils.safeGetIntExtra(
                    intent, SearchActivityExtras.EXTRA_SEARCH_TYPE, SearchType.TEXT);
        }

        return SearchType.TEXT;
    }

    /**
     * Resolve the {@link requestOmniboxForResult}.
     *
     * @param activity the activity resolving the request
     * @param params optional URL dictating how to resolve the request: null/invalid/empty value
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
                        context,
                        new ComponentName(
                                ContextUtils.getApplicationContext(), ChromeLauncherActivity.class),
                        params);
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
}

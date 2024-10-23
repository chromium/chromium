// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.url.GURL;

import java.util.Locale;

/** Class with logic enabling clients to interact with SearchActivity. */
public class SearchActivityClientImpl implements SearchActivityClient {
    private static final String TAG = "SAClient";

    // Base identifier used by the SAClient to identify requests for result.
    @VisibleForTesting
    /* package */ static final int OMNIBOX_REQUEST_CODE = 'O' << 24 | 'M' << 16 | 'N' << 8;

    // Intent Action format string, used to uniquely identify specific type of action requested
    // by the widget.
    // Note: while we don't rely on Actions, PendingIntents do require them to be Unique.
    // Responsibility to define values for PendingIntents could be offset to Caller; meantime we
    // offer complimentary default values.
    @VisibleForTesting
    /* package */ static final String ACTION_SEARCH_FORMAT =
            "org.chromium.chrome.browser.ui.searchactivityutils.ACTION_SEARCH:%d:%d";

    private final @IntentOrigin int mOrigin;

    /**
     * Creates new instance of the SearchActivityClient.
     *
     * @param origin The {@link IntentOrigin} value representing the client.
     */
    public SearchActivityClientImpl(@IntentOrigin int origin) {
        mOrigin = origin;
    }

    @Override
    public Intent createIntent(
            @NonNull Context context, @Nullable GURL url, @SearchType int searchType) {
        // Ensure `action` is unique especially across different Widget implementations.
        // Otherwise, a QuickActionSearchWidget action may override the SearchActivity widget,
        // triggering functionality we might not want to activate.
        String action =
                String.format(Locale.getDefault(), ACTION_SEARCH_FORMAT, mOrigin, searchType);

        var intent = buildTrustedIntent(context, action);
        intent.putExtra(SearchActivityExtras.EXTRA_ORIGIN, mOrigin)
                .putExtra(SearchActivityExtras.EXTRA_SEARCH_TYPE, searchType)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT)
                .putExtra(
                        SearchActivityExtras.EXTRA_CURRENT_URL,
                        GURL.isEmptyOrInvalid(url) ? null : url.getSpec());

        return intent;
    }

    @Override
    public void requestOmniboxForResult(
            @Nullable Activity activity,
            @NonNull GURL currentUrl,
            @Nullable String referrer,
            boolean isIncognito) {
        if (activity == null) return;

        if (referrer != null && !referrer.matches(SearchActivityExtras.REFERRER_VALIDATION_REGEX)) {
            Log.e(
                    TAG,
                    String.format(
                            "Referrer: '%s' failed to match Re pattern '%s' and will be ignored.",
                            referrer, SearchActivityExtras.REFERRER_VALIDATION_REGEX));
            referrer = null;
        }

        activity.startActivityForResult(
                createServiceRequestIntent(
                        activity,
                        mOrigin,
                        GURL.isEmptyOrInvalid(currentUrl) ? null : currentUrl.getSpec(),
                        referrer,
                        /* isServiceIntent= */ true,
                        isIncognito),
                getClientUniqueRequestCode(),
                ActivityOptions.makeCustomAnimation(
                                activity, android.R.anim.fade_in, R.anim.no_anim)
                        .toBundle());
    }

    @Override
    public boolean isOmniboxResult(int requestCode, @NonNull Intent intent) {
        return requestCode == getClientUniqueRequestCode()
                && IntentUtils.isTrustedIntentFromSelf(intent)
                && !TextUtils.isEmpty(intent.getDataString());
    }

    @Override
    public @Nullable LoadUrlParams getOmniboxResult(
            int requestCode, int resultCode, @NonNull Intent intent) {
        if (!isOmniboxResult(requestCode, intent)) return null;
        if (resultCode != Activity.RESULT_OK) return null;
        var url = new GURL(intent.getDataString());
        if (GURL.isEmptyOrInvalid(url)) return null;

        var params = new LoadUrlParams(url);
        byte[] postData = IntentUtils.safeGetByteArrayExtra(intent, IntentHandler.EXTRA_POST_DATA);
        String postDataType =
                IntentUtils.safeGetStringExtra(intent, IntentHandler.EXTRA_POST_DATA_TYPE);
        if (!TextUtils.isEmpty(postDataType) && postData != null && postData.length > 0) {
            params.setVerbatimHeaders("Content-Type: " + postDataType);
            params.setPostData(ResourceRequestBody.createFromBytes(postData));
        }
        return params;
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

    /** Returns the Request Code to be used with startActivityForResult/onActivityResult. */
    @VisibleForTesting
    /* package */ int getClientUniqueRequestCode() {
        return OMNIBOX_REQUEST_CODE | mOrigin;
    }

    /**
     * Create an intent to be used to startActivityForResult.
     *
     * @param activity the activity to use to build trusted intent
     * @param intentOrigin the component requesting service
     * @param currentUrl the current url, used as a context for the service
     * @param referrer package name of the app on behalf of which the request is made - or null
     * @param isServiceIntent whether SearchActivity should serve result (true), or take action
     *     (false)
     * @param isIncognito whether Omnibox is requested for the incognito profile
     * @return newly created Intent
     */
    @VisibleForTesting
    /* package */ static Intent createServiceRequestIntent(
            @NonNull Activity activity,
            @IntentOrigin int intentOrigin,
            @Nullable String currentUrl,
            @Nullable String referrer,
            boolean isServiceIntent,
            boolean isIncognito) {
        return buildTrustedIntent(
                        activity,
                        String.format(
                                Locale.getDefault(),
                                ACTION_SEARCH_FORMAT,
                                intentOrigin,
                                SearchType.TEXT))
                .putExtra(SearchActivityExtras.EXTRA_CURRENT_URL, currentUrl)
                .putExtra(SearchActivityExtras.EXTRA_ORIGIN, intentOrigin)
                .putExtra(
                        SearchActivityExtras.EXTRA_REFERRER,
                        TextUtils.isEmpty(referrer) ? null : referrer)
                .putExtra(SearchActivityExtras.EXTRA_SEARCH_TYPE, SearchType.TEXT)
                .putExtra(SearchActivityExtras.EXTRA_IS_INCOGNITO, isIncognito)
                .putExtra(SearchActivityExtras.EXTRA_IS_SERVICE_REQUEST, isServiceIntent)
                .addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY | Intent.FLAG_ACTIVITY_PREVIOUS_IS_TOP);
    }
}

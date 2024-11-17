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
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.ResolutionType;
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

    private final @Nullable Context mContext;
    private final @IntentOrigin int mOrigin;

    private static class IntentBuilderImpl implements IntentBuilder {
        private Intent mIntent;
        private @IntentOrigin int mOrigin;
        private @SearchType int mSearchType;

        IntentBuilderImpl(Context context, int origin) {
            mOrigin = origin;
            mIntent = new Intent();
            mIntent.setComponent(new ComponentName(context, SearchActivity.class));
            mIntent.putExtra(SearchActivityExtras.EXTRA_ORIGIN, origin);
            mIntent.addFlags(
                    Intent.FLAG_ACTIVITY_NO_HISTORY | Intent.FLAG_ACTIVITY_PREVIOUS_IS_TOP);

            // Initialize defaults.
            setSearchType(SearchType.TEXT);
            setResolutionType(ResolutionType.OPEN_IN_CHROME);
        }

        @Override
        public IntentBuilder setSearchType(@SearchType int searchType) {
            mIntent.putExtra(SearchActivityExtras.EXTRA_SEARCH_TYPE, searchType);
            mSearchType = searchType;
            return this;
        }

        @Override
        public IntentBuilder setPageUrl(GURL url) {
            mIntent.putExtra(
                    SearchActivityExtras.EXTRA_CURRENT_URL,
                    GURL.isEmptyOrInvalid(url) ? null : url.getSpec());
            return this;
        }

        @Override
        public IntentBuilder setReferrer(String referrer) {
            if (referrer != null
                    && !referrer.matches(SearchActivityExtras.REFERRER_VALIDATION_REGEX)) {
                Log.e(
                        TAG,
                        String.format(
                                "Referrer: '%s' failed to match Re pattern '%s' and will be"
                                        + " ignored.",
                                referrer, SearchActivityExtras.REFERRER_VALIDATION_REGEX));
                referrer = null;
            }

            mIntent.putExtra(
                    SearchActivityExtras.EXTRA_REFERRER,
                    TextUtils.isEmpty(referrer) ? null : referrer);
            return this;
        }

        @Override
        public IntentBuilder setIncognito(boolean isIncognito) {
            mIntent.putExtra(SearchActivityExtras.EXTRA_IS_INCOGNITO, isIncognito);
            return this;
        }

        @Override
        public IntentBuilder setResolutionType(@ResolutionType int resolutionType) {
            mIntent.putExtra(SearchActivityExtras.EXTRA_RESOLUTION_TYPE, resolutionType);
            return this;
        }

        @Override
        public Intent build() {
            // Ensure `action` is unique especially across different Widget implementations.
            // Otherwise, a QuickActionSearchWidget action may override the SearchActivity widget,
            // triggering functionality we might not want to activate.
            mIntent.setAction(
                    String.format(Locale.getDefault(), ACTION_SEARCH_FORMAT, mOrigin, mSearchType));
            // Ensure a copy is made so that the builder can be reused, producing variations of an
            // intent.
            var intent = new Intent(mIntent);
            IntentUtils.addTrustedIntentExtras(intent);
            return intent;
        }
    }

    /**
     * Creates new instance of the SearchActivityClient.
     *
     * @param context Current context. This must be the Activity facilitating exchange, if the
     *     caller intends to use the requestOmniboxForResult call.
     * @param origin The {@link IntentOrigin} value representing the client.
     */
    public SearchActivityClientImpl(Context context, @IntentOrigin int origin) {
        mContext = context;
        mOrigin = origin;
    }

    @Override
    public IntentBuilder newIntentBuilder() {
        return new IntentBuilderImpl(mContext, mOrigin);
    }

    @Override
    public void requestOmniboxForResult(Intent intent) {
        if (!(mContext instanceof Activity)) {
            Log.w(TAG, "Intent not dispatched; SearchActivityClient not associated with Activity");
            return;
        }

        ((Activity) mContext)
                .startActivityForResult(
                        intent,
                        getClientUniqueRequestCode(),
                        ActivityOptions.makeCustomAnimation(
                                        mContext, android.R.anim.fade_in, R.anim.no_anim)
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

    /** Returns the Request Code to be used with startActivityForResult/onActivityResult. */
    @VisibleForTesting
    /* package */ int getClientUniqueRequestCode() {
        return OMNIBOX_REQUEST_CODE | mOrigin;
    }
}

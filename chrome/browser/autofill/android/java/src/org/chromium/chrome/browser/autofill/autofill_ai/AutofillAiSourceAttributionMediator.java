// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.net.Uri;

import androidx.annotation.DrawableRes;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.HeaderProperties;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.ItemType;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.SourceCardProperties;
import org.chromium.components.autofill.autofill_ai.AutofillAiSourceAttributionInfo;
import org.chromium.components.autofill.autofill_ai.SourceType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

@NullMarked
class AutofillAiSourceAttributionMediator {
    private final Context mContext;
    private final ModelList mModelList;
    private boolean mIsDestroyed;

    AutofillAiSourceAttributionMediator(
            Context context,
            ModelList modelList,
            List<AutofillAiSourceAttributionInfo> sources,
            String subtitle) {
        mContext = context;
        mModelList = modelList;

        mModelList.add(createHeader(subtitle));
        for (AutofillAiSourceAttributionInfo source : sources) {
            ListItem card = createSourceCard(source);
            if (card != null) {
                mModelList.add(card);
            }
        }
    }

    void destroy() {
        if (mIsDestroyed) {
            return;
        }
        mIsDestroyed = true;
        mModelList.clear();
    }

    private static ListItem createHeader(String subtitle) {
        PropertyModel headerModel =
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.SUBTITLE, subtitle)
                        .build();
        return new ListItem(ItemType.HEADER, headerModel);
    }

    private @Nullable ListItem createSourceCard(AutofillAiSourceAttributionInfo source) {
        @SourceType int sourceType = source.getSourceType();

        @DrawableRes int iconResId;
        String appName;
        switch (sourceType) {
            case SourceType.GMAIL:
                iconResId = R.drawable.autofill_ai_gmail_24dp;
                appName = mContext.getString(R.string.autofill_ai_source_app_gmail);
                break;
            case SourceType.PHOTOS:
                iconResId = R.drawable.autofill_ai_photos_24dp;
                appName = mContext.getString(R.string.autofill_ai_source_app_photos);
                break;
            default:
                assert false : "Unexpected source type: " + sourceType;
                return null;
        }

        String contentDescription =
                mContext.getString(
                        R.string.autofill_ai_attribution_open_source_description,
                        appName,
                        source.getTitle());
        PropertyModel cardModel =
                new PropertyModel.Builder(SourceCardProperties.ALL_KEYS)
                        .with(SourceCardProperties.ICON_RES_ID, iconResId)
                        .with(SourceCardProperties.TITLE, source.getTitle())
                        .with(SourceCardProperties.CONTENT_DESCRIPTION, contentDescription)
                        .with(
                                SourceCardProperties.ON_CLICK_LISTENER,
                                () -> onSourceClicked(source.getUrl(), sourceType))
                        .build();
        return new ListItem(ItemType.SOURCE_CARD, cardModel);
    }

    private void onSourceClicked(GURL url, @SourceType int sourceType) {
        if (mIsDestroyed) {
            return;
        }
        launchCustomTab(mContext, url, sourceType);
    }

    private static void launchCustomTab(Context context, GURL url, @SourceType int sourceType) {
        Activity activity = ContextUtils.activityFromContext(context);
        if (activity == null || activity.isFinishing() || activity.isDestroyed()) {
            return;
        }
        try {
            CustomTabsIntent customTabsIntent =
                    new CustomTabsIntent.Builder().setShowTitle(true).build();
            customTabsIntent.intent.setPackage(context.getPackageName());
            customTabsIntent.launchUrl(context, Uri.parse(url.getSpec()));
            RecordHistogram.recordEnumeratedHistogram(
                    "Autofill.Ai.AttributionSheet.SourceClicked",
                    sourceType,
                    SourceType.MAX_VALUE + 1);
        } catch (ActivityNotFoundException | SecurityException ignored) {
            // Avoid logging personal-context URLs to logcat.
        }
    }
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipData.Item;
import android.content.ClipDescription;
import android.content.Context;
import android.content.Intent;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.DragAndDropLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.dragdrop.DragAndDropBrowserDelegate;
import org.chromium.ui.dragdrop.DragDropMetricUtils.UrlIntentSource;
import org.chromium.ui.dragdrop.DropDataAndroid;
import org.chromium.ui.dragdrop.DropDataProviderImpl;
import org.chromium.ui.dragdrop.DropDataProviderUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Delegate for browser related functions used by Drag and Drop. */
public class ChromeDragAndDropBrowserDelegate implements DragAndDropBrowserDelegate {
    private static final String PARAM_CLEAR_CACHE_DELAYED_MS = "ClearCacheDelayedMs";
    @VisibleForTesting static final String PARAM_DROP_IN_CHROME = "DropInChrome";
    private final String[] mSupportedMimeTypes;

    private final Context mContext;
    private final boolean mSupportDropInChrome;
    private final boolean mSupportAnimatedImageDragShadow;

    /**
     * @param context The current context this delegate is associated with.
     */
    public ChromeDragAndDropBrowserDelegate(Context context) {
        mContext = context;
        mSupportDropInChrome =
                ContentFeatureMap.getInstance()
                        .getFieldTrialParamByFeatureAsBoolean(
                                ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU,
                                PARAM_DROP_IN_CHROME,
                                false);
        mSupportAnimatedImageDragShadow =
                ChromeFeatureList.isEnabled(ChromeFeatureList.ANIMATED_IMAGE_DRAG_SHADOW);

        int delay =
                ContentFeatureMap.getInstance()
                        .getFieldTrialParamByFeatureAsInt(
                                ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU,
                                PARAM_CLEAR_CACHE_DELAYED_MS,
                                DropDataProviderImpl.DEFAULT_CLEAR_CACHED_DATA_INTERVAL_MS);
        DropDataProviderUtils.setClearCachedDataIntervalMs(delay);

        List<String> supportedMimeTypeList = new ArrayList();
        supportedMimeTypeList.add(MimeTypeUtils.CHROME_MIMETYPE_TAB);
        if (!TabUiFeatureUtilities.DISABLE_DRAG_TO_NEW_INSTANCE_DD.getValue()) {
            supportedMimeTypeList.addAll(
                    Arrays.asList(
                            ClipDescription.MIMETYPE_TEXT_PLAIN,
                            ClipDescription.MIMETYPE_TEXT_INTENT,
                            MimeTypeUtils.CHROME_MIMETYPE_LINK));
        }
        mSupportedMimeTypes = new String[supportedMimeTypeList.size()];
        supportedMimeTypeList.toArray(mSupportedMimeTypes);
    }

    @Override
    public boolean getSupportDropInChrome() {
        return mSupportDropInChrome;
    }

    @Override
    public boolean getSupportAnimatedImageDragShadow() {
        return mSupportAnimatedImageDragShadow;
    }

    @Override
    public DragAndDropPermissions getDragAndDropPermissions(DragEvent dropEvent) {
        assert mSupportDropInChrome : "Should only be accessed when drop in Chrome.";

        Activity activity = ContextUtils.activityFromContext(mContext);
        if (activity == null) {
            return null;
        }
        return activity.requestDragAndDropPermissions(dropEvent);
    }

    @Override
    public Intent createUrlIntent(String urlString, @UrlIntentSource int intentSrc) {
        Intent intent = null;
        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            intent =
                    DragAndDropLauncherActivity.getLinkLauncherIntent(
                            mContext,
                            urlString,
                            MultiWindowUtils.getInstanceIdForLinkIntent(
                                    ContextUtils.activityFromContext(mContext)),
                            intentSrc);
        }
        return intent;
    }

    @Override
    public ClipData buildClipData(@NonNull DropDataAndroid dropData) {
        assert dropData instanceof ChromeDropDataAndroid;
        ChromeDropDataAndroid chromeDropDataAndroid = (ChromeDropDataAndroid) dropData;
        Intent intent = null;
        if (!TabUiFeatureUtilities.DISABLE_DRAG_TO_NEW_INSTANCE_DD.getValue()) {
            intent =
                    createUrlIntent(
                            chromeDropDataAndroid.mTab.getUrl().getSpec(),
                            UrlIntentSource.TAB_IN_STRIP);
        }
        return new ClipData(
                null,
                mSupportedMimeTypes,
                new Item(chromeDropDataAndroid.buildTabClipDataText(), intent, null));
    }
}

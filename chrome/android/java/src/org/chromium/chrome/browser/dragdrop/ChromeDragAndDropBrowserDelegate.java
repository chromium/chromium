// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.content.ClipData;
import android.content.ClipData.Item;
import android.content.ClipDescription;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.dragdrop.DragAndDropBrowserDelegate;
import org.chromium.ui.dragdrop.DragDropMetricUtils.UrlIntentSource;
import org.chromium.ui.dragdrop.DropDataAndroid;
import org.chromium.ui.dragdrop.DropDataProviderImpl;
import org.chromium.ui.dragdrop.DropDataProviderUtils;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/** Delegate for browser related functions used by Drag and Drop. */
public class ChromeDragAndDropBrowserDelegate implements DragAndDropBrowserDelegate {
    private static final String TAG = "DragDrop";
    private static final String PARAM_CLEAR_CACHE_DELAYED_MS = "ClearCacheDelayedMs";
    @VisibleForTesting static final String PARAM_DROP_IN_CHROME = "DropInChrome";
    private final String[] mSupportedMimeTypes =
            new String[] {
                MimeTypeUtils.CHROME_MIMETYPE_TAB,
                ClipDescription.MIMETYPE_TEXT_PLAIN,
                ClipDescription.MIMETYPE_TEXT_INTENT,
                MimeTypeUtils.CHROME_MIMETYPE_LINK
            };

    private final Context mContext;
    private final Activity mActivity;
    private final boolean mSupportDropInChrome;
    private final boolean mSupportAnimatedImageDragShadow;

    /**
     * @param context The current context this delegate is associated with.
     */
    public ChromeDragAndDropBrowserDelegate(Context context) {
        mContext = context;
        mActivity = ContextUtils.activityFromContext(mContext);
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

        if (mActivity == null) {
            return null;
        }
        return mActivity.requestDragAndDropPermissions(dropEvent);
    }

    @Override
    public Intent createUrlIntent(String urlString, @UrlIntentSource int intentSrc) {
        Intent intent = null;
        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            intent =
                    DragAndDropLauncherActivity.getLinkLauncherIntent(
                            mContext,
                            urlString,
                            MultiWindowUtils.getInstanceIdForLinkIntent(mActivity),
                            intentSrc);
        }
        return intent;
    }

    @Override
    public ClipData buildClipData(@NonNull DropDataAndroid dropData) {
        assert dropData instanceof ChromeDropDataAndroid;
        ChromeDropDataAndroid chromeDropDataAndroid = (ChromeDropDataAndroid) dropData;
        if (chromeDropDataAndroid.hasTab() && chromeDropDataAndroid.allowTabDragToCreateInstance) {
            ClipData clipData = buildClipDataForTabTearing(chromeDropDataAndroid.tab);
            if (clipData != null) return clipData;
        }
        String text =
                chromeDropDataAndroid.hasTab()
                        ? chromeDropDataAndroid.buildTabClipDataText()
                        : dropData.text;
        return new ClipData(null, mSupportedMimeTypes, new Item(text));
    }

    private @Nullable ClipData buildClipDataForTabTearing(Tab tab) {
        Intent intent =
                DragAndDropLauncherActivity.getTabIntent(
                        tab.getContext(), tab, MultiWindowUtils.INVALID_INSTANCE_ID);
        if (intent != null) {
            ActivityOptions opts = ActivityOptions.makeBasic();
            ApiCompatibilityUtils.setCreatorActivityOptionsBackgroundActivityStartMode(opts);
            PendingIntent pendingIntent =
                    PendingIntent.getActivity(
                            tab.getContext(),
                            0,
                            intent,
                            PendingIntent.FLAG_IMMUTABLE,
                            opts.toBundle());
            Item item = ClipDataItemBuilder.buildClipDataItemWithPendingIntent(pendingIntent);
            return item == null
                    ? new ClipData(null, mSupportedMimeTypes, new Item(intent))
                    : new ClipData(null, mSupportedMimeTypes, item);
        }
        return null;
    }

    @Override
    public int buildFlags(int originalFlag, DropDataAndroid dropData) {
        assert dropData instanceof ChromeDropDataAndroid;
        ChromeDropDataAndroid chromeDropData = (ChromeDropDataAndroid) dropData;
        if (!chromeDropData.hasTab() || !chromeDropData.allowTabDragToCreateInstance) {
            return originalFlag;
        }
        return originalFlag
                | ClipDataItemBuilder.DRAG_FLAG_GLOBAL_SAME_APPLICATION
                | ClipDataItemBuilder.DRAG_FLAG_START_PENDING_INTENT_ON_UNHANDLED_DRAG;
    }

    /**
     * A class to handle the reflection of android.content.ClipData.ItemBuilder for builds that
     * don't have the SDK linked. This returns null if reflection fails, or if
     * |setClipDataForTesting| was called earlier returns the provided value.
     * TODO(crbug.com/328511660): Replace with OS provided values / APIs when available.
     */
    public static class ClipDataItemBuilder {
        static final int DRAG_FLAG_GLOBAL_SAME_APPLICATION = 1 << 12;
        static final int DRAG_FLAG_START_PENDING_INTENT_ON_UNHANDLED_DRAG = 1 << 13;
        private static Item sItemForTesting;
        private static boolean sDefinedItemForTesting;
        private static boolean sAttemptedReflection;
        private static Class sItemBuilder;

        static ClipData.Item buildClipDataItemWithPendingIntent(PendingIntent pendingIntent) {
            if (sDefinedItemForTesting) return sItemForTesting;
            try {
                if (!sAttemptedReflection) {
                    sItemBuilder = Class.forName("android.content.ClipData$Item$Builder");
                    sAttemptedReflection = true;
                }
                if (sItemBuilder == null) return null;
                Object itemBuilderObj = sItemBuilder.newInstance();
                Method method =
                        sItemBuilder.getDeclaredMethod("setIntentSender", IntentSender.class);
                Object obj2 = method.invoke(itemBuilderObj, pendingIntent.getIntentSender());
                Method buildMethod = sItemBuilder.getDeclaredMethod("build");
                return (Item) buildMethod.invoke(obj2);
            } catch (ClassNotFoundException
                    | InstantiationException
                    | NoSuchMethodException
                    | IllegalAccessException e) {
                // Do nothing.
                Log.d(TAG, e.toString());
            } catch (InvocationTargetException e) {
                // Do nothing.
                Log.d(TAG, e.getTargetException().toString());
            }
            return null;
        }

        public static void setClipDataItemForTesting(Item item) {
            sItemForTesting = item;
            sDefinedItemForTesting = true;
            ResettersForTesting.register(
                    () -> {
                        sDefinedItemForTesting = false;
                        sItemForTesting = null;
                    });
        }
    }
}

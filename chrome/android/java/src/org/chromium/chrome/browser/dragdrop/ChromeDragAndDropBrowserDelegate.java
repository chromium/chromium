// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.content.ClipData;
import android.content.ClipData.Item;
import android.content.Intent;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.dragdrop.DragAndDropBrowserDelegate;
import org.chromium.ui.dragdrop.DragDropMetricUtils.UrlIntentSource;
import org.chromium.ui.dragdrop.DropDataAndroid;
import org.chromium.ui.dragdrop.DropDataProviderImpl;
import org.chromium.ui.dragdrop.DropDataProviderUtils;
import org.chromium.ui.util.XrUtils;

/** Delegate for browser related functions used by Drag and Drop. */
public class ChromeDragAndDropBrowserDelegate implements DragAndDropBrowserDelegate {
    private static final String TAG = "ChromeDnDDelegate";
    private static final String PARAM_CLEAR_CACHE_DELAYED_MS = "ClearCacheDelayedMs";
    @VisibleForTesting static final String PARAM_DROP_IN_CHROME = "DropInChrome";

    private static Item sItemWithPendingIntentForTesting;
    private static boolean sDefinedItemWithPendingIntentForTesting;
    private static boolean sClipDataItemBuilderNotFound;
    private final Supplier<Activity> mActivitySupplier;
    private final boolean mSupportDropInChrome;
    private final boolean mSupportAnimatedImageDragShadow;

    /**
     * @param activitySupplier The supplier to get the Activity this delegate is associated with.
     */
    public ChromeDragAndDropBrowserDelegate(Supplier<@Nullable Activity> activitySupplier) {
        mActivitySupplier = activitySupplier;
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

        if (mActivitySupplier.get() == null) {
            return null;
        }
        return mActivitySupplier.get().requestDragAndDropPermissions(dropEvent);
    }

    @Override
    public Intent createUrlIntent(String urlString, @UrlIntentSource int intentSrc) {
        Intent intent = null;
        Activity activity = mActivitySupplier.get();
        if (activity != null && MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            intent =
                    DragAndDropLauncherActivity.getLinkLauncherIntent(
                            activity,
                            urlString,
                            MultiWindowUtils.getInstanceIdForLinkIntent(activity),
                            intentSrc);
        }
        return intent;
    }

    @SuppressWarnings("UnusedVariable")
    @Override
    public ClipData buildClipData(@NonNull DropDataAndroid dropData) {
        assert dropData instanceof ChromeDropDataAndroid;
        ChromeDropDataAndroid chromeDropDataAndroid = (ChromeDropDataAndroid) dropData;

        // Dragging to create new instance.
        if (chromeDropDataAndroid.allowDragToCreateInstance) {
            ClipData clipData =
                    buildClipDataForTabOrGroupTearing(
                            chromeDropDataAndroid,
                            chromeDropDataAndroid.windowId,
                            /* destWindowId= */ TabWindowManager.INVALID_WINDOW_ID);
            if (clipData != null) return clipData;
        }

        // Dragging to existing instances.
        String text = chromeDropDataAndroid.buildTabClipDataText(mActivitySupplier.get());
        return new ClipData(null, chromeDropDataAndroid.getSupportedMimeTypes(), new Item(text));
    }

    private @Nullable ClipData buildClipDataForTabOrGroupTearing(
            ChromeDropDataAndroid chromeDropDataAndroid, int sourceWindowId, int destWindowId) {
        @Nullable
        Intent intent =
                DragAndDropLauncherActivity.buildTabOrGroupIntent(
                        chromeDropDataAndroid,
                        mActivitySupplier.get(),
                        sourceWindowId,
                        destWindowId);

        if (intent == null) return null;
        ActivityOptions opts = ActivityOptions.makeBasic();
        ApiCompatibilityUtils.setCreatorActivityOptionsBackgroundActivityStartMode(opts);
        PendingIntent pendingIntent =
                PendingIntent.getActivity(
                        mActivitySupplier.get(),
                        0,
                        intent,
                        PendingIntent.FLAG_IMMUTABLE,
                        opts.toBundle());
        String clipDataText = chromeDropDataAndroid.buildTabClipDataText(mActivitySupplier.get());
        Item item = buildClipDataItemWithPendingIntent(clipDataText, pendingIntent);
        if (item == null) item = new Item(clipDataText, intent, /* uri= */ null);
        return new ClipData(/* label= */ null, chromeDropDataAndroid.getSupportedMimeTypes(), item);
    }

    @Override
    public int buildFlags(int originalFlag, DropDataAndroid dropData) {
        assert dropData instanceof ChromeDropDataAndroid;
        ChromeDropDataAndroid chromeDropData = (ChromeDropDataAndroid) dropData;
        if (!chromeDropData.hasBrowserContent() || !chromeDropData.allowDragToCreateInstance) {
            return originalFlag;
        }
        return originalFlag
                | View.DRAG_FLAG_GLOBAL_SAME_APPLICATION
                | View.DRAG_FLAG_START_INTENT_SENDER_ON_UNHANDLED_DRAG;
    }

    @SuppressWarnings("NewApi")
    private static ClipData.Item buildClipDataItemWithPendingIntent(
            String clipDataText, PendingIntent pendingIntent) {
        if (sDefinedItemWithPendingIntentForTesting) return sItemWithPendingIntentForTesting;
        // This invocation is wrapped in a try-catch block to allow backporting of the
        // ClipData.Item.Builder() class on pre-V devices. On pre-V devices not supporting this,
        // state will be cached on the first failure to avoid subsequent invalid attempts.
        if (!sClipDataItemBuilderNotFound) {
            try {
                return new ClipData.Item.Builder()
                        .setText(clipDataText)
                        .setIntentSender(pendingIntent.getIntentSender())
                        .build();
            } catch (NoClassDefFoundError e) {
                Log.w(TAG, e.toString());
                sClipDataItemBuilderNotFound = true;
            }
        }

        // This is to handle the pending intent on a SysUI level when the drop occurs outside of the
        // source window. The API is available only on Android XR and should not be used on other
        // platforms.
        if (XrUtils.isXrDevice()) {
            Intent wrapperIntent = new Intent();
            wrapperIntent.putExtra("system_handled_intent", pendingIntent);
            return new ClipData.Item(clipDataText, wrapperIntent, /* uri= */ null);
        }

        return null;
    }

    /** Sets the ClipData.Item with a PendingIntent for testing purposes. */
    public static void setClipDataItemWithPendingIntentForTesting(Item item) {
        sItemWithPendingIntentForTesting = item;
        sDefinedItemWithPendingIntentForTesting = true;
        ResettersForTesting.register(
                () -> {
                    sDefinedItemWithPendingIntentForTesting = false;
                    sItemWithPendingIntentForTesting = null;
                });
    }
}

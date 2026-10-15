// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import android.content.ClipData;
import android.content.ClipDescription;
import android.view.DragEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/**
 * Handles links (and other droppable text) that are dragged onto the tab strip, e.g. from a web
 * page or another app. {@link TabStripDragHandler} owns the drag event flow and forwards the events
 * of link drags to this delegate.
 */
@NullMarked
class TabStripLinkDragDelegate {
    private final Supplier<StripLayoutHelper> mStripLayoutHelperSupplier;

    /**
     * @param stripLayoutHelperSupplier Supplier for the {@link StripLayoutHelper} of the strip that
     *     receives the drag.
     */
    TabStripLinkDragDelegate(Supplier<StripLayoutHelper> stripLayoutHelperSupplier) {
        mStripLayoutHelperSupplier = stripLayoutHelperSupplier;
    }

    /**
     * @param clipDescription The description of the dragged data.
     * @return Whether the drag carries data that this delegate can handle.
     */
    boolean canHandle(@Nullable ClipDescription clipDescription) {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STRIP_LINK_DRAG_DROP)
                && clipDescription != null
                && !MimeTypeUtils.clipDescriptionHasBrowserContent(clipDescription)
                && (clipDescription.hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN)
                        || clipDescription.hasMimeType(ClipDescription.MIMETYPE_TEXT_URILIST)
                        || clipDescription.hasMimeType(ClipDescription.MIMETYPE_TEXT_INTENT));
    }

    /**
     * Called when the link drag enters the tab strip.
     *
     * @param xDp The x-coordinate of the drag in dp.
     * @return Whether the event was handled.
     */
    boolean onDragEnter(float xDp) {
        mStripLayoutHelperSupplier.get().handleLinkDragEnter(xDp);
        return true;
    }

    /**
     * Called when the link drag moves within the tab strip.
     *
     * @param xDp The x-coordinate of the drag in dp.
     * @return Whether the event was handled.
     */
    boolean onDragWithin(float xDp) {
        mStripLayoutHelperSupplier.get().handleLinkDragWithin(xDp);
        return true;
    }

    /**
     * Called when the link drag leaves the tab strip or ends.
     *
     * @return Whether the event was handled.
     */
    boolean onDragExit() {
        mStripLayoutHelperSupplier.get().handleLinkDragExit();
        return true;
    }

    /**
     * Called when the link is dropped on the tab strip.
     *
     * @param dropEvent The drop event.
     * @param xDp The x-coordinate of the drop in dp.
     * @return Whether the drop was handled.
     */
    boolean onDrop(DragEvent dropEvent, float xDp) {
        GURL url = extractUrlFromDropData(dropEvent);
        if (url == null) return false;

        mStripLayoutHelperSupplier.get().handleLinkDrop(xDp, url);
        DragDropMetricUtils.recordDragDropType(
                DragDropType.LINK_TO_TAB_STRIP, /* isTabGroup= */ false, /* isMultiTab= */ false);
        return true;
    }

    /**
     * Extracts a URL from the drop event's ClipData. Tries item Intent data first, then item Uri.
     * Link drags carry the URL in one of these, so the item's text is intentionally not parsed for
     * URLs here.
     */
    static @Nullable GURL extractUrlFromDropData(DragEvent dropEvent) {
        ClipData clipData = dropEvent.getClipData();
        if (clipData == null || clipData.getItemCount() == 0) return null;
        ClipData.Item item = clipData.getItemAt(0);
        if (item == null) return null;

        if (item.getIntent() != null && item.getIntent().getData() != null) {
            String url = item.getIntent().getData().toString();
            GURL gurl = new GURL(url);
            if (isValidHttpOrHttps(gurl)) {
                return gurl;
            }
        }

        if (item.getUri() != null) {
            String url = item.getUri().toString();
            GURL gurl = new GURL(url);
            if (isValidHttpOrHttps(gurl)) {
                return gurl;
            }
        }

        return null;
    }

    private static boolean isValidHttpOrHttps(GURL gurl) {
        return gurl.isValid() && UrlUtilities.isHttpOrHttps(gurl);
    }
}

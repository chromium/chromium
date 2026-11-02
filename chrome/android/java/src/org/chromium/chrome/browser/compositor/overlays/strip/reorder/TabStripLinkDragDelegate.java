// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import android.content.ClipData;
import android.content.ClipDescription;
import android.view.DragEvent;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxViewUtil;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteMatch;
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
    /** The maximum length of dropped text that is classified like text typed into the omnibox. */
    @VisibleForTesting static final int MAX_TEXT_LENGTH_TO_CLASSIFY = 4096;

    private final Supplier<StripLayoutHelper> mStripLayoutHelperSupplier;
    private final Supplier<@Nullable Profile> mProfileSupplier;

    /**
     * @param stripLayoutHelperSupplier Supplier for the {@link StripLayoutHelper} of the strip that
     *     receives the drag.
     * @param profileSupplier Supplier for the profile of the current tab model, used to search for
     *     dropped text.
     */
    TabStripLinkDragDelegate(
            Supplier<StripLayoutHelper> stripLayoutHelperSupplier,
            Supplier<@Nullable Profile> profileSupplier) {
        mStripLayoutHelperSupplier = stripLayoutHelperSupplier;
        mProfileSupplier = profileSupplier;
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
     * Called when the link or text is dropped on the tab strip.
     *
     * @param dropEvent The drop event.
     * @param xDp The x-coordinate of the drop in dp.
     * @return Whether the drop was handled.
     */
    boolean onDrop(DragEvent dropEvent, float xDp) {
        ClipData clipData = dropEvent.getClipData();
        if (clipData == null || clipData.getItemCount() == 0) return false;
        ClipData.Item item = clipData.getItemAt(0);
        if (item == null) return false;

        GURL url = extractUrlFromItem(item);
        if (url == null) url = getUrlForDroppedText(item);
        if (url == null) return false;

        mStripLayoutHelperSupplier.get().handleLinkDrop(xDp, url);
        DragDropMetricUtils.recordDragDropType(
                DragDropType.LINK_TO_TAB_STRIP, /* isTabGroup= */ false, /* isMultiTab= */ false);
        return true;
    }

    /**
     * Extracts a URL from a dropped ClipData item. Tries the item's Intent data first, then its
     * Uri. Link drags carry the URL in one of these, so the item's text is intentionally not parsed
     * for URLs here. Dropped text is classified separately by {@link #getUrlForText(Profile,
     * String)}.
     */
    static @Nullable GURL extractUrlFromItem(ClipData.Item item) {
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

    private @Nullable GURL getUrlForDroppedText(ClipData.Item item) {
        if (item.getText() == null) return null;
        Profile profile = mProfileSupplier.get();
        if (profile == null) return null;
        // Clean up the text the same way the omnibox does for pasted text. For example, line breaks
        // in a wrapped URL are removed, while other line breaks become spaces.
        return getUrlForText(
                profile, OmniboxViewUtil.sanitizeTextForPaste(item.getText().toString()));
    }

    /**
     * Converts dropped plain text into a URL to open. The text is interpreted like text typed into
     * the omnibox, so URL-like text (e.g. "example.com/foo") is opened directly, and any other text
     * is searched for with the default search engine. Only http and https URLs are returned.
     *
     * @param profile The profile to interpret the text for.
     * @param text The dropped text, already sanitized with {@link
     *     OmniboxViewUtil#sanitizeTextForPaste(String)}.
     * @return The URL to open, or {@code null} if there is nothing to open.
     */
    static @Nullable GURL getUrlForText(Profile profile, String text) {
        String query = text.trim();
        if (query.isEmpty()) return null;

        // Classifying is expensive and runs on the UI thread, so long text, which is unlikely to be
        // a URL, is searched for without classifying it.
        if (query.length() <= MAX_TEXT_LENGTH_TO_CLASSIFY) {
            AutocompleteMatch match = AutocompleteCoordinator.classify(profile, query);
            if (match != null && isValidHttpOrHttps(match.getUrl())) return match.getUrl();
        }

        // Text that isn't interpreted as an http(s) URL (e.g. "javascript:...") is searched for
        // instead.
        GURL searchUrl =
                new GURL(
                        TemplateUrlServiceFactory.getForProfile(profile)
                                .getUrlForSearchQuery(query));
        return isValidHttpOrHttps(searchUrl) ? searchUrl : null;
    }

    private static boolean isValidHttpOrHttps(GURL gurl) {
        return gurl.isValid() && UrlUtilities.isHttpOrHttps(gurl);
    }
}

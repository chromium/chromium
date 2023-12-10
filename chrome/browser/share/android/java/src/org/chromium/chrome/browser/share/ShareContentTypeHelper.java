// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.components.browser_ui.share.ShareParams;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/** Content type used by sharing related code. */
public class ShareContentTypeHelper {
    private static final String IMAGE_TYPE = "image/";

    @VisibleForTesting
    public static final HashSet<Integer> ALL_CONTENT_TYPES_FOR_TEST =
            new HashSet<>(
                    Arrays.asList(
                            ContentType.LINK_PAGE_VISIBLE,
                            ContentType.LINK_PAGE_NOT_VISIBLE,
                            ContentType.TEXT,
                            ContentType.HIGHLIGHTED_TEXT,
                            ContentType.LINK_AND_TEXT,
                            ContentType.IMAGE,
                            ContentType.OTHER_FILE_TYPE,
                            ContentType.IMAGE_AND_LINK));

    @IntDef({
        ContentType.LINK_PAGE_VISIBLE,
        ContentType.LINK_PAGE_NOT_VISIBLE,
        ContentType.TEXT,
        ContentType.HIGHLIGHTED_TEXT,
        ContentType.LINK_AND_TEXT,
        ContentType.IMAGE,
        ContentType.OTHER_FILE_TYPE,
        ContentType.IMAGE_AND_LINK
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContentType {
        int LINK_PAGE_VISIBLE = 0;
        int LINK_PAGE_NOT_VISIBLE = 1;
        int TEXT = 2;
        int HIGHLIGHTED_TEXT = 3;
        int LINK_AND_TEXT = 4;
        int IMAGE = 5;
        int OTHER_FILE_TYPE = 6;
        int IMAGE_AND_LINK = 7;
    }

    /**
     * Returns a set of {@link ContentType}s for the current share.
     *
     * Adds {@link ContentType}s according to the following logic:
     *
     * <ul>
     *     <li>If a URL is present, {@code isUrlOfVisiblePage} determines whether to add
     *     {@link ContentType.LINK_PAGE_VISIBLE} or {@link ContentType.LINK_PAGE_NOT_VISIBLE}.
     *     <li>If the text being shared is not the same as the URL, add {@link ContentType.TEXT}
     *     <li>If text is highlighted by user, add {@link ContentType.HIGHLIGHTED_TEXT}.
     *     <li>If the share contains files and the {@code fileContentType} is an image, add
     *     {@link ContentType.IMAGE}. Otherwise, add {@link ContentType.OTHER_FILE_TYPE}.
     * </ul>
     */
    public static Set<Integer> getContentTypes(
            ShareParams params, ChromeShareExtras chromeShareExtras) {
        Set<Integer> contentTypes = new HashSet<>();
        boolean hasUrl = !TextUtils.isEmpty(params.getUrl());
        if (hasUrl && !chromeShareExtras.skipPageSharingActions()) {
            if (chromeShareExtras.isUrlOfVisiblePage()) {
                contentTypes.add(ContentType.LINK_PAGE_VISIBLE);
            } else {
                contentTypes.add(ContentType.LINK_PAGE_NOT_VISIBLE);
            }
        }
        if (!TextUtils.isEmpty(params.getText())) {
            if (chromeShareExtras.getDetailedContentType()
                    == DetailedContentType.HIGHLIGHTED_TEXT) {
                contentTypes.add(ContentType.HIGHLIGHTED_TEXT);
            } else {
                contentTypes.add(ContentType.TEXT);
            }
        }
        if (hasUrl && !TextUtils.isEmpty(params.getText())) {
            contentTypes.add(ContentType.LINK_AND_TEXT);
        }
        if (params.getFileUris() != null || params.getImageUriToShare() != null) {
            if (!TextUtils.isEmpty(params.getFileContentType())
                    && params.getFileContentType().startsWith(IMAGE_TYPE)) {
                if (hasUrl) {
                    contentTypes.add(ContentType.IMAGE_AND_LINK);
                } else {
                    contentTypes.add(ContentType.IMAGE);
                }
            } else {
                contentTypes.add(ContentType.OTHER_FILE_TYPE);
            }
        }
        return contentTypes;
    }
}

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Intent;
import android.net.Uri;

import androidx.core.util.Pair;

import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemShareInfo;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

/** Helper class containing utility methods to make sharing {@link OfflineItem}s easier. */
public class ShareUtils {
    private static final String DEFAULT_MIME_TYPE = "*/*";
    private static final String MIME_TYPE_DELIMITER = "/";

    private ShareUtils() {}

    /**
     * Creates an {@link Intent} that represents
     * @param items A {@link Collection} of pairs of {@link OfflineItem}s and
     *              {@link OfflineItemShareInfo}s.  The {@link OfflineItemShareInfo} contains extra
     *              information about the specific {@link OfflineItem} required to properly build
     *              the share {@link Intent}.
     * @return      An {@link Intent} that can be sent through the Android framework that will share
     *              {@code items} properly.  This might include specific {@link URI}s or it might
     *              include text URLs depending on whether or not the item can be exposed and shared
     *              directly.
     * @see         OfflineContentProvider#getShareInfoForItem(ContentId, ShareCallback)
     */
    public static Intent createIntent(Collection<Pair<OfflineItem, OfflineItemShareInfo>> items) {
        ArrayList<Uri> uris = new ArrayList<>();
        Set<String> mimeTypes = new HashSet<>();
        StringBuilder urls = new StringBuilder();

        for (Pair<OfflineItem, OfflineItemShareInfo> item : items) {
            mimeTypes.add(Intent.normalizeMimeType(item.first.mimeType));

            Uri uri = item.second == null ? null : item.second.uri;
            if (uri != null && uri.compareTo(Uri.EMPTY) != 0) {
                uris.add(uri);
            } else if (item.first.url != null && !item.first.url.isEmpty()) {
                if (urls.length() > 0) urls.append("\n");
                urls.append(item.first.url.getSpec());
            }
        }

        // If we have nothing to share, don't create the intent.  We should theoretically always
        // have the url, but this is a safety precaution.  In the future we could just use the
        // title instead of the URL if the URL doesn't exist.
        if (uris.isEmpty() && urls.length() == 0) return null;

        boolean sendingText = urls.length() > 0;
        boolean singleItem = ((sendingText ? 1 : 0) + uris.size()) == 1;

        Intent intent = new Intent();

        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setType(Intent.normalizeMimeType(resolveMimeType(mimeTypes)));
        intent.setAction(singleItem ? Intent.ACTION_SEND : Intent.ACTION_SEND_MULTIPLE);

        if (sendingText) intent.putExtra(Intent.EXTRA_TEXT, urls.toString());

        if (items.size() == 1) {
            intent.putExtra(Intent.EXTRA_SUBJECT, items.iterator().next().first.title);
        }

        if (uris.size() == 1) {
            intent.putExtra(Intent.EXTRA_STREAM, uris.get(0));
        } else if (uris.size() > 1) {
            intent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris);
        }

        return intent;
    }

    private static String resolveMimeType(Collection<String> mimeTypes) {
        if (mimeTypes.isEmpty()) return DEFAULT_MIME_TYPE;
        if (mimeTypes.size() == 1) return mimeTypes.iterator().next();

        Set<String> firstParts = new HashSet<>();
        Set<String> secondParts = new HashSet<>();

        for (String mimeType : mimeTypes) {
            String[] parts = mimeType.split(MIME_TYPE_DELIMITER);
            firstParts.add(parts[0]);
            secondParts.add(parts[1]);
        }

        if (firstParts.size() == 1) {
            String secondPart = secondParts.size() > 1 ? "*" : secondParts.iterator().next();
            return firstParts.iterator().next() + MIME_TYPE_DELIMITER + secondPart;
        } else {
            return DEFAULT_MIME_TYPE;
        }
    }
}

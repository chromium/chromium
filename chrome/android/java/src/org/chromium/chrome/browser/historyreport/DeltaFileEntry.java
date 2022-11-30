// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.historyreport;

/**
 * Represents entry in delta file. Each entry is a log of action that happened to history data. It
 * can be addition or deletion of an URL.
 */
public class DeltaFileEntry {
    /**
     * Unique id of this entry. Order of two entries can be determined by comparing their seqNos.
     * Smaller happened first.
     */
    public final long seqNo;
    /**
     * Type of the action represented by this entry. Can be 'add' or 'del'.
     */
    public final String type;
    /**
     * ID which identifies the URL targeted by the action represented by this entry.
     * It's shorter than 257 characters.
     */
    public final String id;
    /**
     * URL targeted by the action represented by this entry.
     */
    public final String url;
    /**
     * Score of the URL targeted by the action represented by this entry.
     * It's used in search ranking.
     */
    public final int score;
    /**
     * Title of the URL targeted by the action represented by this entry.
     */
    public final String title;

    /**
     * Part of URL which will be used as a search key in index.
     */
    public final String indexedUrl;

    public DeltaFileEntry(long seqNo, String type, String id, String url, int score, String title,
            String indexedUrl) {
        this.seqNo = seqNo;
        this.type = type;
        this.id = id;
        this.url = url;
        this.score = score;
        this.title = title;
        this.indexedUrl = indexedUrl;
    }

    @Override
    public String toString() {
        return "DeltaFileEntry[" + seqNo + ", " + type + ", " + id + ", " + url + ", " + title
                + "]";
    }
}

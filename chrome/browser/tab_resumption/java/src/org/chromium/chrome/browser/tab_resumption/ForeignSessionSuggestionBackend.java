// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** A SuggestionBackend backed by ForeignSessionHelper. */
public class ForeignSessionSuggestionBackend implements SuggestionBackend {
    private final ForeignSessionHelper mForeignSessionHelper;

    public ForeignSessionSuggestionBackend(ForeignSessionHelper foreignSessionHelper) {
        mForeignSessionHelper = foreignSessionHelper;
    }

    /** Implements {@link SuggestionBackend} */
    @Override
    public void destroy() {
        mForeignSessionHelper.destroy();
    }

    /** Implements {@link SuggestionBackend} */
    @Override
    public void triggerUpdate() {
        mForeignSessionHelper.triggerSessionSync();
    }

    /** Implements {@link SuggestionBackend} */
    @Override
    public void setUpdateObserver(Runnable listener) {
        mForeignSessionHelper.setOnForeignSessionCallback(() -> listener.run());
    }

    /** Implements {@link SuggestionBackend} */
    @Override
    public void readCached(Callback<List<SuggestionEntry>> callback) {
        List<SuggestionEntry> suggestions = new ArrayList<SuggestionEntry>();

        long currentTimeMs = getCurrentTimeMs();
        List<ForeignSession> foreignSessions = mForeignSessionHelper.getForeignSessions();
        for (ForeignSession session : foreignSessions) {
            for (ForeignSessionWindow window : session.windows) {
                for (ForeignSessionTab tab : window.tabs) {
                    if (isForeignSessionTabUsable(tab)
                            && currentTimeMs - tab.lastActiveTime <= STALENESS_THRESHOLD_MS) {
                        suggestions.add(
                                new SuggestionEntry(
                                        session.name,
                                        tab.url,
                                        tab.title,
                                        tab.lastActiveTime,
                                        tab.id));
                    }
                }
            }
        }
        Collections.sort(suggestions);
        callback.onResult(suggestions);
    }

    /** Returns the current time in ms since the epoch. */
    long getCurrentTimeMs() {
        return System.currentTimeMillis();
    }

    private boolean isForeignSessionTabUsable(ForeignSessionTab tab) {
        String scheme = tab.url.getScheme();
        return scheme.equals(UrlConstants.HTTP_SCHEME) || scheme.equals(UrlConstants.HTTPS_SCHEME);
    }
}

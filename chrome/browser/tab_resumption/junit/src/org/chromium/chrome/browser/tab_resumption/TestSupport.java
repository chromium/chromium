// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.junit.Assert;

import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Static constants and helpers shared across Tab Resumption Module unit tests. */
public class TestSupport {
    static final long BASE_TIME_MS = 1705000000000L; // 2024-01-11, 14:06:40 EST.

    // Injected "current time" for each test, set at 1 day after `BASE_TIME_MS`.
    static final long CURRENT_TIME_MS = makeTimestamp(24, 0, 0);

    static final ForeignSessionTab TAB1 =
            new ForeignSessionTab(
                    JUnitTestGURLs.BLUE_1,
                    "Blue 1",
                    makeTimestamp(3, 0, 0),
                    makeTimestamp(3, 0, 0),
                    101);
    // This one is stale.
    static final ForeignSessionTab TAB2 =
            new ForeignSessionTab(
                    JUnitTestGURLs.GOOGLE_URL_DOG,
                    "Google Dog",
                    makeTimestamp(0, 0, -1),
                    makeTimestamp(0, 0, -1),
                    102);
    static final ForeignSessionTab TAB3 =
            new ForeignSessionTab(
                    JUnitTestGURLs.CHROME_ABOUT,
                    "About",
                    makeTimestamp(0, 0, -1), // timestamp != lastUpdatedTime.
                    makeTimestamp(7, 0, 0),
                    103);
    static final ForeignSessionTab TAB4 =
            new ForeignSessionTab(
                    JUnitTestGURLs.URL_1,
                    "One",
                    makeTimestamp(0, 30, 0),
                    makeTimestamp(0, 30, 0),
                    104);
    static final ForeignSessionTab TAB5 =
            new ForeignSessionTab(
                    JUnitTestGURLs.MAPS_URL,
                    "Maps",
                    makeTimestamp(4, 0, 0),
                    makeTimestamp(4, 0, 0),
                    105);
    // This one is the most recent.
    static final ForeignSessionTab TAB6 =
            new ForeignSessionTab(
                    JUnitTestGURLs.INITIAL_URL,
                    "Initial",
                    makeTimestamp(2, 0, 0), // timestamp != lastUpdatedTime.
                    makeTimestamp(8, 0, 0),
                    106);
    static final ForeignSessionTab TAB7 =
            new ForeignSessionTab(
                    JUnitTestGURLs.HTTP_URL,
                    "Old HTTP",
                    makeTimestamp(3, 0, 0),
                    makeTimestamp(3, 0, 0),
                    107);

    /** Makes a test bitmap with specified dimensions. */
    static Bitmap makeBitmap(int width, int height) {
        return Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
    }

    /**
     * Makes a test timestamp in ms since the epoch relative to `BASE_TIME_MS`.
     *
     * @param hours Relative hours, may be negative.
     * @param minutes Relative minutes, may be negative.
     * @param seconds Relative seconds, may be negative.
     */
    static long makeTimestamp(int hours, int minutes, int seconds) {
        return BASE_TIME_MS + ((hours * 60L + minutes) * 60L + seconds) * 1000L;
    }

    /** Makes a list of fixed ForeignSession test data. */
    static List<ForeignSession> makeForeignSessionsA() {
        ForeignSessionWindow desktopWindow1 =
                new ForeignSessionWindow(
                        /* timestamp= */ makeTimestamp(3, 30, 0),
                        /* sessionId= */ 201,
                        /* tabs= */ new ArrayList<>(Arrays.asList(TAB1, TAB2)));
        ForeignSessionWindow desktopWindow2 =
                new ForeignSessionWindow(
                        /* timestamp= */ makeTimestamp(9, 15, 20),
                        /* sessionId= */ 202,
                        /* tabs= */ new ArrayList<>(Arrays.asList(TAB3, TAB4)));
        ForeignSession desktopForeignSession =
                new ForeignSession(
                        /* tag= */ "TagForDesktop",
                        /* name= */ "My Desktop",
                        /* modifiedTime= */ makeTimestamp(10, 0, 0),
                        /* windows= */ new ArrayList<>(
                                Arrays.asList(desktopWindow1, desktopWindow2)),
                        /* formFactor= */ FormFactor.DESKTOP);

        ForeignSessionWindow tabletWindow1 =
                new ForeignSessionWindow(
                        /* timestamp= */ makeTimestamp(8, 1, 15),
                        /* sessionId= */ 301,
                        /* tabs= */ new ArrayList<>(Arrays.asList(TAB5, TAB6, TAB7)));
        ForeignSession tabletForeignSession =
                new ForeignSession(
                        /* tag= */ "TagForTablet",
                        /* name= */ "My Tablet",
                        /* modifiedTime= */ makeTimestamp(8, 5, 20),
                        /* windows= */ new ArrayList<>(Arrays.asList(tabletWindow1)),
                        /* formFactor= */ FormFactor.TABLET);

        return new ArrayList<>(Arrays.asList(desktopForeignSession, tabletForeignSession));
    }

    /** Makes a shorter, altered list of fixed ForeignSession test data. */
    static List<ForeignSession> makeForeignSessionsB() {
        ForeignSessionWindow tabletWindow1 =
                new ForeignSessionWindow(
                        /* timestamp= */ makeTimestamp(9, 1, 15),
                        /* sessionId= */ 301,
                        /* tabs= */ new ArrayList<>(Arrays.asList(TAB5, TAB7)));
        ForeignSession tabletForeignSession =
                new ForeignSession(
                        /* tag= */ "TagForTablet",
                        /* name= */ "My Tablet",
                        /* modifiedTime= */ makeTimestamp(9, 5, 20),
                        /* windows= */ new ArrayList<>(Arrays.asList(tabletWindow1)),
                        /* formFactor= */ FormFactor.TABLET);

        return new ArrayList<>(Arrays.asList(tabletForeignSession));
    }

    /** Makes a list of sorted SuggestionEntry derived from makeForeignSessionsA(). */
    static List<SuggestionEntry> makeForeignSessionSuggestionsA() {
        // There are 7 tabs total, but TAB3 is invalid, and TAB2 is stale, resulting in 5:
        // TAB6 < TAB5 < TAB1 "My Desktop" < TAB7 "My Tablet" < TAB4.
        List<SuggestionEntry> suggestions = new ArrayList<SuggestionEntry>();
        suggestions.add(SuggestionEntry.createFromForeignSessionTab("My Tablet", TAB6));
        suggestions.add(SuggestionEntry.createFromForeignSessionTab("My Tablet", TAB5));
        suggestions.add(SuggestionEntry.createFromForeignSessionTab("My Desktop", TAB1));
        suggestions.add(SuggestionEntry.createFromForeignSessionTab("My Tablet", TAB7));
        suggestions.add(SuggestionEntry.createFromForeignSessionTab("My Desktop", TAB4));
        return suggestions;
    }

    /** Makes a list of sorted SuggestionEntry derived from makeForeignSessionsB(). */
    static List<SuggestionEntry> makeForeignSessionSuggestionsB() {
        // Only TAB5 and TAB7 are open, and they got selected. TAB5 < TAB7.
        List<SuggestionEntry> suggestions = new ArrayList<SuggestionEntry>();
        suggestions.add(SuggestionEntry.createFromForeignSessionTab("My Tablet", TAB5));
        suggestions.add(SuggestionEntry.createFromForeignSessionTab("My Tablet", TAB7));
        return suggestions;
    }

    static SuggestionEntry makeSyncDerivedSuggestion(int index) {
        assert index == 0 || index == 1;
        GURL[] urlChoices = {JUnitTestGURLs.GOOGLE_URL_DOG, JUnitTestGURLs.GOOGLE_URL_CAT};
        String[] titleChoices = {"Google Dog", "Google Cat"};
        return SuggestionEntry.createFromForeignFields(
                /* sourceName= */ "Desktop",
                /* url= */ urlChoices[index],
                /* title= */ titleChoices[index],
                /* timestamp= */ makeTimestamp(16, 0, 0));
    }

    static Tab makeMockBrowserTab() {
        Tab tab = mock(Tab.class);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.BLUE_1);
        when(tab.getTitle()).thenReturn("Blue 1");
        when(tab.getTimestampMillis()).thenReturn(BASE_TIME_MS);
        when(tab.getId()).thenReturn(42);
        return tab;
    }

    /** Asserts that a List<SuggestionEntry> is empty but not null. */
    static void assertEmptySuggestions(@Nullable List<SuggestionEntry> suggestions) {
        Assert.assertNotNull(suggestions);
        Assert.assertEquals(0, suggestions.size());
    }

    /** Asserts that two List<SuggestionEntry> contain identical data. */
    static void assertSuggestionsEqual(
            @NonNull List<SuggestionEntry> expectedSuggestions,
            @Nullable List<SuggestionEntry> suggestions) {
        Assert.assertNotNull(suggestions);
        int n = expectedSuggestions.size();
        Assert.assertEquals(n, suggestions.size());
        for (int i = 0; i < n; ++i) {
            SuggestionEntry expectedEntry = expectedSuggestions.get(i);
            SuggestionEntry entry = suggestions.get(i);
            Assert.assertEquals(expectedEntry.sourceName, entry.sourceName);
            Assert.assertEquals(expectedEntry.url, entry.url);
            Assert.assertEquals(expectedEntry.title, entry.title);
            Assert.assertEquals(expectedEntry.lastActiveTime, entry.lastActiveTime);
            Assert.assertEquals(expectedEntry.getLocalTabId(), entry.getLocalTabId());
        }
    }
}

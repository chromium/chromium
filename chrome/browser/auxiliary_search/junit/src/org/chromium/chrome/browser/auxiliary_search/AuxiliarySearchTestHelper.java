// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Utility helper class for Auxiliary search tests. */
public class AuxiliarySearchTestHelper {
    static final int TAB_ID_1 = 1;
    static final int TAB_ID_2 = 2;
    static final int VISIT_ID_1 = 3;
    static final int VISIT_ID_2 = 4;
    static final int SCORE_1 = 111;
    static final int SCORE_2 = 112;
    static final String TITLE_1 = "Title 1";
    static final String TITLE_2 = "Title 2";
    static final String APP_ID_1 = "App Id 1";
    static final String APP_ID_2 = "App Id 2";

    public static List<AuxiliarySearchEntry> createAuxiliarySearchEntries(long timestamp) {
        List<AuxiliarySearchEntry> entries = new ArrayList<>();
        entries.add(
                AuxiliarySearchProvider.createAuxiliarySearchEntry(
                        TAB_ID_1, TITLE_1, JUnitTestGURLs.URL_1.getSpec(), timestamp));
        entries.add(
                AuxiliarySearchProvider.createAuxiliarySearchEntry(
                        TAB_ID_2, TITLE_2, JUnitTestGURLs.URL_2.getSpec(), timestamp));
        return entries;
    }

    public static List<AuxiliarySearchDataEntry> createAuxiliarySearchDataEntries(long timestamp) {
        List<AuxiliarySearchDataEntry> entries = new ArrayList<>();
        entries.add(
                new AuxiliarySearchDataEntry(
                        /* type= */ AuxiliarySearchEntryType.TAB,
                        /* url= */ JUnitTestGURLs.URL_1,
                        /* title= */ TITLE_1,
                        /* lastActiveTime= */ timestamp,
                        /* tabId= */ TAB_ID_1,
                        /* appId= */ null,
                        /* visitId= */ -1,
                        /* score= */ 0));
        entries.add(
                new AuxiliarySearchDataEntry(
                        /* type= */ AuxiliarySearchEntryType.TAB,
                        /* url= */ JUnitTestGURLs.URL_2,
                        /* title= */ TITLE_2,
                        /* lastActiveTime= */ timestamp,
                        /* tabId= */ TAB_ID_2,
                        /* appId= */ null,
                        /* visitId= */ -1,
                        /* score= */ 0));
        return entries;
    }

    public static List<AuxiliarySearchDataEntry> createAuxiliarySearchDataEntries_TopSite(
            long timestamp) {
        return createAuxiliarySearchDataEntries_TopSite(
                JUnitTestGURLs.URL_1, JUnitTestGURLs.URL_2, timestamp);
    }

    public static List<AuxiliarySearchDataEntry> createAuxiliarySearchDataEntries_TopSite(
            GURL url1, GURL url2, long timestamp) {
        List<AuxiliarySearchDataEntry> entries = new ArrayList<>();
        entries.add(
                new AuxiliarySearchDataEntry(
                        /* type= */ AuxiliarySearchEntryType.TOP_SITE,
                        /* url= */ url1,
                        /* title= */ TITLE_1,
                        /* lastActiveTime= */ timestamp,
                        /* tabId= */ Tab.INVALID_TAB_ID,
                        /* appId= */ null,
                        /* visitId= */ VISIT_ID_1,
                        /* score= */ SCORE_1));
        entries.add(
                new AuxiliarySearchDataEntry(
                        /* type= */ AuxiliarySearchEntryType.TOP_SITE,
                        /* url= */ url2,
                        /* title= */ TITLE_2,
                        /* lastActiveTime= */ timestamp,
                        /* tabId= */ Tab.INVALID_TAB_ID,
                        /* appId= */ null,
                        /* visitId= */ VISIT_ID_2,
                        /* score= */ SCORE_2));
        return entries;
    }

    public static List<AuxiliarySearchDataEntry> createAuxiliarySearchDataEntries_CustomTabs(
            long timestamp) {
        List<AuxiliarySearchDataEntry> entries = new ArrayList<>();
        entries.add(
                new AuxiliarySearchDataEntry(
                        /* type= */ AuxiliarySearchEntryType.CUSTOM_TAB,
                        /* url= */ JUnitTestGURLs.URL_1,
                        /* title= */ TITLE_1,
                        /* lastActiveTime= */ timestamp,
                        /* tabId= */ Tab.INVALID_TAB_ID,
                        /* appId= */ APP_ID_1,
                        /* visitId= */ VISIT_ID_1,
                        /* score= */ 0));
        entries.add(
                new AuxiliarySearchDataEntry(
                        /* type= */ AuxiliarySearchEntryType.CUSTOM_TAB,
                        /* url= */ JUnitTestGURLs.URL_2,
                        /* title= */ TITLE_2,
                        /* lastActiveTime= */ timestamp,
                        /* tabId= */ Tab.INVALID_TAB_ID,
                        /* appId= */ "",
                        /* visitId= */ VISIT_ID_2,
                        /* score= */ 0));
        entries.add(
                new AuxiliarySearchDataEntry(
                        /* type= */ AuxiliarySearchEntryType.CUSTOM_TAB,
                        /* url= */ JUnitTestGURLs.URL_2,
                        /* title= */ TITLE_2,
                        /* lastActiveTime= */ timestamp,
                        /* tabId= */ Tab.INVALID_TAB_ID,
                        /* appId= */ APP_ID_2,
                        /* visitId= */ VISIT_ID_2,
                        /* score= */ 0));
        return entries;
    }
}

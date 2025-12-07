// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;
import android.util.Base64;

import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;

/**
 * Creates and fills up a directory with data suitable for testing code related to {@link TabModel}.
 * Used for mocking out the real data directory.
 *
 * <p>Instead of storing the TabState and metadata files in the test data directories, this class
 * stores them as encoded Base64 strings and then writes them out to storage during the test. This
 * gets around an infrastructure bug with setting file permissions.
 */
public class TestTabModelDirectory {
    private static final String TAG = "tabmodel";

    /**
     * Information about an encoded TabState file. Although the Tab ID is _not_ encoded in the
     * TabState file, it is convenient to store it in this class for testing purposes.
     */
    public static final class TabStateInfo {
        public final int version;
        public final int tabId;
        public final String url;
        public final String title;
        public final String encodedTabState;
        public final String filename;

        public TabStateInfo(
                boolean incognito,
                boolean flatbuffer,
                int version,
                int tabId,
                String url,
                String title,
                String encodedTabState) {
            this.version = version;
            this.tabId = tabId;
            this.url = url;
            this.title = title;
            this.encodedTabState = encodedTabState;
            this.filename =
                    (flatbuffer ? "flatbufferv1_" : "")
                            + (incognito ? "cryptonito" : "tab")
                            + tabId;
        }
    }

    /** Information about a TabModel and all of the TabStates it needs to be restored. */
    public static final class TabModelMetaDataInfo {
        public final int selectedTabId;
        public final TabStateInfo[] contents;
        public final int numRegularTabs;
        public final int numIncognitoTabs;
        public final String encodedFile;

        TabModelMetaDataInfo(
                int version,
                int numIncognitoTabs,
                int selectedTabId,
                TabStateInfo[] contents,
                String encodedFile) {
            this.numRegularTabs = contents.length - numIncognitoTabs;
            this.numIncognitoTabs = numIncognitoTabs;
            this.selectedTabId = selectedTabId;
            this.contents = contents;
            this.encodedFile = encodedFile;
        }
    }

    /** Contains information about an V2 flat buffer NTP. */
    public static final TabStateInfo V2_NTP_FBS =
            new TabStateInfo(
                    false,
                    true,
                    2,
                    0,
                    "chrome-native://newtab/",
                    "New tab",
                    "HAAAABgAPAA4AAAALAAoACQAAAAgAAAAFAAEABgAAAAAAAAAAAAAAAAAAAAAAAAAKcKThpkBAAAA"
                        + "AAAACwAAABgAAAAcAAAAlHyThpkBAAAAAAAA/////wEAAAAgAAAA1AcAANAHAAAAAAAAAgAAAAAA"
                        + "AAA0AwAAMAMAAAAAAAAXAAAAY2hyb21lLW5hdGl2ZTovL25ld3RhYi8ABwAAAE4AZQB3ACAAdABh"
                        + "AGIAAACUAgAAkAIAACEAAACIAgAAGAAAAAAAAAAQAAAAAAAAABAAAAAAAAAACAAAAAAAAACQAAAA"
                        + "BgAAAIgAAAAAAAAAAAAAAAAAAADAAAAAAAAAAAAAAAAAAAAAyAAAAAAAAAAAAAAABgAAAMAAAAAA"
                        + "AAAAA9nJl7U/BgAE2cmXtT8GADABAAAAAAAASAEAAAAAAAAAAAAAAAAAAEABAAAAAAAAmAEAAAAA"
                        + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAgAAAAAAAAANgAAABcAAABjAGgAcgBv"
                        + "AG0AZQAtAG4AYQB0AGkAdgBlADoALwAvAG4AZQB3AHQAYQBiAC8AAAAQAAAAAAAAAAgAAAAAAAAA"
                        + "CAAAAAAAAAAIAAAAAAAAADgAAAABAAAAMAAAAAAAAAA4AAAAAAAAAAAAAMCcgtc/OAAAAAAAAABQ"
                        + "AAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAgA"
                        + "AAAAAAAAEAAAAAQAAABoAHQAbQBsABAAAAAAAAAAAAAAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAA"
                        + "AAAAAAAAAAAAAAAAAAgAAAAAAAAAEAAAAAAAAAAIAAAAAAAAAFAAAAAkAAAAYwAxADcAMQBmAGEA"
                        + "NgBhAC0AOAAzADgAYgAtADQAMgAwADMALQBiADAAZgAzAC0AOAA5ADMAMQBmADIAZgA2ADAANwAw"
                        + "ADMAEAAAAAAAAAAIAAAAAAAAAFAAAAAkAAAAMwAxAGIAYQAyADEAYQA3AC0ANwA2AGQANwAtADQA"
                        + "NAA5ADMALQA4ADQAZAAzAC0ANAAyAGIANQA3ADEANgAyAGYAZQA0AGMABgAAAQAAAAAAAAAAAgAA"
                        + "ABcAAABjaHJvbWUtbmF0aXZlOi8vbmV3dGFiLwAAAAAAHMmz+UueLwAAAAAAAAAAAAYAAAAAAAAA"
                        + "Y+5R4EueLwD//////////2PuUeBLni8AAAAAAIgEAACEBAAAAQAAABcAAABodHRwczovL3d3dy5n"
                        + "b29nbGUuY29tLwAOAAAAdwB3AHcALgBnAG8AbwBnAGwAZQAuAGMAbwBtANwDAADYAwAAIQAAANAD"
                        + "AAAYAAAAAAAAABAAAAAAAAAAEAAAAAAAAAAIAAAAAAAAAJAAAAAGAAAAiAAAAAAAAAAAAAAAAAAA"
                        + "AMAAAAAAAAAAAAAAAAAAAADIAAAAAAAAAAAAAAAGAAAAOAIAAAAAAADHlOSYtT8GAMiU5Ji1PwYA"
                        + "eAIAAAAAAACQAgAAAAAAAAAAAAAAAAAAiAIAAAAAAADgAgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                        + "AAAAAAAAABAAAAAAAAAACAAAAAAAAAA2AAAAFwAAAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcA"
                        + "bwBvAGcAbABlAC4AYwBvAG0ALwAAABAAAAAAAAAACAAAAAAAAAAIAAAAAAAAAEAAAAAHAAAAOAAA"
                        + "AAAAAACoAAAAAAAAAMgAAAAAAAAA4AAAAAAAAADwAAAAAAAAABABAAAAAAAAKAEAAAAAAAAQAAAA"
                        + "AAAAAAgAAAAAAAAAaAAAADAAAAAKAA0APwAlACAAQgBsAGkAbgBrACAAcwBlAHIAaQBhAGwAaQB6"
                        + "AGUAZAAgAGYAbwByAG0AIABzAHQAYQB0AGUAIAB2AGUAcgBzAGkAbwBuACAAMQAwACAACgANAD0A"
                        + "JgAQAAAAAAAAAAgAAAAAAAAAGAAAAAgAAABOAG8AIABvAHcAbgBlAHIAEAAAAAAAAAAIAAAAAAAA"
                        + "AAoAAAABAAAAMQAAAAAAAAAQAAAAAAAAAAgAAAAAAAAACAAAAAAAAAAQAAAAAAAAAAgAAAAAAAAA"
                        + "GAAAAAgAAABjAGgAZQBjAGsAYgBvAHgAEAAAAAAAAAAIAAAAAAAAAAoAAAABAAAAMQAAAAAAAAAQ"
                        + "AAAAAAAAAAgAAAAAAAAADAAAAAIAAABvAG4AAAAAADgAAAABAAAAMAAAAAAAAAA4AAAAAAAAAAAA"
                        + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAA"
                        + "AAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAAAAAAAAAEAAAAAAAAAAIAAAA"
                        + "AAAAAFAAAAAkAAAAOABmADcANgBjAGYANAA5AC0AZQBkADMAMAAtADQAMQA5ADUALQBiADIAOABh"
                        + "AC0AYwA4ADQAOQA5AGEANQBjAGIANAAxADcAEAAAAAAAAAAIAAAAAAAAAFAAAAAkAAAAMgA2ADEA"
                        + "NwBjADgAMgAwAC0ANQBkAGUAMwAtADQANAAxAGYALQBhADgANABjAC0AOABmAGEAZgAzADIAMgA0"
                        + "ADAAMwBhAGEAAQAAAgAAAAAAAAAAAgAAABYAAABodHRwOi8vd3d3Lmdvb2dsZS5jb20vAAAAAAAA"
                        + "CMdr4UueLwAAAAAAAAAAAAYAAAAAAAAACMdr4UueLwD//////////wjHa+FLni8AAAAAAA==");

    public static final TabStateInfo V2_GOOGLE_COM_FBS =
            new TabStateInfo(
                    false,
                    true,
                    2,
                    1,
                    "https://www.google.com/",
                    "www.google.com",
                    "HAAAABgAPAA4AAAALAAoACQAAAAgAAAAFAAEABgAAAAAAAAAAAAAAAAAAAAAAAAA1SYMh5kBAAAA"
                        + "AAAACwAAABgAAAAcAAAAlHyThpkBAAAAAAAA/////wEAAAAgAAAAXAYAAFgGAAAAAAAAAgAAAAEA"
                        + "AAA0AwAAMAMAAAAAAAAXAAAAY2hyb21lLW5hdGl2ZTovL25ld3RhYi8ABwAAAE4AZQB3ACAAdABh"
                        + "AGIAAACUAgAAkAIAACEAAACIAgAAGAAAAAAAAAAQAAAAAAAAABAAAAAAAAAACAAAAAAAAACQAAAA"
                        + "BgAAAIgAAAAAAAAAAAAAAAAAAADAAAAAAAAAAAAAAAAAAAAAyAAAAAAAAAAAAAAABgAAAMAAAAAA"
                        + "AAAAA9nJl7U/BgAE2cmXtT8GADABAAAAAAAASAEAAAAAAAAAAAAAAAAAAEABAAAAAAAAmAEAAAAA"
                        + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAgAAAAAAAAANgAAABcAAABjAGgAcgBv"
                        + "AG0AZQAtAG4AYQB0AGkAdgBlADoALwAvAG4AZQB3AHQAYQBiAC8AAAAQAAAAAAAAAAgAAAAAAAAA"
                        + "CAAAAAAAAAAIAAAAAAAAADgAAAABAAAAMAAAAAAAAAA4AAAAAAAAAAAAAMCcgtc/OAAAAAAAAABQ"
                        + "AAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAgA"
                        + "AAAAAAAAEAAAAAQAAABoAHQAbQBsABAAAAAAAAAAAAAAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAA"
                        + "AAAAAAAAAAAAAAAAAAgAAAAAAAAAEAAAAAAAAAAIAAAAAAAAAFAAAAAkAAAAYwAxADcAMQBmAGEA"
                        + "NgBhAC0AOAAzADgAYgAtADQAMgAwADMALQBiADAAZgAzAC0AOAA5ADMAMQBmADIAZgA2ADAANwAw"
                        + "ADMAEAAAAAAAAAAIAAAAAAAAAFAAAAAkAAAAMwAxAGIAYQAyADEAYQA3AC0ANwA2AGQANwAtADQA"
                        + "NAA5ADMALQA4ADQAZAAzAC0ANAAyAGIANQA3ADEANgAyAGYAZQA0AGMABgAAAQAAAAAAAAAAAgAA"
                        + "ABcAAABjaHJvbWUtbmF0aXZlOi8vbmV3dGFiLwAAAAAAfLyKz02eLwAAAAAAAAAAAAYAAAAAAAAA"
                        + "Y+5R4EueLwD//////////2PuUeBLni8AAAAAABADAAAMAwAAAQAAABcAAABodHRwczovL3d3dy5n"
                        + "b29nbGUuY29tLwAOAAAAdwB3AHcALgBnAG8AbwBnAGwAZQAuAGMAbwBtAGQCAABgAgAAIQAAAFgC"
                        + "AAAYAAAAAAAAABAAAAAAAAAAEAAAAAAAAAAIAAAAAAAAAJAAAAAGAAAAiAAAAAAAAAAAAAAAAAAA"
                        + "AMAAAAAAAAAAAAAAAAAAAADIAAAAAAAAAAAAAAAGAAAAwAAAAAAAAADDy3aHtz8GAMTLdoe3PwYA"
                        + "AAEAAAAAAAAYAQAAAAAAAAAAAAAAAAAAEAEAAAAAAABoAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                        + "AAAAAAAAABAAAAAAAAAACAAAAAAAAAA2AAAAFwAAAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcA"
                        + "bwBvAGcAbABlAC4AYwBvAG0ALwAAABAAAAAAAAAACAAAAAAAAAAIAAAAAAAAAAgAAAAAAAAAOAAA"
                        + "AAEAAAAwAAAAAAAAADgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAA"
                        + "AAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                        + "AAAACAAAAAAAAAAQAAAAAAAAAAgAAAAAAAAAUAAAACQAAAAyADMAYwA2AGMANAAyADAALQBlADcA"
                        + "NwA5AC0ANAAwADMAZQAtAGEAOQAxADQALQA2ADYANAA4ADgAOQA4ADkANQAxADEAOAAQAAAAAAAA"
                        + "AAgAAAAAAAAAUAAAACQAAABiADUANQA3ADQANwA1ADAALQBhADIAYwBkAC0ANABjADgAZAAtADgA"
                        + "ZQBjADMALQAwAGUANwA4AGYAMgAzADgAZABlAGEAMAABAAACAAAAAAAAAAACAAAAFgAAAGh0dHA6"
                        + "Ly93d3cuZ29vZ2xlLmNvbS8AAAAAAAApcf3PTZ4vAAAAAAAAAAAABgAAAAAAAAApcf3PTZ4vAP//"
                        + "////////KXH9z02eLwAAAAAA");

    public static final TabStateInfo V2_GOOGLE_COM_FBS_2 =
            new TabStateInfo(
                    false,
                    true,
                    2,
                    2,
                    "https://www.google.com/",
                    "www.google.com",
                    "HAAAABgAPAA4AAAALAAoACQAAAAgAAAAFAAEABgAAAAAAAAAAAAAAAAAAAAAAAAA1SYMh5kBAAAA"
                        + "AAAACwAAABgAAAAcAAAAlHyThpkBAAAAAAAA/////wEAAAAgAAAAXAYAAFgGAAAAAAAAAgAAAAEA"
                        + "AAA0AwAAMAMAAAAAAAAXAAAAY2hyb21lLW5hdGl2ZTovL25ld3RhYi8ABwAAAE4AZQB3ACAAdABh"
                        + "AGIAAACUAgAAkAIAACEAAACIAgAAGAAAAAAAAAAQAAAAAAAAABAAAAAAAAAACAAAAAAAAACQAAAA"
                        + "BgAAAIgAAAAAAAAAAAAAAAAAAADAAAAAAAAAAAAAAAAAAAAAyAAAAAAAAAAAAAAABgAAAMAAAAAA"
                        + "AAAAA9nJl7U/BgAE2cmXtT8GADABAAAAAAAASAEAAAAAAAAAAAAAAAAAAEABAAAAAAAAmAEAAAAA"
                        + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAgAAAAAAAAANgAAABcAAABjAGgAcgBv"
                        + "AG0AZQAtAG4AYQB0AGkAdgBlADoALwAvAG4AZQB3AHQAYQBiAC8AAAAQAAAAAAAAAAgAAAAAAAAA"
                        + "CAAAAAAAAAAIAAAAAAAAADgAAAABAAAAMAAAAAAAAAA4AAAAAAAAAAAAAMCcgtc/OAAAAAAAAABQ"
                        + "AAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAgA"
                        + "AAAAAAAAEAAAAAQAAABoAHQAbQBsABAAAAAAAAAAAAAAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAA"
                        + "AAAAAAAAAAAAAAAAAAgAAAAAAAAAEAAAAAAAAAAIAAAAAAAAAFAAAAAkAAAAYwAxADcAMQBmAGEA"
                        + "NgBhAC0AOAAzADgAYgAtADQAMgAwADMALQBiADAAZgAzAC0AOAA5ADMAMQBmADIAZgA2ADAANwAw"
                        + "ADMAEAAAAAAAAAAIAAAAAAAAAFAAAAAkAAAAMwAxAGIAYQAyADEAYQA3AC0ANwA2AGQANwAtADQA"
                        + "NAA5ADMALQA4ADQAZAAzAC0ANAAyAGIANQA3ADEANgAyAGYAZQA0AGMABgAAAQAAAAAAAAAAAgAA"
                        + "ABcAAABjaHJvbWUtbmF0aXZlOi8vbmV3dGFiLwAAAAAAfLyKz02eLwAAAAAAAAAAAAYAAAAAAAAA"
                        + "Y+5R4EueLwD//////////2PuUeBLni8AAAAAABADAAAMAwAAAQAAABcAAABodHRwczovL3d3dy5n"
                        + "b29nbGUuY29tLwAOAAAAdwB3AHcALgBnAG8AbwBnAGwAZQAuAGMAbwBtAGQCAABgAgAAIQAAAFgC"
                        + "AAAYAAAAAAAAABAAAAAAAAAAEAAAAAAAAAAIAAAAAAAAAJAAAAAGAAAAiAAAAAAAAAAAAAAAAAAA"
                        + "AMAAAAAAAAAAAAAAAAAAAADIAAAAAAAAAAAAAAAGAAAAwAAAAAAAAADDy3aHtz8GAMTLdoe3PwYA"
                        + "AAEAAAAAAAAYAQAAAAAAAAAAAAAAAAAAEAEAAAAAAABoAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                        + "AAAAAAAAABAAAAAAAAAACAAAAAAAAAA2AAAAFwAAAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcA"
                        + "bwBvAGcAbABlAC4AYwBvAG0ALwAAABAAAAAAAAAACAAAAAAAAAAIAAAAAAAAAAgAAAAAAAAAOAAA"
                        + "AAEAAAAwAAAAAAAAADgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAA"
                        + "AAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                        + "AAAACAAAAAAAAAAQAAAAAAAAAAgAAAAAAAAAUAAAACQAAAAyADMAYwA2AGMANAAyADAALQBlADcA"
                        + "NwA5AC0ANAAwADMAZQAtAGEAOQAxADQALQA2ADYANAA4ADgAOQA4ADkANQAxADEAOAAQAAAAAAAA"
                        + "AAgAAAAAAAAAUAAAACQAAABiADUANQA3ADQANwA1ADAALQBhADIAYwBkAC0ANABjADgAZAAtADgA"
                        + "ZQBjADMALQAwAGUANwA4AGYAMgAzADgAZABlAGEAMAABAAACAAAAAAAAAAACAAAAFgAAAGh0dHA6"
                        + "Ly93d3cuZ29vZ2xlLmNvbS8AAAAAAAApcf3PTZ4vAAAAAAAAAAAABgAAAAAAAAApcf3PTZ4vAP//"
                        + "////////KXH9z02eLwAAAAAA");

    public static final TabStateInfo V2_GOOGLE_CA_FBS =
            new TabStateInfo(
                    false,
                    true,
                    2,
                    3,
                    "https://www.google.ca/",
                    "www.google.ca",
                    "HAAAABgAPAA4AAAALAAoACQAAAAgAAAAFAAEABgAAAAAAAAAAAAAAAAAAAAAAAAAkKUNh5kBAAAA"
                        + "AAAACwAAABgAAAAcAAAAlHyThpkBAAAAAAAA/////wEAAAAgAAAAXAYAAFgGAAAAAAAAAgAAAAEA"
                        + "AAA0AwAAMAMAAAAAAAAXAAAAY2hyb21lLW5hdGl2ZTovL25ld3RhYi8ABwAAAE4AZQB3ACAAdABh"
                        + "AGIAAACUAgAAkAIAACEAAACIAgAAGAAAAAAAAAAQAAAAAAAAABAAAAAAAAAACAAAAAAAAACQAAAA"
                        + "BgAAAIgAAAAAAAAAAAAAAAAAAADAAAAAAAAAAAAAAAAAAAAAyAAAAAAAAAAAAAAABgAAAMAAAAAA"
                        + "AAAAA9nJl7U/BgAE2cmXtT8GADABAAAAAAAASAEAAAAAAAAAAAAAAAAAAEABAAAAAAAAmAEAAAAA"
                        + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAgAAAAAAAAANgAAABcAAABjAGgAcgBv"
                        + "AG0AZQAtAG4AYQB0AGkAdgBlADoALwAvAG4AZQB3AHQAYQBiAC8AAAAQAAAAAAAAAAgAAAAAAAAA"
                        + "CAAAAAAAAAAIAAAAAAAAADgAAAABAAAAMAAAAAAAAAA4AAAAAAAAAAAAAMCcgtc/OAAAAAAAAABQ"
                        + "AAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAgA"
                        + "AAAAAAAAEAAAAAQAAABoAHQAbQBsABAAAAAAAAAAAAAAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAA"
                        + "AAAAAAAAAAAAAAAAAAgAAAAAAAAAEAAAAAAAAAAIAAAAAAAAAFAAAAAkAAAAYwAxADcAMQBmAGEA"
                        + "NgBhAC0AOAAzADgAYgAtADQAMgAwADMALQBiADAAZgAzAC0AOAA5ADMAMQBmADIAZgA2ADAANwAw"
                        + "ADMAEAAAAAAAAAAIAAAAAAAAAFAAAAAkAAAAMwAxAGIAYQAyADEAYQA3AC0ANwA2AGQANwAtADQA"
                        + "NAA5ADMALQA4ADQAZAAzAC0ANAAyAGIANQA3ADEANgAyAGYAZQA0AGMABgAAAQAAAAAAAAAAAgAA"
                        + "ABcAAABjaHJvbWUtbmF0aXZlOi8vbmV3dGFiLwAAAAAAGklY1U2eLwAAAAAAAAAAAAYAAAAAAAAA"
                        + "Y+5R4EueLwD//////////2PuUeBLni8AAAAAABADAAAMAwAAAQAAABYAAABodHRwczovL3d3dy5n"
                        + "b29nbGUuY2EvAAANAAAAdwB3AHcALgBnAG8AbwBnAGwAZQAuAGMAYQAAAGQCAABgAgAAIQAAAFgC"
                        + "AAAYAAAAAAAAABAAAAAAAAAAEAAAAAAAAAAIAAAAAAAAAJAAAAAGAAAAiAAAAAAAAAAAAAAAAAAA"
                        + "AMAAAAAAAAAAAAAAAAAAAADIAAAAAAAAAAAAAAAGAAAAwAAAAAAAAAAleE2Ntz8GACZ4TY23PwYA"
                        + "AAEAAAAAAAAYAQAAAAAAAAAAAAAAAAAAEAEAAAAAAABoAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                        + "AAAAAAAAABAAAAAAAAAACAAAAAAAAAA0AAAAFgAAAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcA"
                        + "bwBvAGcAbABlAC4AYwBhAC8AAAAAABAAAAAAAAAACAAAAAAAAAAIAAAAAAAAAAgAAAAAAAAAOAAA"
                        + "AAEAAAAwAAAAAAAAADgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAA"
                        + "AAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                        + "AAAACAAAAAAAAAAQAAAAAAAAAAgAAAAAAAAAUAAAACQAAAA1ADIAYwAzAGIAYQBiADUALQBmADAA"
                        + "YwAyAC0ANAA5ADIAMwAtADkANAAyADkALQA1AGIAYQAzADkANQBkADQAYgBiAGMAYwAQAAAAAAAA"
                        + "AAgAAAAAAAAAUAAAACQAAAA5ADUAZQBiADEAYgBmAGQALQBkAGIAMwAwAC0ANABiADcAZQAtADgA"
                        + "ZQA3ADgALQA1ADUAYgA0ADUANABjADMAZgAyADgANgABAAACAAAAAAAAAAACAAAAFQAAAGh0dHA6"
                        + "Ly93d3cuZ29vZ2xlLmNhLwAAAAAAAABKPdTVTZ4vAAAAAAAAAAAABgAAAAAAAABKPdTVTZ4vAP//"
                        + "////////Sj3U1U2eLwAAAAAA");

    public static final TabStateInfo V2_BAIDU =
            new TabStateInfo(
                    false,
                    false,
                    2,
                    4,
                    "http://www.baidu.com/",
                    "百度一下",
                    "AAABTbBCEBcAAAFkYAEAAAAAAAABAAAAAAAAAFABAABMAQAAAAAAABUAAABodHRwOi8vd3d3LmJhaWR1LmNvbS"
                        + "8AAAAEAAAAfnamXgBOC07IAAAAxAAAABYAAAAAAAAAKgAAAGgAdAB0AHAAOgAvAC8AdwB3AHcALgBiAGEAaQ"
                        + "BkAHUALgBjAG8AbQAvAAAA/////wAAAAAAAAAAIgAAAGgAdAB0AHAAOgAvAC8AYgBhAGkAZAB1AC4AYwBvAG"
                        + "0ALwAAAAAAAAAIAAAAAAAAwJyC1z9MWRSCeBcFAE1ZFIJ4FwUAS1kUgngXBQABAAAACAAAAAAAAAAAAAAACA"
                        + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/////wAAAAAAAAAIAAAAABEAAABodHRwOi8vYmFpZHUuY29tLwAAAA"
                        + "EAAAARAAAAaHR0cDovL2JhaWR1LmNvbS8AAAAAAAAA7Nyiyg52LgAAAAAAyAAAAAEAAAD/////AAAAAAACAA"
                        + "AAAAAAAAAA");

    public static final TabStateInfo V2_DUCK_DUCK_GO =
            new TabStateInfo(
                    false,
                    false,
                    2,
                    5,
                    "https://duckduckgo.com/",
                    "DuckDuckGo",
                    "AAABTbBCExUAAAFAPAEAAAAAAAABAAAAAAAAACwBAAAoAQAAAAAAABcAAABodHRwczovL2R1Y2tkdWNrZ28uY2"
                        + "9tLwAKAAAARAB1AGMAawBEAHUAYwBrAEcAbwCoAAAApAAAABYAAAAAAAAALgAAAGgAdAB0AHAAcwA6AC8ALw"
                        + "BkAHUAYwBrAGQAdQBjAGsAZwBvAC4AYwBvAG0ALwAAAP////8AAAAAAAAAAP////8AAAAACAAAAAAAAAAAAA"
                        + "AAlVAhgngXBQCWUCGCeBcFAJdQIYJ4FwUAAQAAAAgAAAAAAAAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                        + "AAAP////8AAAAAAAAACAAAAAAAAAAAAQAAABYAAABodHRwOi8vZHVja2R1Y2tnby5jb20vAAAAAAAAmoGoyg"
                        + "52LgAAAAAAyAAAAAEAAAD/////AAAAAAACAAAAAAAAAAAA");

    public static final TabStateInfo V2_HAARETZ =
            new TabStateInfo(
                    false,
                    false,
                    2,
                    6,
                    "http://www.haaretz.co.il/",
                    "חדשות, ידיעות מהארץ והעולם - עיתון הארץ",
                    "AAABTbBhcJQAAAD49AAAAAAAAAABAAAAAAAAAOQAAADgAAAAAAAAABkAAABodHRwOi8vd3d3LmhhYXJldHouY2"
                        + "8uaWwvAAAAJwAAANcF0wXpBdUF6gUsACAA2QXTBdkF4gXVBeoFIADeBdQF0AXoBeUFIADVBdQF4gXVBdwF3Q"
                        + "UgAC0AIADiBdkF6gXVBd8FIADUBdAF6AXlBQAAAAAAAAAAAAgAAAAAGQAAAGh0dHA6Ly93d3cuaGFhcmV0ei"
                        + "5jby5pbC8AAAABAAAAGQAAAGh0dHA6Ly93d3cuaGFhcmV0ei5jby5pbC8AAAAAAAAAJ7hFRQ92LgAAAAAAyA"
                        + "AAAAEAAAD/////AAAAAAACAAAAAAAAAAAA");

    public static final TabStateInfo V2_TEXTAREA =
            new TabStateInfo(
                    false,
                    false,
                    2,
                    7,
                    "http://textarea.org/",
                    "textarea",
                    "AAABSPI9OA8AAALs6AIAAAAAAAACAAAAAQAAACABAAAcAQAAAAAAABcAAABjaHJvbWUtbmF0aXZlOi8vbmV3dG"
                        + "FiLwAHAAAATgBlAHcAIAB0AGEAYgAAAKQAAACgAAAAFQAAAAAAAAAuAAAAYwBoAHIAbwBtAGUALQBuAGEAdA"
                        + "BpAHYAZQA6AC8ALwBuAGUAdwB0AGEAYgAvAAAA/////wAAAAAAAAAA/////wAAAAAIAAAAAAAAAAAA8D8EkS"
                        + "Y/8gQFAAWRJj/yBAUABpEmP/IEBQABAAAACAAAAAAAAAAAAPC/CAAAAAAAAAAAAPC/AAAAAAAAAAD/////AA"
                        + "AAAAYAAAAAAAAAAAAAAAEAAAAXAAAAY2hyb21lLW5hdGl2ZTovL25ld3RhYi8AAAAAAMnUrIeIYy4AAAAAAA"
                        + "AAAAC0AQAAsAEAAAEAAAAUAAAAaHR0cDovL3RleHRhcmVhLm9yZy8IAAAAdABlAHgAdABhAHIAZQBhAEABAA"
                        + "A8AQAAFQAAAAAAAAAoAAAAaAB0AHQAcAA6AC8ALwB0AGUAeAB0AGEAcgBlAGEALgBvAHIAZwAvAP////8AAA"
                        + "AAAAAAAP////8HAAAAYAAAAAoADQA/ACUAIABXAGUAYgBLAGkAdAAgAHMAZQByAGkAYQBsAGkAegBlAGQAIA"
                        + "BmAG8AcgBtACAAcwB0AGEAdABlACAAdgBlAHIAcwBpAG8AbgAgADgAIAAKAA0APQAmABAAAABOAG8AIABvAH"
                        + "cAbgBlAHIAAgAAADEAAAAAAAAAEAAAAHQAZQB4AHQAYQByAGUAYQACAAAAMQAAAAAAAAAIAAAAAAAAAAAAAA"
                        + "AHkSY/8gQFAAiRJj/yBAUABpEmP/IEBQABAAAACAAAAAAAAAAAAAAACAAAAAAAAAAAAAAAAAAAAAAAAAD///"
                        + "//AAAAAAEAAAIAAAAAAAAAAAEAAAAUAAAAaHR0cDovL3RleHRhcmVhLm9yZy8AAAAANKvVh4hjLgAAAAAAyA"
                        + "AAAP////8AAAAAAAIAAAAAAAAAAwE=");

    /**
     * Tab model metadata file containing information about multiple tabs, with Baidu selected.
     *
     * <p>This file was created by clearing Chrome's app data, turning off wi-fi, and then visiting
     * each of the pages in turn.
     */
    public static final TabModelMetaDataInfo TAB_MODEL_METADATA_V4 =
            new TabModelMetaDataInfo(
                    4,
                    0,
                    V2_BAIDU.tabId,
                    new TabStateInfo[] {
                        V2_GOOGLE_COM_FBS,
                        V2_GOOGLE_COM_FBS_2,
                        V2_GOOGLE_CA_FBS,
                        V2_BAIDU,
                        V2_DUCK_DUCK_GO,
                        V2_HAARETZ,
                        V2_TEXTAREA
                    },
                    "AAAABAAAAAf/////AAAAAwAAAAEAFmh0dHA6Ly93d3cuZ29vZ2xlLmNvbS8AAAACABZodHRw"
                        + "Oi8vd3d3Lmdvb2dsZS5jb20vAAAAAwAVaHR0cDovL3d3dy5nb29nbGUuY2EvAAAABAAVaHR0"
                        + "cDovL3d3dy5iYWlkdS5jb20vAAAABQAXaHR0cHM6Ly9kdWNrZHVja2dvLmNvbS8AAAAGABlo"
                        + "dHRwOi8vd3d3LmhhYXJldHouY28uaWwvAAAABwAUaHR0cDovL3RleHRhcmVhLm9yZy8=");

    /** Same as TAB_MODEL_METADATA_V4, but using the version 5 file format. */
    public static final TabModelMetaDataInfo TAB_MODEL_METADATA_V5 =
            new TabModelMetaDataInfo(
                    5,
                    0,
                    V2_BAIDU.tabId,
                    new TabStateInfo[] {
                        V2_GOOGLE_COM_FBS,
                        V2_GOOGLE_COM_FBS_2,
                        V2_GOOGLE_CA_FBS,
                        V2_BAIDU,
                        V2_DUCK_DUCK_GO,
                        V2_HAARETZ,
                        V2_TEXTAREA
                    },
                    "AAAABQAAAAcAAAAA/////wAAAAMAAAABABZodHRwOi8vd3d3Lmdvb2dsZS5jb20vAAAAAgAWaHR0"
                        + "cDovL3d3dy5nb29nbGUuY29tLwAAAAMAFWh0dHA6Ly93d3cuZ29vZ2xlLmNhLwAAAAQAFWh0dHA6"
                        + "Ly93d3cuYmFpZHUuY29tLwAAAAUAF2h0dHBzOi8vZHVja2R1Y2tnby5jb20vAAAABgAZaHR0cDov"
                        + "L3d3dy5oYWFyZXR6LmNvLmlsLwAAAAcAFGh0dHA6Ly90ZXh0YXJlYS5vcmcv");

    /**
     * Similar to TAB_MODEL_METADATA_V5, but has a single Incognito tab. The tab state can't be
     * restored (currently) because this Class doesn't support Incognito TabStates.
     */
    public static final TabModelMetaDataInfo TAB_MODEL_METADATA_V5_WITH_INCOGNITO =
            new TabModelMetaDataInfo(
                    5,
                    1,
                    V2_BAIDU.tabId,
                    new TabStateInfo[] {
                        null,
                        V2_GOOGLE_COM_FBS,
                        V2_GOOGLE_COM_FBS_2,
                        V2_GOOGLE_CA_FBS,
                        V2_BAIDU,
                        V2_DUCK_DUCK_GO,
                        V2_HAARETZ,
                        V2_TEXTAREA
                    },
                    "AAAABQAAAAgAAAABAAAAAAAAAAQAAAAIABRodHRwOi8vZXJmd29ybGQuY29tLwAAAAEAFmh0dHA6"
                        + "Ly93d3cuZ29vZ2xlLmNvbS8AAAACABZodHRwOi8vd3d3Lmdvb2dsZS5jb20vAAAAAwAVaHR0cDov"
                        + "L3d3dy5nb29nbGUuY2EvAAAABAAVaHR0cDovL3d3dy5iYWlkdS5jb20vAAAABQAXaHR0cHM6Ly9k"
                        + "dWNrZHVja2dvLmNvbS8AAAAGABlodHRwOi8vd3d3LmhhYXJldHouY28uaWwvAAAABwAUaHR0cDov"
                        + "L3RleHRhcmVhLm9yZy8=");

    /** Same as TAB_MODEL_METADATA_V4, but using the version 5 file format. */
    public static final TabModelMetaDataInfo TAB_MODEL_METADATA_V5_ALTERNATE_ORDER =
            new TabModelMetaDataInfo(
                    5,
                    0,
                    V2_GOOGLE_CA_FBS.tabId,
                    new TabStateInfo[] {
                        V2_TEXTAREA,
                        V2_BAIDU,
                        V2_DUCK_DUCK_GO,
                        V2_HAARETZ,
                        V2_GOOGLE_CA_FBS,
                        V2_GOOGLE_COM_FBS
                    },
                    "AAAABQAAAAYAAAAA/////wAAAAQAAAAHABRodHRwOi8vdGV4dGFyZWEub3JnLwAAAAQAFWh0dHA6"
                        + "Ly93d3cuYmFpZHUuY29tLwAAAAUAF2h0dHBzOi8vZHVja2R1Y2tnby5jb20vAAAABgAZaHR0cDov"
                        + "L3d3dy5oYWFyZXR6LmNvLmlsLwAAAAMAFWh0dHA6Ly93d3cuZ29vZ2xlLmNhLwAAAAIAFmh0dHA6"
                        + "Ly93d3cuZ29vZ2xlLmNvbS8=");

    // Active Tab is google.ca (V2_GOOGLE_CA_FBS) with Tab ID 1.
    // Other Tab in the Tab Model is google.com (V2_GOOGLE_COM_FBS) with Tab ID 3
    public static final TabModelMetaDataInfo GOOGLE_CA_GOOGLE_COM =
            new TabModelMetaDataInfo(
                    5,
                    0,
                    V2_GOOGLE_CA_FBS.tabId,
                    new TabStateInfo[] {V2_GOOGLE_CA_FBS, V2_GOOGLE_COM_FBS},
                    // The following can be obtained by
                    // 1. Open Chrome for Android, navigate to sites, open new Tabs, windows etc.
                    // 2. adb root
                    // 3. adb pull
                    // /data/data/com.google.android.apps.chrome/app_tabs/0/tab_state<index> .
                    // e.g. adb pull /data/data/com.google.android.apps.chrome/app_tabs/0/tab_state1
                    // .
                    // for second window selector
                    // 4. base64 tab_state<index> e.g. base64 tab_state1
                    "AAAABQAAAAIAAAAA/////wAAAAAAAAABABZodHRwczovL3d3dy5nb29nbGUuY2EvAAAABQAXaHR0"
                            + "cHM6Ly93d3cuZ29vZ2xlLmNvbS8=");

    // Active Tab is textarea.org (V2_TEXTAREA) with Tab ID 4.
    // Other Tab in the Tab Model is duckduckgo.com (V2_DUCK_DUCK_GO) with Tab ID 5
    // Tabs 6 and 7 are incognito Tabs.
    public static final TabModelMetaDataInfo TEXTAREA_DUCK_DUCK_GO =
            new TabModelMetaDataInfo(
                    5,
                    0,
                    V2_TEXTAREA.tabId,
                    new TabStateInfo[] {V2_TEXTAREA, V2_DUCK_DUCK_GO},
                    "AAAABQAAAAQAAAACAAAAAQAAAAIAAAAGABJodHRwczovL2hlbGxvLmNvbS8AAAAHAA9odHRwOi8v"
                        + "Ym9vLmNvbS8AAAADABVodHRwczovL3RleHRhcmVhLm9yZy8AAAAEABdodHRwczovL2R1Y2tkdWNr"
                        + "Z28uY29tLw==");

    private final File mTestingDirectory;
    private File mDataDirectory;

    /**
     * Creates a temporary directory that stores {@link TabState} files and the metadata required to
     * restore the {@link TabModel}s from the {@link TabPersistentStore}.
     *
     * @param context Context to use.
     * @param baseDirectoryName Name of the directory to store test data in.
     * @param subdirectoryName Subdirectory of the base directory. May be null, in which case the
     *     baseDirectoryName will store the data. This mocks out how the ChromeTabbedActivity
     *     instances all store their data in different subdirectories of the base data directory.
     */
    public TestTabModelDirectory(
            Context context, String baseDirectoryName, String subdirectoryName) {
        mTestingDirectory = new File(context.getCacheDir(), baseDirectoryName);
        if (mTestingDirectory.exists()) {
            FileUtils.recursivelyDeleteFile(mTestingDirectory, FileUtils.DELETE_ALL);
        }
        if (!mTestingDirectory.mkdirs()) {
            Log.e(TAG, "Failed to create: " + mTestingDirectory.getName());
        }

        // Create the subdirectory.
        mDataDirectory = mTestingDirectory;
        if (subdirectoryName != null) {
            mDataDirectory = new File(mTestingDirectory, subdirectoryName);
            if (!mDataDirectory.exists() && !mDataDirectory.mkdirs()) {
                Log.e(TAG, "Failed to create subdirectory: " + mDataDirectory.getName());
            }
        }
    }

    /** Nukes all the testing data. */
    public void tearDown() {
        FileUtils.recursivelyDeleteFile(mTestingDirectory, FileUtils.DELETE_ALL);
    }

    /** Returns the base data directory. */
    public File getBaseDirectory() {
        return mTestingDirectory;
    }

    /** Returns the directory containing all the TabStates. */
    public File getDataDirectory() {
        return mDataDirectory;
    }

    /** Calls the three-param version of this method with index = 0. */
    public void writeTabModelFiles(TabModelMetaDataInfo info, boolean writeTabStates)
            throws Exception {
        writeTabModelFiles(info, writeTabStates, 0);
    }

    /**
     * Writes out data required to restore a TabModel to the data directories.
     *
     * @param info The info to write to the tab metadata file.
     * @param writeTabStates Whether or not to write the TabState files for each of the Tabs out.
     * @param index The TabModelSelectorIndex to write the metadata file for.
     */
    public void writeTabModelFiles(TabModelMetaDataInfo info, boolean writeTabStates, int index)
            throws Exception {
        writeFile(mDataDirectory, "tab_state" + Integer.toString(index), info.encodedFile);
        for (TabStateInfo tabStateInfo : info.contents) {
            writeTabStateFile(tabStateInfo);
        }
    }

    /** Writes out a specific TabState file to the data directories. */
    public void writeTabStateFile(TabStateInfo info) throws Exception {
        if (info != null) writeFile(mDataDirectory, info.filename, info.encodedTabState);
    }

    private void writeFile(File directory, String filename, String data) throws Exception {
        File file = new File(directory, filename);
        FileOutputStream outputStream = null;
        try {
            outputStream = new FileOutputStream(file);
            outputStream.write(Base64.decode(data, 0));
        } catch (FileNotFoundException e) {
            assert false : "Failed to create " + filename;
        } finally {
            StreamUtil.closeQuietly(outputStream);
        }
    }
}

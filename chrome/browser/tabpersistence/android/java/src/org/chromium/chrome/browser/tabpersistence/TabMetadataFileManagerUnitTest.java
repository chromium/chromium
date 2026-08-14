// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabpersistence;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.TriState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager.OnTabStateReadCallback;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager.TabModelMetadata;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager.TabModelSelectorMetadata;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabMetadataFileManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabMetadataFileManagerUnitTest {
    @Rule public TemporaryFolder mTemporaryFolder = new TemporaryFolder();

    private static class TabReadDetails {
        public final int index;
        public final int id;
        public final String url;
        public final @TriState int isIncognito;
        public final boolean isStandardActiveIndex;
        public final boolean isIncognitoActiveIndex;

        TabReadDetails(
                int index,
                int id,
                String url,
                @TriState int isIncognito,
                boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex) {
            this.index = index;
            this.id = id;
            this.url = url;
            this.isIncognito = isIncognito;
            this.isStandardActiveIndex = isStandardActiveIndex;
            this.isIncognitoActiveIndex = isIncognitoActiveIndex;
        }
    }

    @Test
    public void testSaveAndReadMetadataFile() throws Exception {
        File file = mTemporaryFolder.newFile("tab_state0");

        TabModelMetadata normalModel = new TabModelMetadata(0);
        normalModel.ids.add(101);
        normalModel.urls.add("https://google.com");
        normalModel.ids.add(102);
        normalModel.urls.add("https://chromium.org");

        TabModelMetadata incognitoModel = new TabModelMetadata(0);
        incognitoModel.ids.add(201);
        incognitoModel.urls.add("https://secret.com");

        TabModelSelectorMetadata metadata =
                new TabModelSelectorMetadata(normalModel, incognitoModel);
        TabMetadataFileManager.saveListToFile(file, metadata);

        List<TabReadDetails> readDetails = new ArrayList<>();
        OnTabStateReadCallback callback =
                (index, id, url, isIncognito, isStandardActiveIndex, isIncognitoActiveIndex) -> {
                    readDetails.add(
                            new TabReadDetails(
                                    index,
                                    id,
                                    url,
                                    isIncognito,
                                    isStandardActiveIndex,
                                    isIncognitoActiveIndex));
                };

        try (DataInputStream stream =
                new DataInputStream(new BufferedInputStream(new FileInputStream(file)))) {
            int nextId = TabMetadataFileManager.readSavedMetadataFile(stream, callback, null);
            assertEquals(202, nextId);
        }

        assertEquals(3, readDetails.size());

        // Incognito tab is stored first.
        TabReadDetails tab0 = readDetails.get(0);
        assertEquals(201, tab0.id);
        assertEquals("https://secret.com", tab0.url);
        assertEquals(TriState.TRUE, tab0.isIncognito);
        assertTrue(tab0.isIncognitoActiveIndex);
        assertFalse(tab0.isStandardActiveIndex);

        // Standard tabs follow.
        TabReadDetails tab1 = readDetails.get(1);
        assertEquals(101, tab1.id);
        assertEquals("https://google.com", tab1.url);
        assertEquals(TriState.FALSE, tab1.isIncognito);
        assertTrue(tab1.isStandardActiveIndex);
        assertFalse(tab1.isIncognitoActiveIndex);

        TabReadDetails tab2 = readDetails.get(2);
        assertEquals(102, tab2.id);
        assertEquals("https://chromium.org", tab2.url);
        assertEquals(TriState.FALSE, tab2.isIncognito);
        assertFalse(tab2.isStandardActiveIndex);
        assertFalse(tab2.isIncognitoActiveIndex);
    }

    @Test
    public void testReadLegacyMetadataFile_unknownIncognito() throws Exception {
        // Version 4 without incognito count.
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        DataOutputStream out = new DataOutputStream(baos);
        out.writeInt(4); // Version 4
        out.writeInt(1); // Total count
        out.writeInt(-1); // incognitoActiveIndex
        out.writeInt(0); // standardActiveIndex
        out.writeInt(101); // Tab ID
        out.writeUTF("https://google.com"); // Tab URL
        out.flush();

        List<TabReadDetails> readDetails = new ArrayList<>();
        OnTabStateReadCallback callback =
                (index, id, url, isIncognito, isStandardActiveIndex, isIncognitoActiveIndex) -> {
                    readDetails.add(
                            new TabReadDetails(
                                    index,
                                    id,
                                    url,
                                    isIncognito,
                                    isStandardActiveIndex,
                                    isIncognitoActiveIndex));
                };

        try (DataInputStream stream =
                new DataInputStream(new ByteArrayInputStream(baos.toByteArray()))) {
            int nextId = TabMetadataFileManager.readSavedMetadataFile(stream, callback, null);
            assertEquals(102, nextId);
        }

        assertEquals(1, readDetails.size());
        TabReadDetails tab0 = readDetails.get(0);
        assertEquals(101, tab0.id);
        assertEquals(TriState.NOT_SET, tab0.isIncognito);
        assertTrue(tab0.isStandardActiveIndex);
    }
}

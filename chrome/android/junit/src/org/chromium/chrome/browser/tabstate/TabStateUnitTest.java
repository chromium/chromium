// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabstate;

import static org.junit.Assert.assertEquals;

import androidx.annotation.Nullable;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.StreamUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;

import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.channels.FileChannel;

/**
 * Unit tests for TabState.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabStateUnitTest {
    private static final byte[] CONTENTS_STATE_BYTES = new byte[] {1, 2, 3};
    private static final long TIMESTAMP = 10L;
    private static final int PARENT_ID = 1;
    private static final int VERSION = 2;
    private static final int THEME_COLOR = 4;
    private static final String OPENER_APP_ID = "test";
    private static final @Nullable @TabLaunchType Integer LAUNCH_TYPE_AT_CREATION = null;
    private static final int ROOT_ID = 1;

    @Rule
    public TemporaryFolder temporaryFolder = new TemporaryFolder();

    @Test
    public void testSaveTabStateWithMemoryMappedContentsState() throws IOException {
        File file = createTestTabStateFile();
        TabState state = createTabStateWithMappedByteBuffer(file);
        TabState.saveState(file, state, false);

        validateTestTabState(TabState.restoreTabState(file, false));
    }

    private TabState createTabStateWithMappedByteBuffer(File file) throws IOException {
        TabState state = new TabState();
        FileInputStream fileInputStream = null;

        try {
            fileInputStream = new FileInputStream(file);
            state.contentsState = new TabState.WebContentsState(fileInputStream.getChannel().map(
                    FileChannel.MapMode.READ_ONLY, fileInputStream.getChannel().position(),
                     file.length()));
            state.contentsState.setVersion(VERSION);
            state.timestampMillis = TIMESTAMP;
            state.parentId = PARENT_ID;
            state.themeColor = THEME_COLOR;
            state.openerAppId = OPENER_APP_ID;
            state.tabLaunchTypeAtCreation = LAUNCH_TYPE_AT_CREATION;
            state.rootId = ROOT_ID;
        } finally {
            StreamUtil.closeQuietly(fileInputStream);
        }
        return state;
    }

    private void validateTestTabState(TabState state) {
        assertEquals(TIMESTAMP, state.timestampMillis);
        assertEquals(PARENT_ID, state.parentId);
        assertEquals(OPENER_APP_ID, state.openerAppId);
        assertEquals(VERSION, state.contentsState.version());
        assertEquals(THEME_COLOR, state.getThemeColor());
        assertEquals(LAUNCH_TYPE_AT_CREATION, state.tabLaunchTypeAtCreation);
        assertEquals(ROOT_ID, state.rootId);
        assertEquals(CONTENTS_STATE_BYTES.length, state.contentsState.buffer().remaining());

        byte[] bytesFromFile = new byte[CONTENTS_STATE_BYTES.length];
        state.contentsState.buffer().get(bytesFromFile);

        for (int i = 0; i < CONTENTS_STATE_BYTES.length; i++) {
            assertEquals(bytesFromFile[i], CONTENTS_STATE_BYTES[i]);
        }
    }

    private File createTestTabStateFile() throws IOException {
        File file = temporaryFolder.newFile("tabStateByteBufferTestFile");
        FileOutputStream fileOutputStream = null;
        DataOutputStream dataOutputStream = null;
        try {
            fileOutputStream = new FileOutputStream(file);
            dataOutputStream = new DataOutputStream(fileOutputStream);
            dataOutputStream.write(CONTENTS_STATE_BYTES);
        } finally {
            StreamUtil.closeQuietly(fileOutputStream);
            StreamUtil.closeQuietly(dataOutputStream);
        }
        return file;
    }
}

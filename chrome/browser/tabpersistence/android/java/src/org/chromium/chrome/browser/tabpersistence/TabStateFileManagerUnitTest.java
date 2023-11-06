// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabpersistence;

import static org.junit.Assert.assertEquals;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.StreamUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabUserAgent;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.flatbuffer.TabLaunchTypeAtCreation;
import org.chromium.chrome.browser.tab.flatbuffer.UserAgentType;

import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.channels.FileChannel;

/** Unit tests for {@link TabStateFileManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabStateFileManagerUnitTest {
    private static final byte[] CONTENTS_STATE_BYTES = new byte[] {1, 2, 3};
    private static final long TIMESTAMP = 10L;
    private static final int PARENT_ID = 1;
    private static final int VERSION = 2;
    private static final int THEME_COLOR = 4;
    private static final String OPENER_APP_ID = "test";
    private static final @Nullable @TabLaunchType Integer LAUNCH_TYPE_AT_CREATION = null;
    private static final int ROOT_ID = 1;
    private static final @TabUserAgent int USER_AGENT = TabUserAgent.MOBILE;

    @Rule public TemporaryFolder temporaryFolder = new TemporaryFolder();

    @Test
    public void testSaveTabStateWithMemoryMappedContentsState() throws IOException {
        File file = createTestTabStateFile();
        TabState state = createTabStateWithMappedByteBuffer(file);
        TabStateFileManager.saveStateInternal(file, state, false);

        validateTestTabState(TabStateFileManager.restoreTabStateInternal(file, false));
    }

    @Test
    public void testFlatBufferValuesUnchanged() {
        // FlatBuffer enum values should not be changed as they are persisted across restarts.
        // Changing them would cause backward compatibility issues
        Assert.assertEquals(-2, TabLaunchTypeAtCreation.SIZE);
        Assert.assertEquals(-1, TabLaunchTypeAtCreation.UNKNOWN);
        Assert.assertEquals(0, TabLaunchTypeAtCreation.FROM_LINK);
        Assert.assertEquals(1, TabLaunchTypeAtCreation.FROM_EXTERNAL_APP);
        Assert.assertEquals(2, TabLaunchTypeAtCreation.FROM_CHROME_UI);
        Assert.assertEquals(3, TabLaunchTypeAtCreation.FROM_RESTORE);
        Assert.assertEquals(4, TabLaunchTypeAtCreation.FROM_LONGPRESS_FOREGROUND);
        Assert.assertEquals(5, TabLaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND);
        Assert.assertEquals(6, TabLaunchTypeAtCreation.FROM_REPARENTING);
        Assert.assertEquals(7, TabLaunchTypeAtCreation.FROM_LAUNCHER_SHORTCUT);
        Assert.assertEquals(8, TabLaunchTypeAtCreation.FROM_SPECULATIVE_BACKGROUND_CREATION);
        Assert.assertEquals(9, TabLaunchTypeAtCreation.FROM_BROWSER_ACTIONS);
        Assert.assertEquals(10, TabLaunchTypeAtCreation.FROM_LAUNCH_NEW_INCOGNITO_TAB);
        Assert.assertEquals(11, TabLaunchTypeAtCreation.FROM_STARTUP);
        Assert.assertEquals(12, TabLaunchTypeAtCreation.FROM_START_SURFACE);
        Assert.assertEquals(13, TabLaunchTypeAtCreation.FROM_TAB_GROUP_UI);
        Assert.assertEquals(14, TabLaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND_IN_GROUP);
        Assert.assertEquals(15, TabLaunchTypeAtCreation.FROM_APP_WIDGET);
        Assert.assertEquals(16, TabLaunchTypeAtCreation.FROM_LONGPRESS_INCOGNITO);
        Assert.assertEquals(17, TabLaunchTypeAtCreation.FROM_RECENT_TABS);
        Assert.assertEquals(18, TabLaunchTypeAtCreation.FROM_READING_LIST);
        Assert.assertEquals(19, TabLaunchTypeAtCreation.FROM_TAB_SWITCHER_UI);
        Assert.assertEquals(20, TabLaunchTypeAtCreation.FROM_RESTORE_TABS_UI);
        Assert.assertEquals(21, TabLaunchTypeAtCreation.FROM_OMNIBOX);
        Assert.assertEquals(
                "Need to increment 1 to expected value each time a LaunchTypeAtCreation "
                        + "is added. Also need to add any new LaunchTypeAtCreation to this test.",
                24,
                TabLaunchTypeAtCreation.names.length);
    }

    @Test
    public void testUserAgentUnchanged() {
        // User agent enum values should not be changed as they are persisted across restarts.
        // Changing them would cause backward compatibility issues
        Assert.assertEquals(-2, UserAgentType.USER_AGENT_SIZE);
        Assert.assertEquals(-1, UserAgentType.USER_AGENT_UNKNOWN);
        Assert.assertEquals(0, UserAgentType.DEFAULT);
        Assert.assertEquals(1, UserAgentType.MOBILE);
        Assert.assertEquals(2, UserAgentType.DESKTOP);
        Assert.assertEquals(3, UserAgentType.UNSET);
    }

    @Test
    public void testLaunchTypeFromFlatBufferConversion() {
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_LINK,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LINK));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_EXTERNAL_APP,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_EXTERNAL_APP));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_CHROME_UI,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_CHROME_UI));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_RESTORE,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_RESTORE));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LONGPRESS_FOREGROUND));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_LONGPRESS_INCOGNITO,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LONGPRESS_INCOGNITO));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_REPARENTING,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_REPARENTING));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_LAUNCHER_SHORTCUT,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LAUNCHER_SHORTCUT));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_SPECULATIVE_BACKGROUND_CREATION));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_BROWSER_ACTIONS,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_BROWSER_ACTIONS));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LAUNCH_NEW_INCOGNITO_TAB));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LAUNCH_NEW_INCOGNITO_TAB));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_STARTUP,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_STARTUP));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_START_SURFACE,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_START_SURFACE));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_TAB_GROUP_UI,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_TAB_GROUP_UI));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_TAB_SWITCHER_UI,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_TAB_SWITCHER_UI));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_RESTORE_TABS_UI,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_RESTORE_TABS_UI));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND_IN_GROUP));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_APP_WIDGET,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_APP_WIDGET));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_RECENT_TABS,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_RECENT_TABS));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_READING_LIST,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_READING_LIST));
        Assert.assertEquals(
                (Integer) TabLaunchType.FROM_OMNIBOX,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_OMNIBOX));
        Assert.assertEquals(
                (Integer) TabLaunchType.SIZE,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.SIZE));
        Assert.assertEquals(
                null,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.UNKNOWN));
    }

    @Test
    public void testLaunchTypeToFlatBufferConversion() {
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_LINK,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(TabLaunchType.FROM_LINK));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_EXTERNAL_APP,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_EXTERNAL_APP));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_CHROME_UI,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_CHROME_UI));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_RESTORE,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(TabLaunchType.FROM_RESTORE));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_LONGPRESS_FOREGROUND,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_LONGPRESS_FOREGROUND));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_LONGPRESS_INCOGNITO,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_LONGPRESS_INCOGNITO));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_REPARENTING,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_REPARENTING));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_LAUNCHER_SHORTCUT,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_LAUNCHER_SHORTCUT));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_SPECULATIVE_BACKGROUND_CREATION,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_BROWSER_ACTIONS,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_BROWSER_ACTIONS));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_LAUNCH_NEW_INCOGNITO_TAB,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_LAUNCH_NEW_INCOGNITO_TAB,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_STARTUP,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(TabLaunchType.FROM_STARTUP));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_START_SURFACE,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_START_SURFACE));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_TAB_GROUP_UI,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_TAB_GROUP_UI));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_TAB_SWITCHER_UI,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_TAB_SWITCHER_UI));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_RESTORE_TABS_UI,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_RESTORE_TABS_UI));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_APP_WIDGET,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_APP_WIDGET));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_RECENT_TABS,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_RECENT_TABS));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_READING_LIST,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_READING_LIST));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_OMNIBOX,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(TabLaunchType.FROM_OMNIBOX));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.SIZE,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(TabLaunchType.SIZE));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.UNKNOWN,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(null));
    }

    @Test
    public void testUserAgentFromFromBufferConversion() {
        Assert.assertEquals(
                TabUserAgent.DEFAULT,
                FlatBufferTabStateSerializer.getTabUserAgentTypeFromFlatBuffer(
                        UserAgentType.DEFAULT));
        Assert.assertEquals(
                TabUserAgent.DESKTOP,
                FlatBufferTabStateSerializer.getTabUserAgentTypeFromFlatBuffer(
                        UserAgentType.DESKTOP));
        Assert.assertEquals(
                TabUserAgent.MOBILE,
                FlatBufferTabStateSerializer.getTabUserAgentTypeFromFlatBuffer(
                        UserAgentType.MOBILE));
        Assert.assertEquals(
                TabUserAgent.UNSET,
                FlatBufferTabStateSerializer.getTabUserAgentTypeFromFlatBuffer(
                        UserAgentType.UNSET));
        Assert.assertEquals(
                TabUserAgent.SIZE,
                FlatBufferTabStateSerializer.getTabUserAgentTypeFromFlatBuffer(
                        UserAgentType.USER_AGENT_SIZE));
    }

    @Test
    public void testUserAgentFromToBufferConversion() {
        Assert.assertEquals(
                UserAgentType.DEFAULT,
                FlatBufferTabStateSerializer.getUserAgentTypeToFlatBuffer(TabUserAgent.DEFAULT));
        Assert.assertEquals(
                UserAgentType.DESKTOP,
                FlatBufferTabStateSerializer.getUserAgentTypeToFlatBuffer(TabUserAgent.DESKTOP));
        Assert.assertEquals(
                UserAgentType.MOBILE,
                FlatBufferTabStateSerializer.getUserAgentTypeToFlatBuffer(TabUserAgent.MOBILE));
        Assert.assertEquals(
                UserAgentType.UNSET,
                FlatBufferTabStateSerializer.getUserAgentTypeToFlatBuffer(TabUserAgent.UNSET));
        Assert.assertEquals(
                UserAgentType.USER_AGENT_SIZE,
                FlatBufferTabStateSerializer.getUserAgentTypeToFlatBuffer(TabUserAgent.SIZE));
    }

    private TabState createTabStateWithMappedByteBuffer(File file) throws IOException {
        TabState state = new TabState();
        FileInputStream fileInputStream = null;

        try {
            fileInputStream = new FileInputStream(file);
            state.contentsState =
                    new WebContentsState(
                            fileInputStream
                                    .getChannel()
                                    .map(
                                            FileChannel.MapMode.READ_ONLY,
                                            fileInputStream.getChannel().position(),
                                            file.length()));
            state.contentsState.setVersion(VERSION);
            state.timestampMillis = TIMESTAMP;
            state.parentId = PARENT_ID;
            state.themeColor = THEME_COLOR;
            state.openerAppId = OPENER_APP_ID;
            state.tabLaunchTypeAtCreation = LAUNCH_TYPE_AT_CREATION;
            state.rootId = ROOT_ID;
            state.userAgent = USER_AGENT;
            state.lastNavigationCommittedTimestampMillis = TIMESTAMP;
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
        assertEquals(USER_AGENT, state.userAgent);
        assertEquals(TIMESTAMP, state.lastNavigationCommittedTimestampMillis);

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

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabpersistence;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.StreamUtil;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabUserAgent;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.flatbuffer.TabLaunchTypeAtCreation;
import org.chromium.chrome.browser.tab.flatbuffer.UserAgentType;
import org.chromium.chrome.browser.tabpersistence.FlatBufferTabStateSerializer.TabStateFlatBufferDeserializeResult;
import org.chromium.chrome.test.util.ByteBufferTestUtils;

import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
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
    private static final @TabLaunchType int LAUNCH_TYPE_AT_CREATION = TabLaunchType.UNSET;
    private static final int ROOT_ID = 1;
    private static final @TabUserAgent int USER_AGENT = TabUserAgent.MOBILE;
    private static final long TAB_GROUP_ID_TOKEN_HIGH = 0x1234567890L;
    private static final long TAB_GROUP_ID_TOKEN_LOW = 0xABCDEF1234L;
    private static final Token TAB_GROUP_ID =
            new Token(TAB_GROUP_ID_TOKEN_HIGH, TAB_GROUP_ID_TOKEN_LOW);
    private static final int LARGE_BYTE_BUFFER_SIZE = Integer.MAX_VALUE / 4;
    private static final boolean CONTENT_IS_SENSITIVE = true;

    @Rule public TemporaryFolder temporaryFolder = new TemporaryFolder();

    private CipherFactory mCipherFactory;

    @Before
    public void setUp() {
        mCipherFactory = new CipherFactory();
    }

    @Test
    public void testSaveTabStateWithMemoryMappedContentsState_WithoutTabGroupId()
            throws IOException {
        Token tabGroupId = null;
        File file = createTestTabStateFile();
        TabState state = createTabStateWithMappedByteBuffer(file, tabGroupId);
        TabStateFileManager.saveStateInternal(file, state, false, mCipherFactory);

        validateTestTabState(
                TabStateFileManager.restoreTabStateInternal(file, false, mCipherFactory),
                tabGroupId);
    }

    @Test
    public void testSaveTabStateWithMemoryMappedContentsState_WithTabGroupId() throws IOException {
        Token tabGroupId = new Token(TAB_GROUP_ID_TOKEN_HIGH, TAB_GROUP_ID_TOKEN_LOW);
        File file = createTestTabStateFile();
        TabState state = createTabStateWithMappedByteBuffer(file, tabGroupId);
        TabStateFileManager.saveStateInternal(file, state, false, mCipherFactory);

        validateTestTabState(
                TabStateFileManager.restoreTabStateInternal(file, false, mCipherFactory),
                tabGroupId);
    }

    @Test
    public void testLargeContentsState() throws IOException {
        File file = createTestTabStateFile();
        ByteBuffer buffer = ByteBuffer.allocateDirect(LARGE_BYTE_BUFFER_SIZE);
        for (int i = 0; i < LARGE_BYTE_BUFFER_SIZE; i++) {
            buffer.put((byte) (i % Byte.MAX_VALUE));
        }
        WebContentsState contentsState = new WebContentsState(buffer);
        contentsState.setVersion(WebContentsState.CONTENTS_STATE_CURRENT_VERSION);
        TabState state = createTabState(contentsState);
        TabStateFileManager.saveStateInternal(file, state, /* encrypted= */ false, mCipherFactory);
        validateTestTabState(
                TabStateFileManager.restoreTabStateInternal(
                        file, /* isEncrypted= */ false, mCipherFactory),
                contentsState);
    }

    @Test
    public void testInvalidBuffer() {
        byte[] bytes = new byte[5000];
        for (int i = 0; i < bytes.length; i++) {
            bytes[i] = (byte) i;
        }

        var builder =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Tabs.TabState.FlatBufferDeserializeResult",
                                TabStateFlatBufferDeserializeResult
                                        .FAILURE_INDEX_OUT_OF_BOUNDS_EXCEPTION);
        HistogramWatcher histograms = builder.build();
        FlatBufferTabStateSerializer serializer = new FlatBufferTabStateSerializer(false);
        Assert.assertNull(serializer.deserialize(ByteBuffer.wrap(bytes)));
        histograms.assertExpected();
    }

    @Test
    public void testFlatBufferValuesUnchanged() {
        // FlatBuffer enum values should not be changed as they are persisted across restarts.
        // Changing them would cause backward compatibility issues
        Assert.assertEquals(-2, TabLaunchTypeAtCreation.SIZE);
        // UNKNOWN is effectively deprecated.
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
        Assert.assertEquals(22, TabLaunchTypeAtCreation.UNSET);
        Assert.assertEquals(23, TabLaunchTypeAtCreation.FROM_SYNC_BACKGROUND);
        Assert.assertEquals(24, TabLaunchTypeAtCreation.FROM_RECENT_TABS_FOREGROUND);
        Assert.assertEquals(25, TabLaunchTypeAtCreation.FROM_COLLABORATION_BACKGROUND_IN_GROUP);
        // Note this should be the total number of TabLaunchTypeAtCreation values including
        // SIZE and UNKNOWN so it should be equal to the last value +3.
        Assert.assertEquals(
                "Need to increment 1 to expected value each time a LaunchTypeAtCreation "
                        + "is added. Also need to add any new LaunchTypeAtCreation to this test.",
                28,
                TabLaunchTypeAtCreation.names.length);
    }

    @Test
    public void testTabLaunchTypeAddShouldUpdateFlatBuffer() {
        Assert.assertEquals(
                "When adding a new TabLaunchType please update tab_state_common.fbs,"
                        + " FlatBufferTabStateSerizer#getLaunchTypeFromFlatBuffer,"
                        + " FlatBufferTabStateSerizer#getLaunchTypeToFlatBuffer"
                        + " and this test file.",
                26,
                TabLaunchType.SIZE);
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
                TabLaunchType.FROM_LINK,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LINK));
        Assert.assertEquals(
                TabLaunchType.FROM_EXTERNAL_APP,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_EXTERNAL_APP));
        Assert.assertEquals(
                TabLaunchType.FROM_CHROME_UI,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_CHROME_UI));
        Assert.assertEquals(
                TabLaunchType.FROM_RESTORE,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_RESTORE));
        Assert.assertEquals(
                TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LONGPRESS_FOREGROUND));
        Assert.assertEquals(
                TabLaunchType.FROM_LONGPRESS_INCOGNITO,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LONGPRESS_INCOGNITO));
        Assert.assertEquals(
                TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND));
        Assert.assertEquals(
                TabLaunchType.FROM_REPARENTING,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_REPARENTING));
        Assert.assertEquals(
                TabLaunchType.FROM_LAUNCHER_SHORTCUT,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LAUNCHER_SHORTCUT));
        Assert.assertEquals(
                TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_SPECULATIVE_BACKGROUND_CREATION));
        Assert.assertEquals(
                TabLaunchType.FROM_BROWSER_ACTIONS,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_BROWSER_ACTIONS));
        Assert.assertEquals(
                TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LAUNCH_NEW_INCOGNITO_TAB));
        Assert.assertEquals(
                TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LAUNCH_NEW_INCOGNITO_TAB));
        Assert.assertEquals(
                TabLaunchType.FROM_STARTUP,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_STARTUP));
        Assert.assertEquals(
                TabLaunchType.FROM_START_SURFACE,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_START_SURFACE));
        Assert.assertEquals(
                TabLaunchType.FROM_TAB_GROUP_UI,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_TAB_GROUP_UI));
        Assert.assertEquals(
                TabLaunchType.FROM_TAB_SWITCHER_UI,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_TAB_SWITCHER_UI));
        Assert.assertEquals(
                TabLaunchType.FROM_RESTORE_TABS_UI,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_RESTORE_TABS_UI));
        Assert.assertEquals(
                TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND_IN_GROUP));
        Assert.assertEquals(
                TabLaunchType.FROM_APP_WIDGET,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_APP_WIDGET));
        Assert.assertEquals(
                TabLaunchType.FROM_RECENT_TABS,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_RECENT_TABS));
        Assert.assertEquals(
                TabLaunchType.FROM_READING_LIST,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_READING_LIST));
        Assert.assertEquals(
                TabLaunchType.FROM_OMNIBOX,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_OMNIBOX));
        Assert.assertEquals(
                TabLaunchType.UNSET,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.UNSET));
        Assert.assertEquals(
                TabLaunchType.FROM_SYNC_BACKGROUND,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_SYNC_BACKGROUND));
        Assert.assertEquals(
                TabLaunchType.FROM_RECENT_TABS_FOREGROUND,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_RECENT_TABS_FOREGROUND));
        Assert.assertEquals(
                TabLaunchType.SIZE,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.SIZE));
        Assert.assertEquals(
                TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP,
                FlatBufferTabStateSerializer.getLaunchTypeFromFlatBuffer(
                        TabLaunchTypeAtCreation.FROM_COLLABORATION_BACKGROUND_IN_GROUP));
        Assert.assertEquals(
                TabLaunchType.UNSET,
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
                TabLaunchTypeAtCreation.FROM_SYNC_BACKGROUND,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_SYNC_BACKGROUND));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.FROM_RECENT_TABS_FOREGROUND,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(
                        TabLaunchType.FROM_RECENT_TABS_FOREGROUND));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.UNSET,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(TabLaunchType.UNSET));
        Assert.assertEquals(
                TabLaunchTypeAtCreation.SIZE,
                FlatBufferTabStateSerializer.getLaunchTypeToFlatBuffer(TabLaunchType.SIZE));
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

    @Test
    public void testNullStateDirectoryDeleteFlatBuffer() {
        try {
            TabStateFileManager.deleteFlatBufferFiles(null);
        } catch (NullPointerException e) {
            throw new AssertionError(
                    "deleteFlatBufferFiles should not throw NullPointerException", e);
        }
    }

    @Test
    public void testNullListFilesDeleteFlatBuffer() {
        try {
            File stateDirectory = Mockito.mock(File.class);
            Mockito.doReturn(null).when(stateDirectory).listFiles();
            TabStateFileManager.deleteFlatBufferFiles(stateDirectory);
        } catch (NullPointerException e) {
            throw new AssertionError(
                    "deleteFlatBufferFiles should not throw NullPointerException", e);
        }
    }

    private TabState createTabStateWithMappedByteBuffer(File file, @Nullable Token tabGroupId)
            throws IOException {
        FileInputStream fileInputStream = null;
        try {
            fileInputStream = new FileInputStream(file);
            return createTabState(
                    new WebContentsState(
                            fileInputStream
                                    .getChannel()
                                    .map(
                                            FileChannel.MapMode.READ_ONLY,
                                            fileInputStream.getChannel().position(),
                                            file.length())),
                    tabGroupId);
        } finally {
            StreamUtil.closeQuietly(fileInputStream);
        }
    }

    private static TabState createTabState(
            WebContentsState contentsState, @Nullable Token tabGroupId) {
        TabState state = new TabState();
        state.contentsState = contentsState;
        state.contentsState.setVersion(VERSION);
        state.timestampMillis = TIMESTAMP;
        state.parentId = PARENT_ID;
        state.themeColor = THEME_COLOR;
        state.openerAppId = OPENER_APP_ID;
        state.tabLaunchTypeAtCreation = LAUNCH_TYPE_AT_CREATION;
        state.rootId = ROOT_ID;
        state.userAgent = USER_AGENT;
        state.lastNavigationCommittedTimestampMillis = TIMESTAMP;
        state.tabGroupId = tabGroupId;
        state.tabHasSensitiveContent = CONTENT_IS_SENSITIVE;
        return state;
    }

    private static TabState createTabState(WebContentsState contentsState) {
        return createTabState(contentsState, TAB_GROUP_ID);
    }

    private void validateTestTabState(TabState state, @Nullable Token tabGroupId) {
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(CONTENTS_STATE_BYTES.length);
        for (int i = 0; i < CONTENTS_STATE_BYTES.length; i++) {
            byteBuffer.put(CONTENTS_STATE_BYTES[i]);
        }
        validateTestTabState(state, tabGroupId, new WebContentsState(byteBuffer));
    }

    private static void validateTestTabState(TabState state, WebContentsState contentsState) {
        validateTestTabState(state, TAB_GROUP_ID, contentsState);
    }

    private static void validateTestTabState(
            TabState state, @Nullable Token tabGroupId, WebContentsState contentsState) {
        assertEquals(TIMESTAMP, state.timestampMillis);
        assertEquals(PARENT_ID, state.parentId);
        assertEquals(OPENER_APP_ID, state.openerAppId);
        assertEquals(VERSION, state.contentsState.version());
        assertEquals(THEME_COLOR, state.getThemeColor());
        assertEquals(LAUNCH_TYPE_AT_CREATION, state.tabLaunchTypeAtCreation);
        assertEquals(ROOT_ID, state.rootId);
        assertEquals(USER_AGENT, state.userAgent);
        assertEquals(TIMESTAMP, state.lastNavigationCommittedTimestampMillis);
        assertEquals(CONTENT_IS_SENSITIVE, state.tabHasSensitiveContent);
        if (tabGroupId == null) {
            assertNull(state.tabGroupId);
        } else {
            assertEquals(tabGroupId, state.tabGroupId);
        }
        ByteBufferTestUtils.verifyByteBuffer(state.contentsState.buffer(), contentsState.buffer());
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

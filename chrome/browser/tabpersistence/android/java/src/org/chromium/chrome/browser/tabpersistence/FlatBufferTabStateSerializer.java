// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabpersistence;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.flatbuffers.FlatBufferBuilder;

import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabUserAgent;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.flatbuffer.TabLaunchTypeAtCreation;
import org.chromium.chrome.browser.tab.flatbuffer.TabStateFlatBufferV1;
import org.chromium.chrome.browser.tab.flatbuffer.UserAgentType;

import java.nio.ByteBuffer;

/** {@link TabStateSerializer} backed by a FlatBuffer */
public class FlatBufferTabStateSerializer implements TabStateSerializer {
    private static final String NULL_OPENER_APP_ID = " ";

    private final boolean mIsEncrypted;

    public FlatBufferTabStateSerializer(boolean isEncrypted) {
        mIsEncrypted = isEncrypted;
    }

    @Override
    public ByteBuffer serialize(TabState state) {
        FlatBufferBuilder fbb = new FlatBufferBuilder();
        ByteBuffer byteBuffer =
                state.contentsState.buffer() != null
                        ? state.contentsState.buffer().asReadOnlyBuffer()
                        : null;
        if (byteBuffer != null) {
            byteBuffer.rewind();
        }
        int webContentsState =
                TabStateFlatBufferV1.createWebContentsStateBytesVector(
                        fbb,
                        byteBuffer == null
                                ? ByteBuffer.allocate(0).put(new byte[] {})
                                : byteBuffer);
        int openerAppId =
                fbb.createString(
                        state.openerAppId == null ? NULL_OPENER_APP_ID : state.openerAppId);
        TabStateFlatBufferV1.startTabStateFlatBufferV1(fbb);
        TabStateFlatBufferV1.addParentId(fbb, state.parentId);
        TabStateFlatBufferV1.addRootId(fbb, state.rootId);
        TabStateFlatBufferV1.addTimestampMillis(fbb, state.timestampMillis);
        TabStateFlatBufferV1.addWebContentsStateBytes(fbb, webContentsState);
        TabStateFlatBufferV1.addOpenerAppId(fbb, openerAppId);
        TabStateFlatBufferV1.addThemeColor(fbb, state.themeColor);
        TabStateFlatBufferV1.addLaunchTypeAtCreation(
                fbb, getLaunchTypeToFlatBuffer(state.tabLaunchTypeAtCreation));
        TabStateFlatBufferV1.addUserAgent(fbb, getUserAgentTypeToFlatBuffer(state.userAgent));
        TabStateFlatBufferV1.addLastNavigationCommittedTimestampMillis(
                fbb, state.lastNavigationCommittedTimestampMillis);
        int r = TabStateFlatBufferV1.endTabStateFlatBufferV1(fbb);
        fbb.finish(r);
        return fbb.dataBuffer();
    }

    @Override
    public TabState deserialize(ByteBuffer bytes) {
        TabStateFlatBufferV1 tabStateFlatBuffer =
                TabStateFlatBufferV1.getRootAsTabStateFlatBufferV1(bytes);

        TabState state = new TabState();
        state.parentId = tabStateFlatBuffer.parentId();
        state.rootId = tabStateFlatBuffer.rootId();
        state.openerAppId =
                NULL_OPENER_APP_ID.equals(tabStateFlatBuffer.openerAppId())
                        ? null
                        : tabStateFlatBuffer.openerAppId();
        state.timestampMillis = tabStateFlatBuffer.timestampMillis();
        state.lastNavigationCommittedTimestampMillis =
                tabStateFlatBuffer.lastNavigationCommittedTimestampMillis();
        state.userAgent = getTabUserAgentTypeFromFlatBuffer(tabStateFlatBuffer.userAgent());
        state.tabLaunchTypeAtCreation =
                getLaunchTypeFromFlatBuffer(tabStateFlatBuffer.launchTypeAtCreation());
        state.themeColor = tabStateFlatBuffer.themeColor();
        ByteBuffer webContentsStateBuffer =
                tabStateFlatBuffer.webContentsStateBytesAsByteBuffer() == null
                        ? ByteBuffer.allocateDirect(0)
                        : tabStateFlatBuffer.webContentsStateBytesAsByteBuffer().slice();
        if (mIsEncrypted) {
            state.contentsState =
                    new WebContentsState(
                            ByteBuffer.allocateDirect(webContentsStateBuffer.remaining()));
            state.contentsState.buffer().put(webContentsStateBuffer);
        } else {
            state.contentsState = new WebContentsState(webContentsStateBuffer);
        }
        state.contentsState.setVersion(WebContentsState.CONTENTS_STATE_CURRENT_VERSION);
        return state;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static @Nullable @TabLaunchType Integer getLaunchTypeFromFlatBuffer(
            int flatBufferLaunchType) {
        switch (flatBufferLaunchType) {
            case TabLaunchTypeAtCreation.FROM_LINK:
                return TabLaunchType.FROM_LINK;
            case TabLaunchTypeAtCreation.FROM_EXTERNAL_APP:
                return TabLaunchType.FROM_EXTERNAL_APP;
            case TabLaunchTypeAtCreation.FROM_CHROME_UI:
                return TabLaunchType.FROM_CHROME_UI;
            case TabLaunchTypeAtCreation.FROM_RESTORE:
                return TabLaunchType.FROM_RESTORE;
            case TabLaunchTypeAtCreation.FROM_LONGPRESS_FOREGROUND:
                return TabLaunchType.FROM_LONGPRESS_FOREGROUND;
            case TabLaunchTypeAtCreation.FROM_LONGPRESS_INCOGNITO:
                return TabLaunchType.FROM_LONGPRESS_INCOGNITO;
            case TabLaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND:
                return TabLaunchType.FROM_LONGPRESS_BACKGROUND;
            case TabLaunchTypeAtCreation.FROM_REPARENTING:
                return TabLaunchType.FROM_REPARENTING;
            case TabLaunchTypeAtCreation.FROM_LAUNCHER_SHORTCUT:
                return TabLaunchType.FROM_LAUNCHER_SHORTCUT;
            case TabLaunchTypeAtCreation.FROM_SPECULATIVE_BACKGROUND_CREATION:
                return TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION;
            case TabLaunchTypeAtCreation.FROM_BROWSER_ACTIONS:
                return TabLaunchType.FROM_BROWSER_ACTIONS;
            case TabLaunchTypeAtCreation.FROM_LAUNCH_NEW_INCOGNITO_TAB:
                return TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB;
            case TabLaunchTypeAtCreation.FROM_STARTUP:
                return TabLaunchType.FROM_STARTUP;
            case TabLaunchTypeAtCreation.FROM_START_SURFACE:
                return TabLaunchType.FROM_START_SURFACE;
            case TabLaunchTypeAtCreation.FROM_TAB_GROUP_UI:
                return TabLaunchType.FROM_TAB_GROUP_UI;
            case TabLaunchTypeAtCreation.FROM_TAB_SWITCHER_UI:
                return TabLaunchType.FROM_TAB_SWITCHER_UI;
            case TabLaunchTypeAtCreation.FROM_RESTORE_TABS_UI:
                return TabLaunchType.FROM_RESTORE_TABS_UI;
            case TabLaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND_IN_GROUP:
                return TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP;
            case TabLaunchTypeAtCreation.FROM_APP_WIDGET:
                return TabLaunchType.FROM_APP_WIDGET;
            case TabLaunchTypeAtCreation.FROM_RECENT_TABS:
                return TabLaunchType.FROM_RECENT_TABS;
            case TabLaunchTypeAtCreation.FROM_READING_LIST:
                return TabLaunchType.FROM_READING_LIST;
            case TabLaunchTypeAtCreation.FROM_OMNIBOX:
                return TabLaunchType.FROM_OMNIBOX;
            case TabLaunchTypeAtCreation.SIZE:
                return TabLaunchType.SIZE;
            case TabLaunchTypeAtCreation.UNKNOWN:
                return null;
            default:
                assert false
                        : "Unexpected deserialization of LaunchAtCreationType: "
                                + flatBufferLaunchType;
                // shouldn't happen
                return null;
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static int getLaunchTypeToFlatBuffer(@Nullable @TabLaunchType Integer tabLaunchType) {
        if (tabLaunchType == null) {
            return TabLaunchTypeAtCreation.UNKNOWN;
        }
        switch (tabLaunchType) {
            case TabLaunchType.FROM_LINK:
                return TabLaunchTypeAtCreation.FROM_LINK;
            case TabLaunchType.FROM_EXTERNAL_APP:
                return TabLaunchTypeAtCreation.FROM_EXTERNAL_APP;
            case TabLaunchType.FROM_CHROME_UI:
                return TabLaunchTypeAtCreation.FROM_CHROME_UI;
            case TabLaunchType.FROM_RESTORE:
                return TabLaunchTypeAtCreation.FROM_RESTORE;
            case TabLaunchType.FROM_LONGPRESS_FOREGROUND:
                return TabLaunchTypeAtCreation.FROM_LONGPRESS_FOREGROUND;
            case TabLaunchType.FROM_LONGPRESS_INCOGNITO:
                return TabLaunchTypeAtCreation.FROM_LONGPRESS_INCOGNITO;
            case TabLaunchType.FROM_LONGPRESS_BACKGROUND:
                return TabLaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND;
            case TabLaunchType.FROM_REPARENTING:
                return TabLaunchTypeAtCreation.FROM_REPARENTING;
            case TabLaunchType.FROM_LAUNCHER_SHORTCUT:
                return TabLaunchTypeAtCreation.FROM_LAUNCHER_SHORTCUT;
            case TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION:
                return TabLaunchTypeAtCreation.FROM_SPECULATIVE_BACKGROUND_CREATION;
            case TabLaunchType.FROM_BROWSER_ACTIONS:
                return TabLaunchTypeAtCreation.FROM_BROWSER_ACTIONS;
            case TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB:
                return TabLaunchTypeAtCreation.FROM_LAUNCH_NEW_INCOGNITO_TAB;
            case TabLaunchType.FROM_STARTUP:
                return TabLaunchTypeAtCreation.FROM_STARTUP;
            case TabLaunchType.FROM_START_SURFACE:
                return TabLaunchTypeAtCreation.FROM_START_SURFACE;
            case TabLaunchType.FROM_TAB_GROUP_UI:
                return TabLaunchTypeAtCreation.FROM_TAB_GROUP_UI;
            case TabLaunchType.FROM_TAB_SWITCHER_UI:
                return TabLaunchTypeAtCreation.FROM_TAB_SWITCHER_UI;
            case TabLaunchType.FROM_RESTORE_TABS_UI:
                return TabLaunchTypeAtCreation.FROM_RESTORE_TABS_UI;
            case TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP:
                return TabLaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND_IN_GROUP;
            case TabLaunchType.FROM_APP_WIDGET:
                return TabLaunchTypeAtCreation.FROM_APP_WIDGET;
            case TabLaunchType.FROM_RECENT_TABS:
                return TabLaunchTypeAtCreation.FROM_RECENT_TABS;
            case TabLaunchType.FROM_READING_LIST:
                return TabLaunchTypeAtCreation.FROM_READING_LIST;
            case TabLaunchType.FROM_OMNIBOX:
                return TabLaunchTypeAtCreation.FROM_OMNIBOX;
            case TabLaunchType.SIZE:
                return TabLaunchTypeAtCreation.SIZE;
            default:
                assert false : "Unexpected serialization of LaunchAtCreationType: " + tabLaunchType;
                // shouldn't happen
                return TabLaunchTypeAtCreation.UNKNOWN;
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static @TabUserAgent int getTabUserAgentTypeFromFlatBuffer(int flatbufferUserAgentType) {
        switch (flatbufferUserAgentType) {
            case UserAgentType.DEFAULT:
                return TabUserAgent.DEFAULT;
            case UserAgentType.MOBILE:
                return TabUserAgent.MOBILE;
            case UserAgentType.DESKTOP:
                return TabUserAgent.DESKTOP;
            case UserAgentType.UNSET:
                return TabUserAgent.UNSET;
            case UserAgentType.USER_AGENT_SIZE:
                return TabUserAgent.SIZE;
            default:
                assert false
                        : "Unexpected deserialization of UserAgentType: " + flatbufferUserAgentType;
                // shouldn't happen
                return TabUserAgent.DEFAULT;
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static int getUserAgentTypeToFlatBuffer(@TabUserAgent int userAgent) {
        switch (userAgent) {
            case TabUserAgent.DEFAULT:
                return UserAgentType.DEFAULT;
            case TabUserAgent.MOBILE:
                return UserAgentType.MOBILE;
            case TabUserAgent.DESKTOP:
                return UserAgentType.DESKTOP;
            case TabUserAgent.UNSET:
                return UserAgentType.UNSET;
            case TabUserAgent.SIZE:
                return UserAgentType.USER_AGENT_SIZE;
            default:
                assert false : "Unexpected serialization of UserAgentType: " + userAgent;
                // shouldn't happen
                return UserAgentType.USER_AGENT_UNKNOWN;
        }
    }
}

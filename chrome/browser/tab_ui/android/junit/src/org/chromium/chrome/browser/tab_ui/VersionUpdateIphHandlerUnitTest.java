// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.TabGroupMessageMetadata;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.MessageType;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.VersioningMessageController;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.Collections;
import java.util.List;
import java.util.Optional;

/** Unit tests for {@link VersionUpdateIphHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VersionUpdateIphHandlerUnitTest {
    private static final String FAKE_GROUP_TITLE = "FAKE_GROUP_TITLE";
    private static final Token FAKE_TOKEN = new Token(1L, 2L);
    private static final TabModelDotInfo DOT_INFO =
            new TabModelDotInfo(/* showDot= */ true, FAKE_GROUP_TITLE);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Profile mProfile;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private VersioningMessageController mVersioningMessageController;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;

    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private View mAnchorView;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mAnchorView = new View(context);

        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS)).thenReturn(true);

        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);

        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mTabGroupSyncService.getVersioningMessageController())
                .thenReturn(mVersioningMessageController);
        when(mVersioningMessageController.isInitialized()).thenReturn(true);
        when(mVersioningMessageController.shouldShowMessageUi(MessageType.VERSION_UPDATED_MESSAGE))
                .thenReturn(true);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getTabGroupTitle(any(Token.class))).thenReturn(FAKE_GROUP_TITLE);
        when(mTabModel.getProfile()).thenReturn(mProfile);
    }

    @Test
    public void testTabSwitcherButtonIph_isShown() {
        VersionUpdateIphHandler.maybeShowTabSwitcherButtonIph(
                mUserEducationHelper, mAnchorView, mProfile, DOT_INFO);

        verify(mUserEducationHelper).requestShowIph(any());
        verify(mVersioningMessageController).onMessageUiShown(MessageType.VERSION_UPDATED_MESSAGE);
    }

    @Test
    public void testTabSwitcherButtonIph_notShown_offTheRecord() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        VersionUpdateIphHandler.maybeShowTabSwitcherButtonIph(
                mUserEducationHelper, mAnchorView, mProfile, DOT_INFO);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    public void testTabSwitcherButtonIph_notShown_autoOpenEnabled() {
        when(mVersioningMessageController.isInitialized()).thenReturn(false);
        VersionUpdateIphHandler.maybeShowTabSwitcherButtonIph(
                mUserEducationHelper, mAnchorView, mProfile, DOT_INFO);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    public void testTabSwitcherButtonIph_notShown_controllerShouldNotShow() {
        when(mVersioningMessageController.shouldShowMessageUi(MessageType.VERSION_UPDATED_MESSAGE))
                .thenReturn(false);
        VersionUpdateIphHandler.maybeShowTabSwitcherButtonIph(
                mUserEducationHelper, mAnchorView, mProfile, DOT_INFO);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    public void testTabGroupPaneButtonIph_isShown() {
        when(mPrefService.getBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS)).thenReturn(false);
        PersistentMessage message = createMessage(FAKE_TOKEN);
        when(mMessagingBackendService.getMessages(Optional.of(MessageType.VERSION_UPDATED_MESSAGE)))
                .thenReturn(List.of(message));

        VersionUpdateIphHandler.maybeShowTabGroupPaneButtonIph(
                mUserEducationHelper, mTabGroupModelFilter, mAnchorView);
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());
        verify(mVersioningMessageController).onMessageUiShown(MessageType.VERSION_UPDATED_MESSAGE);
    }

    @Test
    public void testTabGroupPaneButtonIph_notShown_autoOpenEnabled() {
        when(mVersioningMessageController.shouldShowMessageUi(MessageType.VERSION_UPDATED_MESSAGE))
                .thenReturn(false);
        VersionUpdateIphHandler.maybeShowTabGroupPaneButtonIph(
                mUserEducationHelper, mTabGroupModelFilter, mAnchorView);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    public void testTabGroupPaneButtonIph_notShown_noMessages() {
        when(mMessagingBackendService.getMessages(Optional.of(MessageType.VERSION_UPDATED_MESSAGE)))
                .thenReturn(Collections.emptyList());
        VersionUpdateIphHandler.maybeShowTabGroupPaneButtonIph(
                mUserEducationHelper, mTabGroupModelFilter, mAnchorView);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    public void testTabGroupPaneButtonIph_notShown_noTitle() {
        PersistentMessage message = createMessage(FAKE_TOKEN);
        when(mMessagingBackendService.getMessages(Optional.of(MessageType.VERSION_UPDATED_MESSAGE)))
                .thenReturn(List.of(message));
        VersionUpdateIphHandler.maybeShowTabGroupPaneButtonIph(
                mUserEducationHelper, mTabGroupModelFilter, mAnchorView);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    private PersistentMessage createMessage(Token tabGroupId) {
        TabGroupMessageMetadata metadata = new TabGroupMessageMetadata();
        metadata.localTabGroupId = new LocalTabGroupId(tabGroupId);

        MessageAttribution attribution = new MessageAttribution();
        attribution.tabGroupMetadata = metadata;

        PersistentMessage message = new PersistentMessage();
        message.attribution = attribution;
        return message;
    }
}

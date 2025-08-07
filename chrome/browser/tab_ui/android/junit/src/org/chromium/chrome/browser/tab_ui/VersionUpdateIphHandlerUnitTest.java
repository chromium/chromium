// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.junit.Assert.assertEquals;
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
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.MessageType;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.VersioningMessageController;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.Collections;
import java.util.Set;

/** Unit tests for {@link VersionUpdateIphHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VersionUpdateIphHandlerUnitTest {
    private static final String FAKE_GROUP_TITLE = "FAKE_GROUP_TITLE";
    private static final Token FAKE_TOKEN = new Token(1L, 2L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Profile mProfile;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private VersioningMessageController mVersioningMessageController;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;

    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private Context mContext;
    private View mAnchorView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mAnchorView = new View(mContext);

        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS)).thenReturn(true);

        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);

        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mTabGroupSyncService.getVersioningMessageController())
                .thenReturn(mVersioningMessageController);
        when(mVersioningMessageController.isInitialized()).thenReturn(true);
        when(mVersioningMessageController.shouldShowMessageUi(MessageType.VERSION_UPDATED_MESSAGE))
                .thenReturn(true);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.tabGroupExists(FAKE_TOKEN)).thenReturn(true);
        when(mTabGroupModelFilter.getTabGroupTitle(FAKE_TOKEN)).thenReturn(FAKE_GROUP_TITLE);
        when(mTabModel.getProfile()).thenReturn(mProfile);

        SavedTabGroup group = new SavedTabGroup();
        group.collaborationId = "COLLABORATION_ID";
        group.title = FAKE_GROUP_TITLE;
        when(mTabGroupModelFilter.getAllTabGroupIds()).thenReturn(Set.of(FAKE_TOKEN));
        when(mTabGroupSyncService.getGroup(new LocalTabGroupId(FAKE_TOKEN))).thenReturn(group);
    }

    @Test
    public void testIph_autoOpenEnabled() {
        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mTabGroupModelFilter,
                /* expectsAutoOpen= */ true);

        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());

        IphCommand command = mIphCommandCaptor.getValue();
        String expectedText =
                mContext.getString(R.string.tab_group_update_iph_text, FAKE_GROUP_TITLE);
        assertEquals(expectedText, command.contentString);
        command.onShowCallback.run();

        verify(mVersioningMessageController).onMessageUiShown(MessageType.VERSION_UPDATED_MESSAGE);
    }

    @Test
    public void testIph_autoOpenDisabled() {
        when(mPrefService.getBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS)).thenReturn(false);
        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mTabGroupModelFilter,
                /* expectsAutoOpen= */ false);

        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());

        IphCommand command = mIphCommandCaptor.getValue();
        command.onShowCallback.run();

        verify(mVersioningMessageController).onMessageUiShown(MessageType.VERSION_UPDATED_MESSAGE);
    }

    @Test
    public void testIph_notShown() {
        when(mPrefService.getBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS)).thenReturn(false);
        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mTabGroupModelFilter,
                /* expectsAutoOpen= */ true);
        verify(mUserEducationHelper, never()).requestShowIph(any());

        when(mPrefService.getBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS)).thenReturn(true);
        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mTabGroupModelFilter,
                /* expectsAutoOpen= */ false);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    public void testIph_profileOffTheRecord() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mTabGroupModelFilter,
                /* expectsAutoOpen= */ true);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    public void testIph_controllerNotInitialized() {
        when(mVersioningMessageController.isInitialized()).thenReturn(false);
        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mTabGroupModelFilter,
                /* expectsAutoOpen= */ true);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    public void testIph_controllerShouldNotShow() {
        when(mVersioningMessageController.shouldShowMessageUi(MessageType.VERSION_UPDATED_MESSAGE))
                .thenReturn(false);
        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mTabGroupModelFilter,
                /* expectsAutoOpen= */ true);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    public void testIph_noTabGroups() {
        when(mTabGroupModelFilter.getAllTabGroupIds()).thenReturn(Collections.emptySet());
        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mTabGroupModelFilter,
                /* expectsAutoOpen= */ true);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    public void testIph_noSharedTabGroups() {
        SavedTabGroup group = new SavedTabGroup();
        group.collaborationId = null;
        group.title = FAKE_GROUP_TITLE;
        when(mTabGroupSyncService.getGroup(new LocalTabGroupId(FAKE_TOKEN))).thenReturn(group);

        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mTabGroupModelFilter,
                /* expectsAutoOpen= */ true);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }
}

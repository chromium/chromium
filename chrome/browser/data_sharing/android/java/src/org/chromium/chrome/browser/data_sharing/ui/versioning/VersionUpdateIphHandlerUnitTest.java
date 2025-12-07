// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.versioning;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.MessageType;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.VersioningMessageController;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/** Unit tests for {@link VersionUpdateIphHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VersionUpdateIphHandlerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Profile mProfile;
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
    }

    @Test
    public void testIph_autoOpenEnabled() {
        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mProfile,
                /* requiresAutoOpenSettingEnabled= */ true);

        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());

        IphCommand command = mIphCommandCaptor.getValue();
        String expectedText =
                mContext.getString(
                        R.string.collaboration_shared_tab_groups_available_again_iph_message);
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
                mProfile,
                /* requiresAutoOpenSettingEnabled= */ false);

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
                mProfile,
                /* requiresAutoOpenSettingEnabled= */ true);
        verify(mUserEducationHelper, never()).requestShowIph(any());

        when(mPrefService.getBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS)).thenReturn(true);
        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mProfile,
                /* requiresAutoOpenSettingEnabled= */ false);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    public void testIph_profileOffTheRecord() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mProfile,
                /* requiresAutoOpenSettingEnabled= */ true);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    public void testIph_controllerNotInitialized() {
        when(mVersioningMessageController.isInitialized()).thenReturn(false);
        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mProfile,
                /* requiresAutoOpenSettingEnabled= */ true);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    public void testIph_controllerShouldNotShow() {
        when(mVersioningMessageController.shouldShowMessageUi(MessageType.VERSION_UPDATED_MESSAGE))
                .thenReturn(false);
        VersionUpdateIphHandler.maybeShowVersioningIph(
                mUserEducationHelper,
                mAnchorView,
                mProfile,
                /* requiresAutoOpenSettingEnabled= */ true);
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }
}

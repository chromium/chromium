// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetProperties.VISIBLE;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link PostPasswordMigrationSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class PostPasswordMigrationSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    private final PostPasswordMigrationSheetMediator mMediator =
            new PostPasswordMigrationSheetMediator();

    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private PrefService mPrefService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJni);
        when(mUserPrefsJni.get(eq(mProfile))).thenReturn(mPrefService);
        mMediator.initialize(
                mProfile,
                PostPasswordMigrationSheetProperties.createDefaultModel(mMediator::onDismissed));
    }

    @Test
    public void testShowWarningChangesVisibility() {
        PropertyModel model = mMediator.getModel();
        assertFalse(model.get(VISIBLE));
        mMediator.showSheet();
        assertTrue(model.get(VISIBLE));
    }

    @Test
    public void testOnDismissedChangesVisibility() {
        PropertyModel model = mMediator.getModel();
        mMediator.showSheet();
        assertTrue(model.get(VISIBLE));
        mMediator.onDismissed(StateChangeReason.NONE);
        assertFalse(model.get(VISIBLE));
    }

    @Test
    public void testDismissHandlerChangesVisibility() {
        PropertyModel model = mMediator.getModel();
        assertNotNull(model.get(DISMISS_HANDLER));
        assertFalse(model.get(VISIBLE));
        mMediator.showSheet();
        assertTrue(model.get(VISIBLE));
        model.get(DISMISS_HANDLER).onResult(StateChangeReason.NONE);
        assertFalse(model.get(VISIBLE));
    }

    @Test
    public void testOnDismissedChangesThePref() {
        PropertyModel model = mMediator.getModel();
        mMediator.showSheet();
        assertTrue(model.get(VISIBLE));
        mMediator.onDismissed(StateChangeReason.SWIPE);
        assertFalse(model.get(VISIBLE));
        verify(mPrefService)
                .setBoolean(
                        eq(Pref.SHOULD_SHOW_POST_PASSWORD_MIGRATION_SHEET_AT_STARTUP), eq(false));
    }

    @Test
    public void testOnDismissedDoesNotChangeThePrefIfSheetWasNotShown() {
        PropertyModel model = mMediator.getModel();
        assertFalse(model.get(VISIBLE));
        mMediator.onDismissed(StateChangeReason.NONE);
        assertFalse(model.get(VISIBLE));
        verify(mPrefService, times(0))
                .setBoolean(
                        eq(Pref.SHOULD_SHOW_POST_PASSWORD_MIGRATION_SHEET_AT_STARTUP),
                        anyBoolean());
    }
}

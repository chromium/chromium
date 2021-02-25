// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.ALL_KEYS;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.DUPLICATE_USERNAME_ERROR;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.EMPTY_PASSWORD_ERROR;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.FEDERATION_ORIGIN;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD_VISIBLE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.UI_ACTION_HANDLER;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.UI_DISMISSED_BY_NATIVE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.URL_OR_APP;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.USERNAME;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.password_entry_edit.CredentialEditCoordinator.CredentialActionDelegate;
import org.chromium.chrome.browser.password_manager.settings.PasswordAccessReauthenticationHelper;
import org.chromium.chrome.browser.password_manager.settings.PasswordAccessReauthenticationHelper.ReauthReason;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests verifying that the credential edit mediator modifies the model correctly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CredentialEditControllerTest {
    private static final String TEST_URL = "https://m.a.xyz/signin";
    private static final String TEST_USERNAME = "TestUsername";
    private static final String NEW_TEST_USERNAME = "TestNewUsername";
    private static final String TEST_PASSWORD = "TestPassword";
    private static final String NEW_TEST_PASSWORD = "TestNewPassword";

    @Mock
    private PasswordAccessReauthenticationHelper mReauthenticationHelper;

    @Mock
    private CredentialActionDelegate mCredentialActionDelegate;

    CredentialEditMediator mMediator;
    PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMediator = new CredentialEditMediator(mReauthenticationHelper, mCredentialActionDelegate);
        mModel = new PropertyModel.Builder(ALL_KEYS)
                         .with(UI_ACTION_HANDLER, mMediator)
                         .with(URL_OR_APP, TEST_URL)
                         .with(FEDERATION_ORIGIN, "")
                         .build();
        mMediator.initialize(mModel);
    }

    @Test
    public void testSetsCredential() {
        mMediator.setCredential(TEST_USERNAME, TEST_PASSWORD);
        assertEquals(TEST_USERNAME, mModel.get(USERNAME));
        assertEquals(TEST_PASSWORD, mModel.get(PASSWORD));
        assertFalse(mModel.get(PASSWORD_VISIBLE));
    }

    @Test
    public void testDismissPropagatesToTheModel() {
        mMediator.dismiss();
        assertTrue(mModel.get(UI_DISMISSED_BY_NATIVE));
    }

    @Test
    public void testMaskingWithoutReauth() {
        mModel.set(PASSWORD_VISIBLE, true);
        mMediator.onMaskOrUnmaskPassword();
        verify(mReauthenticationHelper, never()).canReauthenticate();
        verify(mReauthenticationHelper, never()).reauthenticate(anyInt(), any(Callback.class));
    }

    @Test
    public void testCannotReauthPromptsToast() {
        when(mReauthenticationHelper.canReauthenticate()).thenReturn(false);
        mModel.set(PASSWORD_VISIBLE, false);
        mMediator.onMaskOrUnmaskPassword();
        verify(mReauthenticationHelper).showScreenLockToast(eq(ReauthReason.VIEW_PASSWORD));
        verify(mReauthenticationHelper, never()).reauthenticate(anyInt(), any(Callback.class));
    }

    @Test
    public void testUnmaskTriggersReauthenticate() {
        when(mReauthenticationHelper.canReauthenticate()).thenReturn(true);
        mModel.set(PASSWORD_VISIBLE, false);
        mMediator.onMaskOrUnmaskPassword();
        verify(mReauthenticationHelper)
                .reauthenticate(eq(ReauthReason.VIEW_PASSWORD), any(Callback.class));
    }

    @Test
    public void testCannotUnmaskIfReauthFailed() {
        when(mReauthenticationHelper.canReauthenticate()).thenReturn(true);
        mModel.set(PASSWORD_VISIBLE, false);
        doAnswer((invocation) -> {
            Callback callback = (Callback) invocation.getArguments()[1];
            callback.onResult(false);
            return null;
        })
                .when(mReauthenticationHelper)
                .reauthenticate(eq(ReauthReason.VIEW_PASSWORD), any(Callback.class));
        mMediator.onMaskOrUnmaskPassword();
        verify(mReauthenticationHelper)
                .reauthenticate(eq(ReauthReason.VIEW_PASSWORD), any(Callback.class));
        assertFalse(mModel.get(PASSWORD_VISIBLE));
    }

    @Test
    public void testCopyPasswordTriggersReauth() {
        when(mReauthenticationHelper.canReauthenticate()).thenReturn(true);
        mMediator.onCopyPassword(ApplicationProvider.getApplicationContext());
        verify(mReauthenticationHelper)
                .reauthenticate(eq(ReauthReason.COPY_PASSWORD), any(Callback.class));
    }

    @Test
    public void testCantCopyPasswordIfReauthFails() {
        mModel.set(PASSWORD, TEST_PASSWORD);
        when(mReauthenticationHelper.canReauthenticate()).thenReturn(true);
        doAnswer((invocation) -> {
            Callback callback = (Callback) invocation.getArguments()[1];
            callback.onResult(false);
            return null;
        })
                .when(mReauthenticationHelper)
                .reauthenticate(eq(ReauthReason.COPY_PASSWORD), any(Callback.class));

        Context context = ApplicationProvider.getApplicationContext();
        mMediator.onCopyPassword(context);

        verify(mReauthenticationHelper)
                .reauthenticate(eq(ReauthReason.COPY_PASSWORD), any(Callback.class));
        ClipboardManager clipboard =
                (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
        assertNull(clipboard.getPrimaryClip());
    }

    @Test
    public void testCanCopyPasswordIfReauthSucceeds() {
        mModel.set(PASSWORD, TEST_PASSWORD);
        when(mReauthenticationHelper.canReauthenticate()).thenReturn(true);
        doAnswer((invocation) -> {
            Callback callback = (Callback) invocation.getArguments()[1];
            callback.onResult(true);
            return null;
        })
                .when(mReauthenticationHelper)
                .reauthenticate(eq(ReauthReason.COPY_PASSWORD), any(Callback.class));
        Context context = ApplicationProvider.getApplicationContext();
        mMediator.onCopyPassword(context);

        ClipboardManager clipboard =
                (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData expectedClip = ClipData.newPlainText("password", TEST_PASSWORD);
        assertEquals(expectedClip.toString(), clipboard.getPrimaryClip().toString());
    }

    @Test
    public void callsTheDelegateWithCorrectDataWhenSaving() {
        mModel.set(USERNAME, TEST_USERNAME);
        mModel.set(PASSWORD, TEST_PASSWORD);
        mMediator.onSave();
        verify(mCredentialActionDelegate).saveChanges(TEST_USERNAME, TEST_PASSWORD);
    }

    @Test
    public void testUsernameTextChangedUpdatesModel() {
        mMediator.setCredential(TEST_USERNAME, TEST_PASSWORD);
        mMediator.setExistingUsernames(new String[] {TEST_USERNAME});
        mMediator.onUsernameTextChanged(NEW_TEST_USERNAME);
        assertEquals(NEW_TEST_USERNAME, mModel.get(USERNAME));
    }

    @Test
    public void testPasswordTextChangedUpdatesModel() {
        mMediator.setCredential(TEST_USERNAME, TEST_PASSWORD);
        mMediator.onPasswordTextChanged(NEW_TEST_PASSWORD);
        assertEquals(NEW_TEST_PASSWORD, mModel.get(PASSWORD));
    }

    @Test
    public void testEmptyPasswordTriggersError() {
        mMediator.setCredential(TEST_USERNAME, TEST_PASSWORD);
        mMediator.onPasswordTextChanged("");
        assertTrue(mModel.get(EMPTY_PASSWORD_ERROR));

        mMediator.onPasswordTextChanged(TEST_PASSWORD);
        assertFalse(mModel.get(EMPTY_PASSWORD_ERROR));
    }

    @Test
    public void testDuplicateUsernameTriggersError() {
        mMediator.setCredential(TEST_USERNAME, TEST_PASSWORD);
        mMediator.setExistingUsernames(new String[] {TEST_USERNAME, NEW_TEST_USERNAME});

        mMediator.onUsernameTextChanged(NEW_TEST_USERNAME);
        assertTrue(mModel.get(DUPLICATE_USERNAME_ERROR));

        mMediator.onUsernameTextChanged(TEST_USERNAME);
        assertFalse(mModel.get(DUPLICATE_USERNAME_ERROR));
    }
}

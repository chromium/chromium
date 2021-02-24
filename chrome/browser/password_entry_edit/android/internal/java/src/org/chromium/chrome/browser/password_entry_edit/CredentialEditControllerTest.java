// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.ALL_KEYS;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.FEDERATION_ORIGIN;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD_VISIBLE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.UI_ACTION_HANDLER;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.UI_DISMISSED_BY_NATIVE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.URL_OR_APP;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.USERNAME;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
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
    private static final String TEST_PASSWORD = "TestPassword";

    @Mock
    private PasswordAccessReauthenticationHelper mReauthenticationHelper;

    CredentialEditMediator mMediator;
    PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMediator = new CredentialEditMediator(mReauthenticationHelper);
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
    public void testCanReauthTriggersReauthenticate() {
        when(mReauthenticationHelper.canReauthenticate()).thenReturn(true);
        mModel.set(PASSWORD_VISIBLE, false);
        mMediator.onMaskOrUnmaskPassword();
        verify(mReauthenticationHelper)
                .reauthenticate(eq(ReauthReason.VIEW_PASSWORD), any(Callback.class));
    }
}

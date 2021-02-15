// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.ALL_KEYS;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.FEDERATION_ORIGIN;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD_VISIBLE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.UI_DISMISSED_BY_NATIVE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.URL_OR_APP;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.USERNAME;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
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

    CredentialEditMediator mMediator;
    PropertyModel mModel;

    @Before
    public void setUp() {
        mMediator = new CredentialEditMediator();
        mModel = new PropertyModel.Builder(ALL_KEYS)
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
}

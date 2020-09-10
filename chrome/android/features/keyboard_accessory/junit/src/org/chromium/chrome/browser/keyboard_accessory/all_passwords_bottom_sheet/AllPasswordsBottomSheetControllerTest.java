// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.VISIBLE;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.ItemType;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Controller tests for the all passwords bottom sheet.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.FILLING_PASSWORDS_FROM_ANY_ORIGIN)
public class AllPasswordsBottomSheetControllerTest {
    private static final Credential ANA =
            new Credential("Ana", "S3cr3t", "Ana", "https://m.a.xyz/", true, false);
    private static final Credential BOB =
            new Credential("Bob", "*****", "Bob", "https://subdomain.example.xyz", true, false);
    private static final Credential CARL =
            new Credential("Carl", "G3h3!m", "Carl", "https://www.example.xyz", false, false);
    private static final Credential[] TEST_CREDENTIALS = new Credential[] {ANA, BOB, CARL};
    private static final boolean IS_PASSWORD_FIELD = true;

    @Mock
    private AllPasswordsBottomSheetCoordinator.Delegate mMockDelegate;

    private AllPasswordsBottomSheetMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMediator = new AllPasswordsBottomSheetMediator();
        mModel = AllPasswordsBottomSheetProperties.createDefaultModel(mMediator::onDismissed);
        mMediator.initialize(mMockDelegate, mModel);
    }

    @Test
    public void testCreatesValidDefaultModel() {
        assertNotNull(mModel.get(SHEET_ITEMS));
        assertNotNull(mModel.get(DISMISS_HANDLER));
        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testShowCredentialsSetsVisibile() {
        mMediator.showCredentials(TEST_CREDENTIALS, IS_PASSWORD_FIELD);
        assertThat(mModel.get(VISIBLE), is(true));
    }

    @Test
    public void testShowCredentialSetsCredentialListModel() {
        mMediator.showCredentials(TEST_CREDENTIALS, IS_PASSWORD_FIELD);
        ListModel<ListItem> itemList = mModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(3));

        assertThat(itemList.get(0).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(0).model.get(CREDENTIAL), is(ANA));
        assertThat(itemList.get(1).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(1).model.get(CREDENTIAL), is(BOB));
        assertThat(itemList.get(2).type, is(ItemType.CREDENTIAL));
        assertThat(itemList.get(2).model.get(CREDENTIAL), is(CARL));
    }

    @Test
    public void testOnCredentialSelected() {
        mMediator.showCredentials(TEST_CREDENTIALS, IS_PASSWORD_FIELD);
        mMediator.onCredentialSelected(TEST_CREDENTIALS[1]);
        assertThat(mModel.get(VISIBLE), is(false));
        verify(mMockDelegate).onCredentialSelected(TEST_CREDENTIALS[1]);
    }

    @Test
    public void testOnDismiss() {
        mMediator.showCredentials(TEST_CREDENTIALS, IS_PASSWORD_FIELD);
        mMediator.onDismissed(BottomSheetController.StateChangeReason.BACK_PRESS);
        assertThat(mModel.get(VISIBLE), is(false));
        verify(mMockDelegate).onDismissed();
    }
}

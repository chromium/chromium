// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerProperties.AddAccountRowProperties;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerProperties.ExistingAccountRowProperties;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.AccountTrackerService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests the class {@link AccountPickerMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.DEPRECATE_MENAGERIE_API})
public class AccountPickerMediatorTest {
    private static final String FULL_NAME1 = "Test Account1";
    private static final String FULL_NAME2 = "Test Account2";
    private static final String ACCOUNT_EMAIL1 = "test.account1@gmail.com";
    private static final String ACCOUNT_EMAIL2 = "test.account2@gmail.com";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final Features.JUnitProcessor processor = new Features.JUnitProcessor();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeAccountInfoService());

    @Mock
    private AccountPickerCoordinator.Listener mListenerMock;

    private final MVCListAdapter.ModelList mModelList = new MVCListAdapter.ModelList();

    private AccountPickerMediator mMediator;

    @Before
    public void setUp() {
        AccountInfoServiceProvider.init(
                mock(IdentityManager.class), mock(AccountTrackerService.class));
    }

    @After
    public void tearDown() {
        if (mMediator != null) {
            mMediator.destroy();
        }
        AccountInfoServiceProvider.resetForTests();
    }

    @Test
    public void testModelPopulation() {
        mAccountManagerTestRule.addAccount(ACCOUNT_EMAIL1, FULL_NAME1, null, null);
        mAccountManagerTestRule.addAccount(ACCOUNT_EMAIL2, FULL_NAME2, null, null);
        mMediator = new AccountPickerMediator(
                RuntimeEnvironment.application, mModelList, mListenerMock);
        // ACCOUNT_NAME1, ACCOUNT_NAME2, ADD_ACCOUNT.
        Assert.assertEquals(3, mModelList.size());
        checkItemForExistingAccountRow(0, ACCOUNT_EMAIL1, FULL_NAME1);
        checkItemForExistingAccountRow(1, ACCOUNT_EMAIL2, FULL_NAME2);
        checkItemForAddAccountRow(2);
    }

    @Test
    public void testProfileDataUpdateWhenAccountPickerIsShownFromSettings() {
        mAccountManagerTestRule.addAccount(ACCOUNT_EMAIL1, FULL_NAME1, null, null);
        mAccountManagerTestRule.addAccount(ACCOUNT_EMAIL2, FULL_NAME2, null, null);
        mMediator = new AccountPickerMediator(
                RuntimeEnvironment.application, mModelList, mListenerMock);
        String newFullName2 = "Full Name2";
        mAccountManagerTestRule.addAccount(ACCOUNT_EMAIL2, newFullName2, "", null);
        // ACCOUNT_NAME1, ACCOUNT_NAME2, ADD_ACCOUNT
        Assert.assertEquals(3, mModelList.size());
        checkItemForExistingAccountRow(0, ACCOUNT_EMAIL1, FULL_NAME1);
        checkItemForExistingAccountRow(1, ACCOUNT_EMAIL2, newFullName2);
        checkItemForAddAccountRow(2);
    }

    private void checkItemForExistingAccountRow(
            int position, String accountEmail, String fullName) {
        MVCListAdapter.ListItem item = mModelList.get(position);
        Assert.assertEquals(AccountPickerProperties.ItemType.EXISTING_ACCOUNT_ROW, item.type);
        PropertyModel model = item.model;
        DisplayableProfileData profileData = model.get(ExistingAccountRowProperties.PROFILE_DATA);
        Assert.assertEquals(accountEmail, profileData.getAccountEmail());
        Assert.assertEquals(fullName, profileData.getFullName());
        Assert.assertNotNull("Profile avatar should not be null!", profileData.getImage());

        model.get(ExistingAccountRowProperties.ON_CLICK_LISTENER).onResult(profileData);
        verify(mListenerMock).onAccountSelected(accountEmail, position == 0);
    }

    private void checkItemForAddAccountRow(int position) {
        MVCListAdapter.ListItem item = mModelList.get(position);
        Assert.assertEquals(AccountPickerProperties.ItemType.ADD_ACCOUNT_ROW, item.type);
        item.model.get(AddAccountRowProperties.ON_CLICK_LISTENER).onClick(null);
        verify(mListenerMock).addAccount();
    }
}

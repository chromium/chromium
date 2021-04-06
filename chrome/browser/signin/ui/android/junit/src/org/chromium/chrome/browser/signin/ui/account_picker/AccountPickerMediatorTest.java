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
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.identitymanager.AccountInfoService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests the class {@link AccountPickerMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.DisableFeatures({ChromeFeatureList.DEPRECATE_MENAGERIE_API})
public class AccountPickerMediatorTest {
    private static final String FULL_NAME1 = "Test Account1";
    private static final String ACCOUNT_EMAIL1 = "test.account1@gmail.com";
    private static final String ACCOUNT_EMAIL2 = "test.account2@gmail.com";

    private static class CustomProfileDataSource extends FakeProfileDataSource {
        void removeProfileData(String accountEmail) {
            mProfileDataMap.remove(accountEmail);
            for (Observer observer : mObservers) {
                observer.removeProfileData(accountEmail);
            }
        }
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final Features.JUnitProcessor processor = new Features.JUnitProcessor();

    private final CustomProfileDataSource mFakeProfileDataSource = new CustomProfileDataSource();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeProfileDataSource);

    @Mock
    private AccountPickerCoordinator.Listener mListenerMock;

    private final MVCListAdapter.ModelList mModelList = new MVCListAdapter.ModelList();

    private AccountPickerMediator mMediator;

    @Before
    public void setUp() {
        AccountInfoService.init(mock(IdentityManager.class));
    }

    @After
    public void tearDown() {
        if (mMediator != null) {
            mMediator.destroy();
        }
        AccountInfoService.resetForTests();
    }

    @Test
    public void testModelPopulatedWhenStartedFromWeb() {
        addAccount(ACCOUNT_EMAIL1, FULL_NAME1);
        addAccount(ACCOUNT_EMAIL2, "");
        mMediator = new AccountPickerMediator(
                RuntimeEnvironment.application, mModelList, mListenerMock, ACCOUNT_EMAIL1, true);
        // ACCOUNT_NAME1, ACCOUNT_NAME2, ADD_ACCOUNT, INCOGNITO MODE.
        Assert.assertEquals(4, mModelList.size());
        checkItemForExistingAccountRow(
                0, ACCOUNT_EMAIL1, FULL_NAME1, /* isSelectedAccount= */ true);
        checkItemForExistingAccountRow(1, ACCOUNT_EMAIL2, "", /* isSelectedAccount= */ false);
        checkItemForAddAccountRow(2);
        checkItemForIncognitoAccountRow(3);
    }

    @Test
    public void testModelPopulatedWhenStartedFromSettings() {
        addAccount(ACCOUNT_EMAIL1, FULL_NAME1);
        addAccount(ACCOUNT_EMAIL2, "");
        mMediator = new AccountPickerMediator(
                RuntimeEnvironment.application, mModelList, mListenerMock, ACCOUNT_EMAIL1, false);
        // ACCOUNT_NAME1, ACCOUNT_NAME2, ADD_ACCOUNT
        Assert.assertEquals(3, mModelList.size());
        checkItemForExistingAccountRow(
                0, ACCOUNT_EMAIL1, FULL_NAME1, /* isSelectedAccount= */ true);
        checkItemForExistingAccountRow(1, ACCOUNT_EMAIL2, "", /* isSelectedAccount= */ false);
        checkItemForAddAccountRow(2);
    }

    @Test
    public void testModelUpdatedAfterSetSelectedAccountNameFromSettings() {
        addAccount(ACCOUNT_EMAIL1, FULL_NAME1);
        addAccount(ACCOUNT_EMAIL2, "");
        mMediator = new AccountPickerMediator(
                RuntimeEnvironment.application, mModelList, mListenerMock, ACCOUNT_EMAIL1, false);
        mMediator.setSelectedAccountName(ACCOUNT_EMAIL2);
        // ACCOUNT_NAME1, ACCOUNT_NAME2, ADD_ACCOUNT
        Assert.assertEquals(3, mModelList.size());
        checkItemForExistingAccountRow(
                0, ACCOUNT_EMAIL1, FULL_NAME1, /* isSelectedAccount= */ false);
        checkItemForExistingAccountRow(1, ACCOUNT_EMAIL2, "", /* isSelectedAccount= */ true);
        checkItemForAddAccountRow(2);
    }

    @Test
    public void testProfileDataUpdateWhenAccountPickerIsShownFromSettings() {
        addAccount(ACCOUNT_EMAIL1, FULL_NAME1);
        addAccount(ACCOUNT_EMAIL2, "");
        mMediator = new AccountPickerMediator(
                RuntimeEnvironment.application, mModelList, mListenerMock, ACCOUNT_EMAIL1, false);
        String fullName2 = "Full Name2";
        mFakeProfileDataSource.addProfileData(
                new ProfileDataSource.ProfileData(ACCOUNT_EMAIL2, null, fullName2, null));
        // ACCOUNT_NAME1, ACCOUNT_NAME2, ADD_ACCOUNT
        Assert.assertEquals(3, mModelList.size());
        checkItemForExistingAccountRow(
                0, ACCOUNT_EMAIL1, FULL_NAME1, /* isSelectedAccount= */ true);
        checkItemForExistingAccountRow(
                1, ACCOUNT_EMAIL2, fullName2, /* isSelectedAccount= */ false);
        checkItemForAddAccountRow(2);
    }

    @Test
    public void testProfileDataRemovedWhenAccountPickerIsShownFromSettings() {
        addAccount(ACCOUNT_EMAIL1, FULL_NAME1);
        final String fullName2 = "Full Name2";
        addAccount(ACCOUNT_EMAIL2, fullName2);
        mMediator = new AccountPickerMediator(
                RuntimeEnvironment.application, mModelList, mListenerMock, ACCOUNT_EMAIL1, false);
        mFakeProfileDataSource.removeProfileData(ACCOUNT_EMAIL1);
        // ACCOUNT_NAME1, ACCOUNT_NAME2, ADD_ACCOUNT
        Assert.assertEquals(3, mModelList.size());
        checkItemForExistingAccountRow(0, ACCOUNT_EMAIL1, null, /* isSelectedAccount= */ true);
        checkItemForExistingAccountRow(
                1, ACCOUNT_EMAIL2, fullName2, /* isSelectedAccount= */ false);
        checkItemForAddAccountRow(2);
    }

    private void checkItemForExistingAccountRow(
            int position, String accountEmail, String fullName, boolean isSelectedAccount) {
        MVCListAdapter.ListItem item = mModelList.get(position);
        Assert.assertEquals(AccountPickerProperties.ItemType.EXISTING_ACCOUNT_ROW, item.type);
        PropertyModel model = item.model;
        DisplayableProfileData profileData = model.get(ExistingAccountRowProperties.PROFILE_DATA);
        Assert.assertEquals(accountEmail, profileData.getAccountEmail());
        Assert.assertEquals(fullName, profileData.getFullName());
        Assert.assertNotNull("Profile avatar should not be null!", profileData.getImage());
        Assert.assertEquals(
                isSelectedAccount, model.get(ExistingAccountRowProperties.IS_SELECTED_ACCOUNT));

        model.get(ExistingAccountRowProperties.ON_CLICK_LISTENER).onResult(profileData);
        verify(mListenerMock).onAccountSelected(accountEmail, position == 0);
    }

    private void checkItemForAddAccountRow(int position) {
        MVCListAdapter.ListItem item = mModelList.get(position);
        Assert.assertEquals(AccountPickerProperties.ItemType.ADD_ACCOUNT_ROW, item.type);
        item.model.get(AddAccountRowProperties.ON_CLICK_LISTENER).onClick(null);
        verify(mListenerMock).addAccount();
    }

    private void checkItemForIncognitoAccountRow(int position) {
        MVCListAdapter.ListItem item = mModelList.get(position);
        Assert.assertEquals(AccountPickerProperties.ItemType.INCOGNITO_ACCOUNT_ROW, item.type);
        item.model.get(AccountPickerProperties.IncognitoAccountRowProperties.ON_CLICK_LISTENER)
                .onClick(null);
        verify(mListenerMock).goIncognitoMode();
    }

    private void addAccount(String accountName, String fullName) {
        ProfileDataSource.ProfileData profileData =
                new ProfileDataSource.ProfileData(accountName, null, fullName, null);
        mAccountManagerTestRule.addAccount(profileData);
    }
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.VISIBLE;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.ItemType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Controller tests for the all passwords bottom sheet. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.FILLING_PASSWORDS_FROM_ANY_ORIGIN)
public class AllPasswordsBottomSheetControllerTest {
    private static final Credential ANA =
            new Credential(
                    /* username= */ "ana@gmail.com",
                    /* password= */ "S3cr3t",
                    /* formattedUsername= */ "ana@gmail.com",
                    /* originUrl= */ "https://m.domain.xyz/",
                    /* isAndroidCredential= */ false,
                    /* appDisplayName= */ "",
                    /* isPlusAddressUsername= */ true);
    private static final Credential BOB =
            new Credential(
                    /* username= */ "Bob",
                    /* password= */ "*****",
                    /* formattedUsername= */ "Bob",
                    /* originUrl= */ "https://subdomain.example.xyz",
                    /* isAndroidCredential= */ false,
                    /* appDisplayName= */ "",
                    /* isPlusAddressUsername= */ false);
    private static final Credential CARL =
            new Credential(
                    /* username= */ "Carl",
                    /* password= */ "G3h3!m",
                    /* formattedUsername= */ "Carl",
                    /* originUrl= */ "https://www.origin.xyz",
                    /* isAndroidCredential= */ false,
                    /* appDisplayName= */ "",
                    /* isPlusAddressUsername= */ false);
    private static final List<Credential> TEST_CREDENTIALS = List.of(BOB, CARL, ANA);
    private static final boolean IS_PASSWORD_FIELD = true;
    private static final String EXAMPLE_ORIGIN = "https://m.example.com/";

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock private AllPasswordsBottomSheetCoordinator.Delegate mMockDelegate;

    private AllPasswordsBottomSheetMediator mMediator;
    private PropertyModel mModel;
    private ListModel<ListItem> mListModel;
    private PropertyModel mModalDialogModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);
        mMediator = new AllPasswordsBottomSheetMediator();
        mModel =
                AllPasswordsBottomSheetProperties.createDefaultModel(
                        EXAMPLE_ORIGIN, mMediator::onDismissed, mMediator::onQueryTextChange);
        mListModel = new ListModel<>();
        mMediator.initialize(mMockDelegate, mModel, mListModel);

        when(mUrlUtilitiesJniMock.getDomainAndRegistry(anyString(), anyBoolean()))
                .then(inv -> getDomainAndRegistry(inv.getArgument(0)));
    }

    @Test
    public void testCreatesValidDefaultModel() {
        assertNotNull(mListModel);
        assertNotNull(mModel.get(DISMISS_HANDLER));
        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testShowCredentialSetsCredentialListModel() {
        mMediator.showCredentials(new ArrayList<>(TEST_CREDENTIALS), IS_PASSWORD_FIELD);

        ListModel<ListItem> itemList = mListModel;
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
        mMediator.showCredentials(new ArrayList<>(TEST_CREDENTIALS), IS_PASSWORD_FIELD);
        CredentialFillRequest request = new CredentialFillRequest(TEST_CREDENTIALS.get(1), true);
        mMediator.onCredentialSelected(request);
        assertThat(mModel.get(VISIBLE), is(false));
        verify(mMockDelegate).onCredentialSelected(request);
    }

    @Test
    public void testOnDismiss() {
        mMediator.showCredentials(new ArrayList<>(TEST_CREDENTIALS), IS_PASSWORD_FIELD);
        mMediator.onDismissed(BottomSheetController.StateChangeReason.BACK_PRESS);
        assertThat(mModel.get(VISIBLE), is(false));
        verify(mMockDelegate).onDismissed();
    }

    @Test
    public void testSearchFilterByUsername() {
        mMediator.showCredentials(new ArrayList<>(TEST_CREDENTIALS), IS_PASSWORD_FIELD);
        mMediator.onQueryTextChange("Bob");
        assertThat(mListModel.size(), is(1));
    }

    @Test
    public void testSearchFilterByURL() {
        mMediator.showCredentials(new ArrayList<>(TEST_CREDENTIALS), IS_PASSWORD_FIELD);
        mMediator.onQueryTextChange("subdomain");
        assertThat(mListModel.size(), is(1));
    }

    @Test
    public void testCredentialSortedByOrigin() {
        mMediator.showCredentials(new ArrayList<>(TEST_CREDENTIALS), IS_PASSWORD_FIELD);

        ListModel<ListItem> itemList = mListModel;

        assertThat(itemList.get(0).model.get(CREDENTIAL), is(ANA));
        assertThat(itemList.get(1).model.get(CREDENTIAL), is(BOB));
        assertThat(itemList.get(2).model.get(CREDENTIAL), is(CARL));
    }

    /**
     * Helper to get organization-identifying host from URLs. The real implementation calls {@link
     * UrlUtilities}. It's not useful to actually reimplement it, so just return a string in a
     * trivial way.
     *
     * @param origin A URL.
     * @return The organization-identifying host from the given URL.
     */
    private String getDomainAndRegistry(String origin) {
        return origin.replaceAll(".*\\.(.+\\.[^.]+$)", "$1");
    }
}

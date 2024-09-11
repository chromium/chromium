// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.List;

/**
 * These tests render screenshots of the {@link SaveUpdateAddressProfilePrompt} and compare them to
 * a gold standard.
 */
@DoNotBatch(reason = "The tests can't be batched because they run for different set-ups.")
@RunWith(ParameterizedRunner.class)
public class SaveUpdateAddressProfilePromptRenderTest extends BlankUiTestActivityTestCase {
    private static final long NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER = 100L;

    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(0)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private SaveUpdateAddressProfilePromptController.Natives mPromptControllerJni;
    @Mock private AutofillProfileBridge.Natives mAutofillProfileBridgeJni;
    @Mock private PersonalDataManager mPersonalDataManager;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SyncService mSyncService;

    private SaveUpdateAddressProfilePromptController mPromptController;
    private SaveUpdateAddressProfilePrompt mPrompt;

    public SaveUpdateAddressProfilePromptRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    @Override
    public void setUpTest() throws Exception {
        MockitoAnnotations.initMocks(this);
        runOnUiThreadBlocking(
                () -> {
                    PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);
                    when(mPersonalDataManager.getDefaultCountryCodeForNewAddress())
                            .thenReturn("US");
                    SyncServiceFactory.setInstanceForTesting(mSyncService);
                    IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
                    when(mIdentityServicesProvider.getIdentityManager(any()))
                            .thenReturn(mIdentityManager);
                });

        mPromptController =
                SaveUpdateAddressProfilePromptController.create(
                        NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER);

        mJniMocker.mock(
                SaveUpdateAddressProfilePromptControllerJni.TEST_HOOKS, mPromptControllerJni);
        mJniMocker.mock(AutofillProfileBridgeJni.TEST_HOOKS, mAutofillProfileBridgeJni);
        super.setUpTest();
    }

    @After
    @Override
    public void tearDownTest() throws Exception {
        runOnUiThreadBlocking(mPrompt::dismiss);
        super.tearDownTest();
    }

    @AfterClass
    public static void tearDownClass() throws Exception {
        runOnUiThreadBlocking(NightModeTestUtils::tearDownNightModeForBlankUiTestActivity);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void saveLocalOrSyncAddress() throws Exception {
        View dialogView =
                runOnUiThreadBlocking(
                        () -> {
                            mPrompt =
                                    new SaveUpdateAddressProfilePrompt(
                                            mPromptController,
                                            getActivity().getModalDialogManager(),
                                            getActivity(),
                                            mProfile,
                                            AutofillProfile.builder().build(),
                                            /* isUpdate= */ false,
                                            /* isMigrationToAccount= */ false);
                            mPrompt.setDialogDetails(
                                    /* title= */ "Dialog title",
                                    /* positiveButtonText= */ "Accept",
                                    /* negativeButtonText= */ "Cancel");
                            mPrompt.setSaveOrMigrateDetails(
                                    /* address= */ "321 Spear Street",
                                    /* email= */ "example@example.com",
                                    /* phone= */ "+0000000000000");
                            mPrompt.show();
                            return mPrompt.getDialogViewForTesting();
                        });
        mRenderTestRule.render(dialogView, "save_local_or_sync_address");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void saveAccountAddress() throws Exception {
        View dialogView =
                runOnUiThreadBlocking(
                        () -> {
                            mPrompt =
                                    new SaveUpdateAddressProfilePrompt(
                                            mPromptController,
                                            getActivity().getModalDialogManager(),
                                            getActivity(),
                                            mProfile,
                                            AutofillProfile.builder().build(),
                                            /* isUpdate= */ false,
                                            /* isMigrationToAccount= */ false);
                            mPrompt.setDialogDetails(
                                    /* title= */ "Dialog title",
                                    /* positiveButtonText= */ "Accept",
                                    /* negativeButtonText= */ "Cancel");
                            mPrompt.setSaveOrMigrateDetails(
                                    /* address= */ "321 Spear Street",
                                    /* email= */ "example@example.com",
                                    /* phone= */ "+0000000000000");
                            mPrompt.setRecordTypeNotice(
                                    getActivity()
                                            .getString(
                                                    R.string
                                                            .autofill_address_will_be_saved_in_account_record_type_notice)
                                            .replace("$1", "example@gmail.com"));
                            mPrompt.show();
                            return mPrompt.getDialogViewForTesting();
                        });
        mRenderTestRule.render(dialogView, "save_account_address");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void migrateAddress() throws Exception {
        View dialogView =
                runOnUiThreadBlocking(
                        () -> {
                            mPrompt =
                                    new SaveUpdateAddressProfilePrompt(
                                            mPromptController,
                                            getActivity().getModalDialogManager(),
                                            getActivity(),
                                            mProfile,
                                            AutofillProfile.builder().build(),
                                            /* isUpdate= */ false,
                                            /* isMigrationToAccount= */ true);
                            mPrompt.setDialogDetails(
                                    /* title= */ "Dialog title",
                                    /* positiveButtonText= */ "Accept",
                                    /* negativeButtonText= */ "Cancel");
                            mPrompt.setSaveOrMigrateDetails(
                                    /* address= */ "321 Spear Street",
                                    /* email= */ "example@example.com",
                                    /* phone= */ "+0000000000000");
                            mPrompt.setRecordTypeNotice(
                                    getActivity()
                                            .getString(
                                                    R.string
                                                            .autofill_address_will_be_saved_in_account_record_type_notice)
                                            .replace("$1", "example@gmail.com"));
                            mPrompt.show();
                            return mPrompt.getDialogViewForTesting();
                        });
        mRenderTestRule.render(dialogView, "migrate_address");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void updateLocalOrSyncAddress() throws Exception {
        View dialogView =
                runOnUiThreadBlocking(
                        () -> {
                            mPrompt =
                                    new SaveUpdateAddressProfilePrompt(
                                            mPromptController,
                                            getActivity().getModalDialogManager(),
                                            getActivity(),
                                            mProfile,
                                            AutofillProfile.builder().build(),
                                            /* isUpdate= */ true,
                                            /* isMigrationToAccount= */ false);
                            mPrompt.setDialogDetails(
                                    /* title= */ "Dialog title",
                                    /* positiveButtonText= */ "Accept",
                                    /* negativeButtonText= */ "Cancel");
                            mPrompt.setUpdateDetails(
                                    /* subtitle= */ "Update your address",
                                    /* oldDetails= */ "321 Spear Street",
                                    /* newDetails= */ "123 Lake Street");
                            mPrompt.show();
                            return mPrompt.getDialogViewForTesting();
                        });
        mRenderTestRule.render(dialogView, "update_local_or_sync_address");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void updateAccountAddress() throws Exception {
        View dialogView =
                runOnUiThreadBlocking(
                        () -> {
                            mPrompt =
                                    new SaveUpdateAddressProfilePrompt(
                                            mPromptController,
                                            getActivity().getModalDialogManager(),
                                            getActivity(),
                                            mProfile,
                                            AutofillProfile.builder().build(),
                                            /* isUpdate= */ true,
                                            /* isMigrationToAccount= */ false);
                            mPrompt.setDialogDetails(
                                    /* title= */ "Dialog title",
                                    /* positiveButtonText= */ "Accept",
                                    /* negativeButtonText= */ "Cancel");
                            mPrompt.setUpdateDetails(
                                    /* subtitle= */ "Update your address",
                                    /* oldDetails= */ "321 Spear Street",
                                    /* newDetails= */ "123 Lake Street");
                            mPrompt.setRecordTypeNotice(
                                    getActivity()
                                            .getString(
                                                    R.string
                                                            .autofill_address_already_saved_in_account_record_type_notice)
                                            .replace("$1", "example@gmail.com"));
                            mPrompt.show();
                            return mPrompt.getDialogViewForTesting();
                        });
        mRenderTestRule.render(dialogView, "update_local_or_sync_address");
    }
}

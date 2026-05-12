// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.content.Context;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.RecordType;
import org.chromium.components.autofill.autofill_ai.utils.TestUtils;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.List;

/**
 * These tests render screenshots of the {@link AutofillAiSaveUpdateEntityPrompt} and compare them
 * to a gold standard.
 */
@DoNotBatch(reason = "The tests can't be batched because they run for different set-ups.")
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({
    ChromeFeatureList.AUTOFILL_AI_EDIT_ENTITIES_FROM_SAVE_UPDATE_PROMPT,
    ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA
})
public class AutofillAiSaveUpdateEntityPromptRenderTest {
    private static final long NATIVE_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER = 100L;

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutofillAiSaveUpdateEntityPromptController.Natives mPromptControllerJni;
    @Mock private Profile mProfile;
    @Mock private IdentityManager mIdentityManager;
    @Mock private PersonalDataManager mPersonalDataManager;

    private final EntityInstance mEntity =
            new EntityInstance.Builder(TestUtils.getVehicleEntityType())
                    .setGuid("")
                    .setRecordType(RecordType.LOCAL)
                    .setUseCount(0)
                    .build();
    private AutofillAiSaveUpdateEntityPromptController mPromptController;
    private AutofillAiSaveUpdateEntityPrompt mPrompt;

    public AutofillAiSaveUpdateEntityPromptRenderTest(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startOnBlankPage();
        mActivityTestRule.waitForActivityCompletelyLoaded();

        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManager);
        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);

        mPromptController =
                AutofillAiSaveUpdateEntityPromptController.create(
                        NATIVE_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER);

        AutofillAiSaveUpdateEntityPromptControllerJni.setInstanceForTesting(mPromptControllerJni);
    }

    @After
    public void tearDown() throws Exception {
        runOnUiThreadBlocking(mPrompt::dismiss);
    }

    @AfterClass
    public static void tearDownClass() throws Exception {
        runOnUiThreadBlocking(NightModeTestUtils::tearDownNightModeForBlankUiTestActivity);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void saveEntityWithData() throws Exception {
        View dialogView =
                runOnUiThreadBlocking(
                        () -> {
                            mPrompt =
                                    new AutofillAiSaveUpdateEntityPrompt(
                                            mPromptController,
                                            mActivityTestRule.getActivity().getModalDialogManager(),
                                            mActivityTestRule.getActivity(),
                                            mProfile,
                                            mEntity);
                            mPrompt.setDialogDetails(
                                    /* title= */ "Dialog title",
                                    /* positiveButtonText= */ "Accept",
                                    /* negativeButtonText= */ "Cancel",
                                    /* isWalletableEntity= */ false);
                            mPrompt.setEntityUpdateDetails(
                                    List.of(
                                            new EntityAttributeUpdateDetails(
                                                    "Country",
                                                    "Ukraine",
                                                    "",
                                                    EntityAttributeUpdateType
                                                            .NEW_ENTITY_ATTRIBUTE_UNCHANGED),
                                            new EntityAttributeUpdateDetails(
                                                    "Number",
                                                    "AA123456",
                                                    "",
                                                    EntityAttributeUpdateType
                                                            .NEW_ENTITY_ATTRIBUTE_UNCHANGED),
                                            new EntityAttributeUpdateDetails(
                                                    "Issue date",
                                                    "Oct. 10, 2019",
                                                    "",
                                                    EntityAttributeUpdateType
                                                            .NEW_ENTITY_ATTRIBUTE_UNCHANGED),
                                            new EntityAttributeUpdateDetails(
                                                    "Expiration date",
                                                    "Oct. 10, 2029",
                                                    "",
                                                    EntityAttributeUpdateType
                                                            .NEW_ENTITY_ATTRIBUTE_UNCHANGED)),
                                    /* isUpdatePrompt= */ false);
                            mPrompt.setSourceNotice(
                                    "Saved to this device", /* insertManageInfoLink= */ false);
                            mPrompt.show();
                            return mPrompt.getDialogViewForTesting();
                        });
        mRenderTestRule.render(dialogView, "autofill_ai_save_entity_with_data");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void updateWalletEntity() throws Exception {
        View dialogView =
                runOnUiThreadBlocking(
                        () -> {
                            mPrompt =
                                    new AutofillAiSaveUpdateEntityPrompt(
                                            mPromptController,
                                            mActivityTestRule.getActivity().getModalDialogManager(),
                                            mActivityTestRule.getActivity(),
                                            mProfile,
                                            mEntity);
                            mPrompt.setDialogDetails(
                                    /* title= */ "Dialog title",
                                    /* positiveButtonText= */ "Accept",
                                    /* negativeButtonText= */ "Cancel",
                                    /* isWalletableEntity= */ false);
                            mPrompt.setEntityUpdateDetails(
                                    List.of(
                                            new EntityAttributeUpdateDetails(
                                                    "Country",
                                                    "Ukraine",
                                                    "",
                                                    EntityAttributeUpdateType
                                                            .NEW_ENTITY_ATTRIBUTE_ADDED),
                                            new EntityAttributeUpdateDetails(
                                                    "Number",
                                                    "AA123456",
                                                    "BB123456",
                                                    EntityAttributeUpdateType
                                                            .NEW_ENTITY_ATTRIBUTE_UPDATED),
                                            new EntityAttributeUpdateDetails(
                                                    "Issue date",
                                                    "Oct. 10, 2019",
                                                    "",
                                                    EntityAttributeUpdateType
                                                            .NEW_ENTITY_ATTRIBUTE_UNCHANGED),
                                            new EntityAttributeUpdateDetails(
                                                    "Expiration date",
                                                    "Oct. 10, 2029",
                                                    "",
                                                    EntityAttributeUpdateType
                                                            .NEW_ENTITY_ATTRIBUTE_UNCHANGED)),
                                    /* isUpdatePrompt= */ true);
                            Context context = mActivityTestRule.getActivity();
                            String walletTitle =
                                    context.getString(R.string.autofill_google_wallet_title);
                            String email = "alexpark@gmail.com";
                            String sourceNotice =
                                    context.getString(
                                                    R.string
                                                            .autofill_ai_save_or_update_entity_in_wallet_source_notice)
                                            .replace("$1", walletTitle)
                                            .replace("$2", walletTitle)
                                            .replace("$3", email);
                            mPrompt.setSourceNotice(sourceNotice, /* insertManageInfoLink= */ true);
                            mPrompt.show();
                            return mPrompt.getDialogViewForTesting();
                        });
        mRenderTestRule.render(dialogView, "autofill_ai_update_entity");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void updateWalletEntityWithoutNewAttributes() throws Exception {
        View dialogView =
                runOnUiThreadBlocking(
                        () -> {
                            mPrompt =
                                    new AutofillAiSaveUpdateEntityPrompt(
                                            mPromptController,
                                            mActivityTestRule.getActivity().getModalDialogManager(),
                                            mActivityTestRule.getActivity(),
                                            mProfile,
                                            mEntity);
                            mPrompt.setDialogDetails(
                                    /* title= */ "Dialog title",
                                    /* positiveButtonText= */ "Accept",
                                    /* negativeButtonText= */ "Cancel",
                                    /* isWalletableEntity= */ false);
                            mPrompt.setEntityUpdateDetails(
                                    List.of(
                                            new EntityAttributeUpdateDetails(
                                                    "Country",
                                                    "Ukraine",
                                                    "Germany",
                                                    EntityAttributeUpdateType
                                                            .NEW_ENTITY_ATTRIBUTE_UPDATED),
                                            new EntityAttributeUpdateDetails(
                                                    "Number",
                                                    "AA123456",
                                                    "BB123456",
                                                    EntityAttributeUpdateType
                                                            .NEW_ENTITY_ATTRIBUTE_UPDATED),
                                            new EntityAttributeUpdateDetails(
                                                    "Issue date",
                                                    "Oct. 10, 2019",
                                                    "",
                                                    EntityAttributeUpdateType
                                                            .NEW_ENTITY_ATTRIBUTE_UNCHANGED),
                                            new EntityAttributeUpdateDetails(
                                                    "Expiration date",
                                                    "Oct. 10, 2029",
                                                    "",
                                                    EntityAttributeUpdateType
                                                            .NEW_ENTITY_ATTRIBUTE_UNCHANGED)),
                                    /* isUpdatePrompt= */ true);
                            Context context = mActivityTestRule.getActivity();
                            String walletTitle =
                                    context.getString(R.string.autofill_google_wallet_title);
                            String email = "alexpark@gmail.com";
                            String sourceNotice =
                                    context.getString(
                                                    R.string
                                                            .autofill_ai_save_or_update_entity_in_wallet_source_notice)
                                            .replace("$1", walletTitle)
                                            .replace("$2", walletTitle)
                                            .replace("$3", email);
                            mPrompt.setSourceNotice(sourceNotice, /* insertManageInfoLink= */ true);
                            mPrompt.show();
                            return mPrompt.getDialogViewForTesting();
                        });
        mRenderTestRule.render(dialogView, "autofill_ai_update_entity_without_new_attributes");
    }
}

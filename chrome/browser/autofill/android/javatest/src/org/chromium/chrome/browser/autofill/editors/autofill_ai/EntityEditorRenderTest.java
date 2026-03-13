// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import static org.mockito.ArgumentMatchers.anyInt;
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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge.Natives;
import org.chromium.chrome.browser.autofill.AutofillProfileBridgeJni;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorCoordinator.Delegate;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.autofill.DropdownKeyValue;
import org.chromium.components.autofill.autofill_ai.AttributeInstance;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.utils.TestUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.time.LocalDate;
import java.time.ZoneId;
import java.util.HashSet;
import java.util.List;

/**
 * These tests render screenshots of the autofill AI entity editor and compare them to a gold
 * standard.
 */
@DoNotBatch(reason = "The tests can't be batched because they run for different set-ups.")
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class EntityEditorRenderTest {
    private static final String USER_EMAIL = "example@gmail.com";

    private static final EntityInstance sNewEntityInstance =
            new EntityInstance.Builder(
                            TestUtils.getPassportEntityType(
                                    /* isReadOnly= */ false, /* isEnabled= */ true))
                    .setModifiedDate(LocalDate.now(ZoneId.systemDefault()))
                    .setUseCount(0)
                    .build();

    private static final EntityInstance sExistingEntityInstance =
            new EntityInstance.Builder(
                            TestUtils.getPassportEntityType(
                                    /* isReadOnly= */ false, /* isEnabled= */ true))
                    .setModifiedDate(LocalDate.now(ZoneId.systemDefault()))
                    .setUseCount(0)
                    .addAttribute(
                            new AttributeInstance(
                                    TestUtils.getPassportNameAttributeType(), "John Doe"))
                    .addAttribute(
                            new AttributeInstance(
                                    TestUtils.getPassportCountryAttributeType(), "Germany"))
                    .addAttribute(
                            new AttributeInstance(
                                    TestUtils.getPassportNumberAttributeType(), "AA123456"))
                    .build();

    // Note: can't initialize this list statically because of how Robolectric
    // initializes Android library dependencies.
    private static final List<DropdownKeyValue> sSupportedCountries =
            List.of(
                    new DropdownKeyValue("US", "United States"),
                    new DropdownKeyValue("DE", "Germany"),
                    new DropdownKeyValue("CU", "Cuba"));

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Natives mAutofillProfileBridgeJni;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SyncService mSyncService;
    @Mock private PersonalDataManager mPersonalDataManager;
    @Mock private Profile mProfile;
    @Mock private Delegate mDelegate;

    private EntityEditorCoordinator mEntityEditor;

    private final CoreAccountInfo mAccountInfo =
            CoreAccountInfo.createFromEmailAndGaiaId(USER_EMAIL, new GaiaId("gaia_id"));

    public EntityEditorRenderTest(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startOnBlankPage();
        mActivityTestRule.waitForActivityCompletelyLoaded();

        AutofillProfileBridgeJni.setInstanceForTesting(mAutofillProfileBridgeJni);
        when(mAutofillProfileBridgeJni.getSupportedCountries()).thenReturn(sSupportedCountries);

        runOnUiThreadBlocking(
                () -> {
                    when(mSyncService.getSelectedTypes()).thenReturn(new HashSet());
                    SyncServiceFactory.setInstanceForTesting(mSyncService);

                    when(mPersonalDataManager.getDefaultCountryCodeForNewAddress())
                            .thenReturn("US");
                    PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);

                    ProfileManager.setLastUsedProfileForTesting(mProfile);
                    IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
                    when(mIdentityServicesProvider.getIdentityManager(mProfile))
                            .thenReturn(mIdentityManager);
                    when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(mAccountInfo);
                });
    }

    @After
    public void tearDown() {
        mActivityTestRule.skipWindowAndTabStateCleanup();
    }

    @AfterClass
    public static void tearDownClass() throws Exception {
        runOnUiThreadBlocking(NightModeTestUtils::tearDownNightModeForBlankUiTestActivity);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void editNewEntity() throws Exception {
        View editor =
                runOnUiThreadBlocking(
                        () -> {
                            mEntityEditor =
                                    new EntityEditorCoordinator(
                                            mActivityTestRule.getActivity(),
                                            mDelegate,
                                            mProfile,
                                            sNewEntityInstance);
                            mEntityEditor.showEditorDialog();
                            return mEntityEditor.getEntityEditorViewForTest().getContentView();
                        });
        mRenderTestRule.render(editor, "edit_new_entity");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void saveEmptyEntity() throws Exception {
        View editor =
                runOnUiThreadBlocking(
                        () -> {
                            mEntityEditor =
                                    new EntityEditorCoordinator(
                                            mActivityTestRule.getActivity(),
                                            mDelegate,
                                            mProfile,
                                            sNewEntityInstance);
                            mEntityEditor.showEditorDialog();
                            mEntityEditor
                                    .getEntityEditorViewForTest()
                                    .getContainerView()
                                    .findViewById(R.id.editor_dialog_done_button)
                                    .performClick();
                            return mEntityEditor.getEntityEditorViewForTest().getContentView();
                        });
        mRenderTestRule.render(editor, "save_empty_entity");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void editExistingEntity() throws Exception {
        View editor =
                runOnUiThreadBlocking(
                        () -> {
                            mEntityEditor =
                                    new EntityEditorCoordinator(
                                            mActivityTestRule.getActivity(),
                                            mDelegate,
                                            mProfile,
                                            sExistingEntityInstance);
                            mEntityEditor.showEditorDialog();
                            return mEntityEditor.getEntityEditorViewForTest().getContentView();
                        });
        mRenderTestRule.render(editor, "edit_existing_entity");
    }
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FORMATTED_ORIGIN;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.SHOW_SUBMIT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.MANAGE_BUTTON_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.SUBTITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.TITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.SHOW_WEBAUTHN_SUBMIT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.WEBAUTHN_CREDENTIAL;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import static java.util.Arrays.asList;

import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.password_manager.GetLoginMatchType;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProvider;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebauthnCredential;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.List;

/** These tests render screenshots of touch to fill sheet and compare them to a gold standard. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@DoNotBatch(reason = "The tests can't be batched because they run for different set-ups.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchToFillRenderTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"),
                    new ParameterSet().value(true, false).name("NightMode"));

    private static final GURL TEST_URL = JUnitTestGURLs.EXAMPLE_URL;
    private static final Credential ARON =
            new Credential("אהרן", "S3cr3t", "אהרן", "", "example.com", GetLoginMatchType.EXACT, 0);
    private static final Credential BOB =
            new Credential("Bob", "*****", "Bob", "", "example.com", GetLoginMatchType.EXACT, 0);
    private static final Credential MARIAM =
            new Credential("مريم", "***", "مريم", "", "example.com", GetLoginMatchType.EXACT, 0);
    private static final byte[] RANDOM_ID = new byte[] {0};
    private static final WebauthnCredential BATMAN =
            new WebauthnCredential("example.com", RANDOM_ID, RANDOM_ID, "batman");
    private static final WebauthnCredential PETROL =
            new WebauthnCredential("example.com", RANDOM_ID, RANDOM_ID, "petrol");
    private static final WebauthnCredential SPOR =
            new WebauthnCredential("example.com", RANDOM_ID, RANDOM_ID, "spor");

    @Mock private Callback<Integer> mDismissHandler;
    @Mock private Callback<Credential> mCredentialCallback;

    private PropertyModel mModel;
    private TouchToFillView mTouchToFillView;
    private BottomSheetController mBottomSheetController;
    PasswordManagerResourceProvider mResourceProvider;

    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(8)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    public TouchToFillRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        setRtlForTesting(useRtlLayout);
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.waitForActivityCompletelyLoaded();
        mBottomSheetController =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
        mResourceProvider = PasswordManagerResourceProviderFactory.create();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel = TouchToFillProperties.createDefaultModel(mDismissHandler);
                    mTouchToFillView =
                            new TouchToFillView(
                                    mActivityTestRule.getActivity(), mBottomSheetController);
                    TouchToFillCoordinator.setUpModelChangeProcessors(mModel, mTouchToFillView);
                });
    }

    @After
    public void tearDown() {
        setRtlForTesting(false);
        try {
            ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
        } catch (Exception e) {
            // Activity was already closed (e.g. due to last test tearing down the suite).
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsOneCredentialModern() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    addHeader(
                            mActivityTestRule
                                    .getActivity()
                                    .getString(R.string.touch_to_fill_sheet_uniform_title));
                    mModel.get(SHEET_ITEMS).addAll(asList(buildCredentialItem(ARON)));
                    addButton(ARON);
                    addFooter();
                    mModel.set(VISIBLE, true);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "ttf_shows_one_credential_modern_ui");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsOneCredentialModernHalfState() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    addHeader(
                            mActivityTestRule
                                    .getActivity()
                                    .getString(R.string.touch_to_fill_sheet_uniform_title));
                    mModel.get(SHEET_ITEMS).addAll(asList(buildCredentialItem(ARON)));
                    addButton(ARON);
                    addFooter();
                    mModel.set(VISIBLE, true);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ViewGroup bottomSheetParentView =
                (ViewGroup)
                        mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet).getParent();
        mRenderTestRule.render(
                bottomSheetParentView, "ttf_shows_one_credential_modern_ui_half_state");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsTwoCredentialsModern() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    addHeader(
                            mActivityTestRule
                                    .getActivity()
                                    .getString(R.string.touch_to_fill_sheet_uniform_title));
                    mModel.get(SHEET_ITEMS);
                    mModel.get(SHEET_ITEMS)
                            .addAll(asList(buildCredentialItem(ARON), buildCredentialItem(BOB)));
                    addFooter();
                    mModel.set(VISIBLE, true);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "ttf_shows_two_credentials_modern_ui");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsTwoCredentialsModernHalfState() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    addHeader(
                            mActivityTestRule
                                    .getActivity()
                                    .getString(R.string.touch_to_fill_sheet_uniform_title));
                    mModel.get(SHEET_ITEMS);
                    mModel.get(SHEET_ITEMS)
                            .addAll(asList(buildCredentialItem(ARON), buildCredentialItem(BOB)));
                    addFooter();
                    mModel.set(VISIBLE, true);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ViewGroup bottomSheetParentView =
                (ViewGroup)
                        mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet).getParent();
        mRenderTestRule.render(
                bottomSheetParentView, "ttf_shows_two_credentials_modern_ui_half_state");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsThreeCredentialsModern() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    addHeader(
                            mActivityTestRule
                                    .getActivity()
                                    .getString(R.string.touch_to_fill_sheet_uniform_title));
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ARON),
                                            buildCredentialItem(BOB),
                                            buildCredentialItem(MARIAM)));
                    addFooter();
                    mModel.set(VISIBLE, true);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "ttf_shows_three_credentials_modern_ui");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsThreeCredentialsModernHalfState() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    addHeader(
                            mActivityTestRule
                                    .getActivity()
                                    .getString(R.string.touch_to_fill_sheet_uniform_title));
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ARON),
                                            buildCredentialItem(BOB),
                                            buildCredentialItem(MARIAM)));
                    addFooter();
                    mModel.set(VISIBLE, true);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ViewGroup bottomSheetParentView =
                (ViewGroup)
                        mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet).getParent();
        mRenderTestRule.render(
                bottomSheetParentView, "ttf_shows_three_credentials_modern_ui_half_state");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsThreeCredentialsWhenThereAreFiveModernHalfState() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    addHeader(
                            mActivityTestRule
                                    .getActivity()
                                    .getString(R.string.touch_to_fill_sheet_uniform_title));
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildWebAuthnCredentialItem(BATMAN),
                                            buildWebAuthnCredentialItem(PETROL),
                                            buildWebAuthnCredentialItem(SPOR),
                                            buildCredentialItem(ARON),
                                            buildCredentialItem(BOB)));
                    addFooter();
                    mModel.set(VISIBLE, true);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ViewGroup bottomSheetParentView =
                (ViewGroup)
                        mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet).getParent();
        mRenderTestRule.render(
                bottomSheetParentView,
                "ttf_shows_three_credentials_when_there_are_five_modern_ui_half_state");
    }

    private MVCListAdapter.ListItem buildCredentialItem(Credential credential) {
        return buildSheetItem(
                TouchToFillProperties.ItemType.CREDENTIAL, credential, mCredentialCallback, false);
    }

    private MVCListAdapter.ListItem buildWebAuthnCredentialItem(WebauthnCredential credential) {
        return new MVCListAdapter.ListItem(
                TouchToFillProperties.ItemType.WEBAUTHN_CREDENTIAL,
                new PropertyModel.Builder(
                                TouchToFillProperties.WebAuthnCredentialProperties.ALL_KEYS)
                        .with(WEBAUTHN_CREDENTIAL, credential)
                        .with(SHOW_WEBAUTHN_SUBMIT_BUTTON, false)
                        .build());
    }

    private static MVCListAdapter.ListItem buildSheetItem(
            @TouchToFillProperties.ItemType int itemType,
            Credential credential,
            Callback<Credential> callback,
            boolean showSubmitButton) {
        return new MVCListAdapter.ListItem(
                itemType,
                new PropertyModel.Builder(TouchToFillProperties.CredentialProperties.ALL_KEYS)
                        .with(CREDENTIAL, credential)
                        .with(ON_CLICK_LISTENER, callback)
                        .with(FORMATTED_ORIGIN, credential.getDisplayName())
                        .with(SHOW_SUBMIT_BUTTON, showSubmitButton)
                        .build());
    }

    private void addHeader(String title) {
        mModel.get(SHEET_ITEMS)
                .add(
                        new MVCListAdapter.ListItem(
                                TouchToFillProperties.ItemType.HEADER,
                                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                                        .with(TITLE, title)
                                        .with(SUBTITLE, TEST_URL.getSpec())
                                        .with(
                                                IMAGE_DRAWABLE_ID,
                                                mResourceProvider.getPasswordManagerIcon())
                                        .build()));
    }

    private void addButton(Credential credential) {
        mModel.get(SHEET_ITEMS)
                .add(
                        new MVCListAdapter.ListItem(
                                TouchToFillProperties.ItemType.FILL_BUTTON,
                                new PropertyModel.Builder(CredentialProperties.ALL_KEYS)
                                        .with(CREDENTIAL, ARON)
                                        .with(
                                                ON_CLICK_LISTENER,
                                                (Credential clickedCredential) -> {})
                                        .with(FORMATTED_ORIGIN, credential.getDisplayName())
                                        .with(SHOW_SUBMIT_BUTTON, true)
                                        .build()));
    }

    private void addFooter() {
        mModel.get(SHEET_ITEMS)
                .add(
                        new MVCListAdapter.ListItem(
                                TouchToFillProperties.ItemType.FOOTER,
                                new PropertyModel.Builder(FooterProperties.ALL_KEYS)
                                        .with(
                                                MANAGE_BUTTON_TEXT,
                                                mActivityTestRule
                                                        .getActivity()
                                                        .getString(R.string.manage_passwords))
                                        .build()));
    }
}

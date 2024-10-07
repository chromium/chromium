// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createClickActionWithFlags;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FORMATTED_ORIGIN;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.ITEM_COLLECTION_INFO;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.SHOW_SUBMIT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.MANAGE_BUTTON_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.ON_CLICK_HYBRID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.ON_CLICK_MANAGE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.SHOW_HYBRID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.SUBTITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.TITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.WEBAUTHN_CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.WEBAUTHN_ITEM_COLLECTION_INFO;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import static java.util.Arrays.asList;

import android.text.method.PasswordTransformationMethod;
import android.view.MotionEvent;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
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
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.GetLoginMatchType;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebauthnCredential;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.webauthn.cred_man.CredManSupportProvider;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * View tests for the Touch To Fill component ensure that model changes are reflected in the sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchToFillViewTest {
    private static final Credential ANA =
            new Credential("Ana", "S3cr3t", "Ana", "", "example.xyz", GetLoginMatchType.EXACT, 0);
    private static final Credential NO_ONE =
            new Credential(
                    "",
                    "***",
                    "No Username",
                    "m.example.xyz",
                    "m.example.xyz",
                    GetLoginMatchType.PSL,
                    0);
    private static final Credential BOB =
            new Credential(
                    "Bob",
                    "***",
                    "Bob",
                    "mobile.example.xyz",
                    "mobile.example.xyz",
                    GetLoginMatchType.PSL,
                    0);
    private static final WebauthnCredential CAM =
            new WebauthnCredential("example.net", new byte[] {1}, new byte[] {2}, "Cam");
    private static final Credential NIK =
            new Credential(
                    "Nik", "***", "Nik", "group.xyz", "group.xyz", GetLoginMatchType.AFFILIATED, 0);
    private final AtomicBoolean mManageButtonClicked = new AtomicBoolean(false);
    private final AtomicBoolean mHybridButtonClicked = new AtomicBoolean(false);
    private final AtomicBoolean mMorePasskeysClicked = new AtomicBoolean(false);

    @Mock private Callback<Integer> mDismissHandler;
    @Mock private Callback<Credential> mCredentialCallback;
    @Mock private FillableItemCollectionInfo mItemCollectionInfo;

    private PropertyModel mModel;
    private TouchToFillView mTouchToFillView;
    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mSheetTestSupport;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mBottomSheetController =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
        mSheetTestSupport = new BottomSheetTestSupport(mBottomSheetController);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel = TouchToFillProperties.createDefaultModel(mDismissHandler);
                    mTouchToFillView = new TouchToFillView(getActivity(), mBottomSheetController);
                    TouchToFillCoordinator.setUpModelChangeProcessors(mModel, mTouchToFillView);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThread(
                () -> {
                    AccessibilityState.setIsTouchExplorationEnabledForTesting(false);
                });
    }

    @Test
    @MediumTest
    public void testVisibilityChangedByModel() {
        // After setting the visibility to true, the view should exist and be visible.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, mItemCollectionInfo),
                                            buildConfirmationButton(ANA, true),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        assertThat(mTouchToFillView.getContentView().isShown(), is(true));

        // After hiding the view, the view should still exist but be invisible.
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        assertThat(mTouchToFillView.getContentView().isShown(), is(false));
    }

    @Test
    @MediumTest
    public void testTitlePropagatesToView() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            new MVCListAdapter.ListItem(
                                                    TouchToFillProperties.ItemType.HEADER,
                                                    new PropertyModel.Builder(
                                                                    HeaderProperties.ALL_KEYS)
                                                            .with(
                                                                    TITLE,
                                                                    getActivity()
                                                                            .getString(
                                                                                    R.string
                                                                                            .touch_to_fill_sheet_uniform_title))
                                                            .with(
                                                                    SUBTITLE,
                                                                    getActivity()
                                                                            .getString(
                                                                                    R.string
                                                                                            .touch_to_fill_sheet_subtitle_submission,
                                                                                    "www.example.org"))
                                                            .with(
                                                                    IMAGE_DRAWABLE_ID,
                                                                    R.drawable
                                                                            .touch_to_fill_header_image)
                                                            .build()),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView title =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_sheet_title);

        assertThat(
                title.getText(),
                is(getActivity().getString(R.string.touch_to_fill_sheet_uniform_title)));
    }

    @Test
    @MediumTest
    public void testManageButtonTextPropagatesToView() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            new MVCListAdapter.ListItem(
                                                    TouchToFillProperties.ItemType.HEADER,
                                                    new PropertyModel.Builder(
                                                                    HeaderProperties.ALL_KEYS)
                                                            .with(
                                                                    TITLE,
                                                                    getActivity()
                                                                            .getString(
                                                                                    R.string
                                                                                            .touch_to_fill_sheet_uniform_title))
                                                            .with(
                                                                    SUBTITLE,
                                                                    getActivity()
                                                                            .getString(
                                                                                    R.string
                                                                                            .touch_to_fill_sheet_subtitle_submission,
                                                                                    "www.example.org"))
                                                            .with(
                                                                    IMAGE_DRAWABLE_ID,
                                                                    R.drawable
                                                                            .touch_to_fill_header_image)
                                                            .build()),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView manageButtonText =
                mTouchToFillView
                        .getContentView()
                        .findViewById(R.id.touch_to_fill_sheet_manage_passwords);

        assertThat(
                manageButtonText.getText(),
                is(getActivity().getString(R.string.manage_passwords_and_passkeys)));
    }

    @Test
    @MediumTest
    public void testSecureSubtitleUrlDisplayed() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            new MVCListAdapter.ListItem(
                                                    TouchToFillProperties.ItemType.HEADER,
                                                    new PropertyModel.Builder(
                                                                    HeaderProperties.ALL_KEYS)
                                                            .with(SUBTITLE, "www.example.org")
                                                            .with(
                                                                    IMAGE_DRAWABLE_ID,
                                                                    R.drawable
                                                                            .touch_to_fill_header_image)
                                                            .build()),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView subtitle =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_sheet_subtitle);

        assertThat(subtitle.getText().toString(), is("www.example.org"));
    }

    @Test
    @MediumTest
    public void testNonSecureSubtitleUrlDisplayed() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            new MVCListAdapter.ListItem(
                                                    TouchToFillProperties.ItemType.HEADER,
                                                    new PropertyModel.Builder(
                                                                    HeaderProperties.ALL_KEYS)
                                                            .with(
                                                                    SUBTITLE,
                                                                    getActivity()
                                                                            .getString(
                                                                                    R.string
                                                                                            .touch_to_fill_sheet_subtitle_not_secure,
                                                                                    "m.example.org"))
                                                            .with(
                                                                    IMAGE_DRAWABLE_ID,
                                                                    R.drawable
                                                                            .touch_to_fill_header_image)
                                                            .build()),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView subtitle =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_sheet_subtitle);

        assertThat(subtitle.getText().toString(), is("m.example.org (not secure)"));
    }

    @Test
    @MediumTest
    public void testSubmissionSubtitleUrlDisplayed() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            new MVCListAdapter.ListItem(
                                                    TouchToFillProperties.ItemType.HEADER,
                                                    new PropertyModel.Builder(
                                                                    HeaderProperties.ALL_KEYS)
                                                            .with(
                                                                    SUBTITLE,
                                                                    getActivity()
                                                                            .getString(
                                                                                    R.string
                                                                                            .touch_to_fill_sheet_subtitle_submission,
                                                                                    "m.example.org"))
                                                            .with(
                                                                    IMAGE_DRAWABLE_ID,
                                                                    R.drawable
                                                                            .touch_to_fill_header_image)
                                                            .build()),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView subtitle =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_sheet_subtitle);

        assertThat(subtitle.getText().toString(), is("You'll sign in to m.example.org"));
    }

    @Test
    @MediumTest
    public void testNonSecureSubmissionSubtitleUrlDisplayed() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            new MVCListAdapter.ListItem(
                                                    TouchToFillProperties.ItemType.HEADER,
                                                    new PropertyModel.Builder(
                                                                    HeaderProperties.ALL_KEYS)
                                                            .with(
                                                                    SUBTITLE,
                                                                    getActivity()
                                                                            .getString(
                                                                                    R.string
                                                                                            .touch_to_fill_sheet_subtitle_insecure_submission,
                                                                                    "m.example.org"))
                                                            .with(
                                                                    IMAGE_DRAWABLE_ID,
                                                                    R.drawable
                                                                            .touch_to_fill_header_image)
                                                            .build()),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView subtitle =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_sheet_subtitle);

        assertThat(
                subtitle.getText().toString(), is("You'll sign in to m.example.org (not secure)"));
    }

    @Test
    @MediumTest
    public void testCredentialsChangedByModel() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS).add(buildCredentialItem(ANA, mItemCollectionInfo));
                    mTouchToFillView.setVisible(true);
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(NO_ONE, mItemCollectionInfo),
                                            buildCredentialItem(BOB, mItemCollectionInfo),
                                            buildCredentialItem(NIK, mItemCollectionInfo)));
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        assertThat(getCredentials().getChildCount(), is(4));
        assertThat(getCredentialOriginAt(0).getVisibility(), is(View.GONE));
        assertThat(getCredentialNameAt(0).getText(), is(ANA.getFormattedUsername()));
        assertThat(getCredentialPasswordOrContextAt(0).getText(), is(ANA.getPassword()));
        assertThat(
                getCredentialPasswordOrContextAt(0).getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
        assertThat(getCredentialOriginAt(1).getVisibility(), is(View.VISIBLE));
        assertThat(getCredentialOriginAt(1).getText(), is("m.example.xyz"));
        assertThat(getCredentialNameAt(1).getText(), is(NO_ONE.getFormattedUsername()));
        assertThat(getCredentialPasswordOrContextAt(1).getText(), is(NO_ONE.getPassword()));
        assertThat(
                getCredentialPasswordOrContextAt(1).getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
        assertThat(getCredentialOriginAt(2).getVisibility(), is(View.VISIBLE));
        assertThat(getCredentialOriginAt(2).getText(), is("mobile.example.xyz"));
        assertThat(getCredentialNameAt(2).getText(), is(BOB.getFormattedUsername()));
        assertThat(getCredentialPasswordOrContextAt(2).getText(), is(BOB.getPassword()));
        assertThat(
                getCredentialPasswordOrContextAt(2).getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
        assertThat(getCredentialOriginAt(3).getVisibility(), is(View.VISIBLE));
        assertThat(getCredentialOriginAt(3).getText(), is("group.xyz"));
        assertThat(getCredentialNameAt(3).getText(), is(NIK.getFormattedUsername()));
        assertThat(getCredentialPasswordOrContextAt(3).getText(), is(NIK.getPassword()));
        assertThat(
                getCredentialPasswordOrContextAt(3).getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
    }

    @Test
    @MediumTest
    public void testCredentialsAreClickable() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, mItemCollectionInfo),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertNotNull(getCredentials().getChildAt(0));

        TouchCommon.singleClickView(getCredentials().getChildAt(0));

        waitForEvent(mCredentialCallback).onResult(eq(ANA));
    }

    @Test
    @MediumTest
    public void testSingleCredentialHasClickableButton() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, mItemCollectionInfo),
                                            buildConfirmationButton(ANA, false),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertNotNull(getCredentials().getChildAt(0));
        assertNotNull(getCredentials().getChildAt(1));

        TouchCommon.singleClickView(getCredentials().getChildAt(1));

        waitForEvent(mCredentialCallback).onResult(eq(ANA));
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
    public void testCredentialClicksThroughObscuringSurfacesAreProcessed() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, mItemCollectionInfo),
                                            buildConfirmationButton(ANA, false),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertNotNull(getCredentials().getChildAt(0));
        assertNotNull(getCredentials().getChildAt(1));

        onViewWaiting(withId(R.id.username))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
        waitForEvent(mCredentialCallback).onResult(eq(ANA));
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
    public void testButtonClicksThroughObscuringSurfacesAreProcessed() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, mItemCollectionInfo),
                                            buildConfirmationButton(ANA, false),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertNotNull(getCredentials().getChildAt(0));
        assertNotNull(getCredentials().getChildAt(1));

        onViewWaiting(withId(R.id.touch_to_fill_button_title))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
        waitForEvent(mCredentialCallback).onResult(eq(ANA));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
    public void testClicksThroughObscuringSurfacesAreIgnoredWhenFeatureIsEnabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, mItemCollectionInfo),
                                            buildConfirmationButton(ANA, false),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertNotNull(getCredentials().getChildAt(0));
        assertNotNull(getCredentials().getChildAt(1));

        onViewWaiting(withId(R.id.username))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
        onViewWaiting(withId(R.id.username))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED));
        onViewWaiting(withId(R.id.touch_to_fill_button_title))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
        onViewWaiting(withId(R.id.touch_to_fill_button_title))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED));
        verify(mCredentialCallback, times(0)).onResult(any());
    }

    @Test
    @MediumTest
    public void testButtonTitleWithoutAutoSubmission() {
        final boolean showSubmitButton = false;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, mItemCollectionInfo),
                                            buildConfirmationButton(ANA, showSubmitButton),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView title =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_button_title);

        assertThat(title.getText(), is(getActivity().getString(R.string.touch_to_fill_continue)));
    }

    @Test
    @MediumTest
    public void testButtonTitle() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, mItemCollectionInfo),
                                            buildConfirmationButton(ANA, true),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView title =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_button_title);

        assertThat(title.getText(), is(getActivity().getString(R.string.touch_to_fill_signin)));
    }

    @Test
    @MediumTest
    public void testManagePasswordsIsClickable() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, mItemCollectionInfo),
                                            buildConfirmationButton(ANA, true),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Swipe the sheet up to it's full state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSheetTestSupport.setSheetState(SheetState.FULL, false));

        TextView manageButton =
                mTouchToFillView
                        .getContentView()
                        .findViewById(R.id.touch_to_fill_sheet_manage_passwords);
        TouchCommon.singleClickView(manageButton);

        pollUiThread(mManageButtonClicked::get);
    }

    @Test
    @MediumTest
    public void testDismissesWhenHidden() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, mItemCollectionInfo),
                                            buildConfirmationButton(ANA, true),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        verify(mDismissHandler).onResult(BottomSheetController.StateChangeReason.NONE);
    }

    @Test
    @MediumTest
    public void testPasswordCredentialAccessibilityDescription() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(
                                                    ANA, new FillableItemCollectionInfo(1, 1)),
                                            buildConfirmationButton(ANA, true),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertNotNull(getCredentials().getChildAt(0));
        String label =
                getActivity()
                        .getString(
                                R.string
                                        .touch_to_fill_password_credential_accessibility_description,
                                ANA.getFormattedUsername());
        assertEquals(
                getCredentials().getChildAt(0).getContentDescription(),
                getActivity()
                        .getString(R.string.touch_to_fill_a11y_item_collection_info, label, 1, 1));
    }

    @Test
    @MediumTest
    public void testPasskeyCredentialAccessibilityDescription() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildWebAuthnCredentialItem(
                                                    CAM, new FillableItemCollectionInfo(1, 1)),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertNotNull(getCredentials().getChildAt(0));
        String label =
                getActivity()
                        .getString(
                                R.string.touch_to_fill_passkey_credential_accessibility_description,
                                CAM.getUsername());
        assertEquals(
                getCredentials().getChildAt(0).getContentDescription(),
                getActivity()
                        .getString(R.string.touch_to_fill_a11y_item_collection_info, label, 1, 1));
    }

    @Test
    @MediumTest
    public void testCredentialAccessibilityDescriptionWithNoCollectionInfo() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, null),
                                            buildWebAuthnCredentialItem(CAM, null),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertNotNull(getCredentials().getChildAt(0));
        assertEquals(
                getCredentials().getChildAt(0).getContentDescription(),
                getActivity()
                        .getString(
                                R.string
                                        .touch_to_fill_password_credential_accessibility_description,
                                ANA.getFormattedUsername()));

        assertNotNull(getCredentials().getChildAt(1));
        assertEquals(
                getCredentials().getChildAt(1).getContentDescription(),
                getActivity()
                        .getString(
                                R.string.touch_to_fill_passkey_credential_accessibility_description,
                                CAM.getUsername()));
    }

    @Test
    @MediumTest
    public void testSheetStartsInFullHeightForAccessibility() {
        // Enabling the accessibility settings.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setIsTouchExplorationEnabledForTesting(true);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, mItemCollectionInfo),
                                            buildConfirmationButton(ANA, true)));
                    mModel.set(VISIBLE, true);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // The sheet should be expanded to full height.
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
    }

    @Test
    @MediumTest
    public void testSheetStartsWithHalfHeight() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, mItemCollectionInfo),
                                            buildConfirmationButton(ANA, true),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // The sheet should be expanded to half height if possible, the half height state is
        // disabled on small screens.
        @BottomSheetController.SheetState
        int desiredState =
                mBottomSheetController.isSmallScreen()
                        ? BottomSheetController.SheetState.FULL
                        : BottomSheetController.SheetState.HALF;
        pollUiThread(() -> getBottomSheetState() == desiredState);
    }

    @Test
    @MediumTest
    public void testSheetScrollabilityDependsOnState() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS).add(buildCredentialItem(ANA, mItemCollectionInfo));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // The sheet should be expanded to half height and suppress scrolling.
        RecyclerView recyclerView = mTouchToFillView.getSheetItemListView();
        assertEquals(!mBottomSheetController.isSmallScreen(), recyclerView.isLayoutSuppressed());

        // Expand the sheet to the full height and scrolling .
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mSheetTestSupport.setSheetState(
                                BottomSheetController.SheetState.FULL, false));
        BottomSheetTestSupport.waitForState(
                mBottomSheetController, BottomSheetController.SheetState.FULL);

        assertFalse(recyclerView.isLayoutSuppressed());
    }

    @Test
    @MediumTest
    public void testHybridPropertyShowsHybridButton() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            new MVCListAdapter.ListItem(
                                                    TouchToFillProperties.ItemType.HEADER,
                                                    new PropertyModel.Builder(
                                                                    HeaderProperties.ALL_KEYS)
                                                            .with(
                                                                    TITLE,
                                                                    getActivity()
                                                                            .getString(
                                                                                    R.string
                                                                                            .touch_to_fill_sheet_uniform_title))
                                                            .with(SUBTITLE, "www.example.org")
                                                            .with(
                                                                    IMAGE_DRAWABLE_ID,
                                                                    R.drawable
                                                                            .touch_to_fill_header_image)
                                                            .build()),
                                            buildFooterItem(true)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView hybridButtonText =
                mTouchToFillView
                        .getContentView()
                        .findViewById(R.id.touch_to_fill_sheet_use_passkeys_other_device);

        assertThat(
                hybridButtonText.getText(),
                is(getActivity().getString(R.string.touch_to_fill_use_device_passkey)));
    }

    @Test
    @MediumTest
    public void testHybridButtonIsClickable() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildCredentialItem(ANA, mItemCollectionInfo),
                                            buildConfirmationButton(ANA, true),
                                            buildFooterItem(true)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSheetTestSupport.setSheetState(SheetState.FULL, false));

        TextView hybridButton =
                mTouchToFillView
                        .getContentView()
                        .findViewById(R.id.touch_to_fill_sheet_use_passkeys_other_device);
        TouchCommon.singleClickView(hybridButton);

        pollUiThread(mHybridButtonClicked::get);
    }

    @Test
    @MediumTest
    public void testMorePasskeysButtonIsClickable() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildMorePasskeysItem(),
                                            buildFooterItem(/* showHybrid= */ false)));
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSheetTestSupport.setSheetState(SheetState.FULL, false));

        TextView morePasskeysItem =
                mTouchToFillView.getContentView().findViewById(R.id.more_passkeys_label);
        TouchCommon.singleClickView(morePasskeysItem);

        pollUiThread(mMorePasskeysClicked::get);
    }

    @Test
    @MediumTest
    public void testPasskeyCredentialSubheader() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .addAll(
                                    asList(
                                            buildWebAuthnCredentialItem(
                                                    CAM, new FillableItemCollectionInfo(1, 1)),
                                            buildFooterItem(false)));
                    mModel.set(VISIBLE, true);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertNotNull(getCredentials().getChildAt(0));

        assertThat(
                getCredentialPasswordOrContextAt(0).getText(),
                is(
                        getActivity()
                                .getString(
                                        R.string.touch_to_fill_sheet_passkey_credential_context)));

        CredManSupportProvider.setupForTesting(/*override*/ false);
    }

    private ChromeActivity getActivity() {
        return mActivityTestRule.getActivity();
    }

    private @SheetState int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }

    private RecyclerView getCredentials() {
        return mTouchToFillView.getContentView().findViewById(R.id.sheet_item_list);
    }

    private TextView getCredentialNameAt(int index) {
        return getCredentials().getChildAt(index).findViewById(R.id.username);
    }

    private TextView getCredentialPasswordOrContextAt(int index) {
        return getCredentials().getChildAt(index).findViewById(R.id.password_or_context);
    }

    private TextView getCredentialOriginAt(int index) {
        return getCredentials().getChildAt(index).findViewById(R.id.credential_origin);
    }

    private static <T> T waitForEvent(T mock) {
        return verify(
                mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    private MVCListAdapter.ListItem buildCredentialItem(
            Credential credential, FillableItemCollectionInfo collectionInfo) {
        return new MVCListAdapter.ListItem(
                TouchToFillProperties.ItemType.CREDENTIAL,
                new PropertyModel.Builder(TouchToFillProperties.CredentialProperties.ALL_KEYS)
                        .with(CREDENTIAL, credential)
                        .with(ON_CLICK_LISTENER, mCredentialCallback)
                        .with(FORMATTED_ORIGIN, credential.getDisplayName())
                        .with(SHOW_SUBMIT_BUTTON, false)
                        .with(ITEM_COLLECTION_INFO, collectionInfo)
                        .build());
    }

    private MVCListAdapter.ListItem buildWebAuthnCredentialItem(
            WebauthnCredential credential, FillableItemCollectionInfo collectionInfo) {
        return new MVCListAdapter.ListItem(
                TouchToFillProperties.ItemType.WEBAUTHN_CREDENTIAL,
                new PropertyModel.Builder(
                                TouchToFillProperties.WebAuthnCredentialProperties.ALL_KEYS)
                        .with(WEBAUTHN_CREDENTIAL, credential)
                        .with(WEBAUTHN_ITEM_COLLECTION_INFO, collectionInfo)
                        .build());
    }

    private MVCListAdapter.ListItem buildConfirmationButton(
            Credential credential, boolean showSubmitButton) {
        return new MVCListAdapter.ListItem(
                TouchToFillProperties.ItemType.FILL_BUTTON,
                new PropertyModel.Builder(TouchToFillProperties.CredentialProperties.ALL_KEYS)
                        .with(CREDENTIAL, credential)
                        .with(ON_CLICK_LISTENER, mCredentialCallback)
                        .with(FORMATTED_ORIGIN, credential.getDisplayName())
                        .with(SHOW_SUBMIT_BUTTON, showSubmitButton)
                        .build());
    }

    private MVCListAdapter.ListItem buildMorePasskeysItem() {
        return new MVCListAdapter.ListItem(
                TouchToFillProperties.ItemType.MORE_PASSKEYS,
                new PropertyModel.Builder(TouchToFillProperties.MorePasskeysProperties.ALL_KEYS)
                        .with(
                                TouchToFillProperties.MorePasskeysProperties.ON_CLICK,
                                () -> mMorePasskeysClicked.set(true))
                        .with(TouchToFillProperties.MorePasskeysProperties.TITLE, "More Passkeys")
                        .build());
    }

    private MVCListAdapter.ListItem buildFooterItem(boolean showHybrid) {
        return new MVCListAdapter.ListItem(
                TouchToFillProperties.ItemType.FOOTER,
                new PropertyModel.Builder(FooterProperties.ALL_KEYS)
                        .with(
                                MANAGE_BUTTON_TEXT,
                                mActivityTestRule
                                        .getActivity()
                                        .getString(R.string.manage_passwords_and_passkeys))
                        .with(ON_CLICK_MANAGE, () -> mManageButtonClicked.set(true))
                        .with(SHOW_HYBRID, showHybrid)
                        .with(ON_CLICK_HYBRID, () -> mHybridButtonClicked.set(true))
                        .build());
    }
}

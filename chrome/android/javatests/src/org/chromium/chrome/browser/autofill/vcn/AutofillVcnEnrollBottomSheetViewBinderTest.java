// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.isEmptyString;
import static org.hamcrest.Matchers.notNullValue;
import static org.junit.Assert.assertEquals;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.Description;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.IssuerIcon;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.LegalMessages;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.LinkOpener;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.autofill.VirtualCardEnrollmentLinkType;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.autofill.payments.LegalMessageLine.Link;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.widget.LoadingView;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Comparator;
import java.util.LinkedList;
import java.util.Optional;
import java.util.TreeMap;
import java.util.concurrent.TimeoutException;

/** Tests for {@link AutofillVcnEnrollBottomSheetViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures(AutofillFeatures.AUTOFILL_ENABLE_VIRTUAL_CARD_JAVA_PAYMENTS_DATA_MANAGER)
public final class AutofillVcnEnrollBottomSheetViewBinderTest extends BlankUiTestActivityTestCase
        implements LinkOpener {
    private PropertyModel.Builder mModelBuilder;
    private PropertyModel mModel;
    private AutofillVcnEnrollBottomSheetView mView;
    private final FakeIconFetcher mFakeIconFetcher = new FakeIconFetcher();

    private static class LoadingViewObserver implements LoadingView.Observer {
        private final CallbackHelper mOnShowHelper = new CallbackHelper();

        private final CallbackHelper mOnHideHelper = new CallbackHelper();

        @Override
        public void onShowLoadingUiComplete() {
            mOnShowHelper.notifyCalled();
        }

        @Override
        public void onHideLoadingUiComplete() {
            mOnHideHelper.notifyCalled();
        }

        public CallbackHelper getOnShowLoadingUiCompleteHelper() {
            return mOnShowHelper;
        }

        public CallbackHelper getOnHideLoadingUiCompleteHelper() {
            return mOnHideHelper;
        }
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        mModelBuilder = new PropertyModel.Builder(AutofillVcnEnrollBottomSheetProperties.ALL_KEYS);
        mView = new AutofillVcnEnrollBottomSheetView(getActivity());
        ThreadUtils.runOnUiThreadBlocking(() -> getActivity().setContentView(mView.mContentView));
        bind(mModelBuilder);
    }

    // Builds the model from the given builder and binds it to the view.
    private void bind(PropertyModel.Builder modelBuilder) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel =
                            modelBuilder
                                    .with(
                                            AutofillVcnEnrollBottomSheetProperties
                                                    .ISSUER_ICON_FETCH_CALLBACK,
                                            mFakeIconFetcher::fetchIcon)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            mModel, mView, AutofillVcnEnrollBottomSheetViewBinder::bind);
                });
    }

    // LinkOpener:
    @Override
    public void openLink(String url, @VirtualCardEnrollmentLinkType int linkType) {}

    @Test
    @SmallTest
    public void testMessageTextInDialogTitle() {
        assertThat(String.valueOf(mView.mDialogTitle.getText()), isEmptyString());

        bind(mModelBuilder.with(AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT, null));
        assertThat(String.valueOf(mView.mDialogTitle.getText()), isEmptyString());

        bind(
                mModelBuilder.with(
                        AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT, "Message text"));
        assertThat(String.valueOf(mView.mDialogTitle.getText()), equalTo("Message text"));
    }

    @Test
    @SmallTest
    public void testDescriptionText() {
        assertThat(String.valueOf(mView.mVirtualCardDescription.getText()), isEmptyString());

        bind(mModelBuilder.with(AutofillVcnEnrollBottomSheetProperties.DESCRIPTION, null));
        assertThat(String.valueOf(mView.mVirtualCardDescription.getText()), isEmptyString());

        bind(
                mModelBuilder.with(
                        AutofillVcnEnrollBottomSheetProperties.DESCRIPTION,
                        new Description(
                                null,
                                null,
                                null,
                                VirtualCardEnrollmentLinkType
                                        .VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
                                /* linkOpener= */ null)));
        assertThat(String.valueOf(mView.mVirtualCardDescription.getText()), isEmptyString());

        bind(
                mModelBuilder.with(
                        AutofillVcnEnrollBottomSheetProperties.DESCRIPTION,
                        new Description(
                                "",
                                "",
                                "",
                                VirtualCardEnrollmentLinkType
                                        .VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
                                /* linkOpener= */ this)));
        assertThat(String.valueOf(mView.mVirtualCardDescription.getText()), isEmptyString());

        bind(
                mModelBuilder.with(
                        AutofillVcnEnrollBottomSheetProperties.DESCRIPTION,
                        new Description(
                                "Description text",
                                "",
                                "",
                                VirtualCardEnrollmentLinkType
                                        .VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
                                /* linkOpener= */ this)));
        assertThat(String.valueOf(mView.mVirtualCardDescription.getText()), isEmptyString());

        bind(
                mModelBuilder.with(
                        AutofillVcnEnrollBottomSheetProperties.DESCRIPTION,
                        new Description(
                                "Description text",
                                "text",
                                "",
                                VirtualCardEnrollmentLinkType
                                        .VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
                                /* linkOpener= */ this)));
        assertThat(String.valueOf(mView.mVirtualCardDescription.getText()), isEmptyString());

        bind(
                mModelBuilder.with(
                        AutofillVcnEnrollBottomSheetProperties.DESCRIPTION,
                        new Description(
                                "Description text",
                                "text",
                                "https://example.test",
                                VirtualCardEnrollmentLinkType
                                        .VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
                                /* linkOpener= */ this)));
        assertThat(
                String.valueOf(mView.mVirtualCardDescription.getText()),
                equalTo("Description text"));
    }

    @Test
    @SmallTest
    public void testCardContainerAccessibilityDescription() {
        ReadableObjectPropertyKey<String> descriptionProperty =
                AutofillVcnEnrollBottomSheetProperties.CARD_CONTAINER_ACCESSIBILITY_DESCRIPTION;

        bind(mModelBuilder.with(descriptionProperty, ""));
        assertThat(String.valueOf(mView.mCardContainer.getContentDescription()), isEmptyString());

        bind(mModelBuilder.with(descriptionProperty, "Content description"));
        assertThat(
                String.valueOf(mView.mCardContainer.getContentDescription()),
                equalTo("Content description"));
    }

    @Test
    @SmallTest
    @DisableFeatures(AutofillFeatures.AUTOFILL_ENABLE_VIRTUAL_CARD_JAVA_PAYMENTS_DATA_MANAGER)
    public void testIssuerIcon() {
        assertThat(mView.mIssuerIcon.getDrawable(), nullValue());

        bind(
                mModelBuilder.with(
                        AutofillVcnEnrollBottomSheetProperties.ISSUER_ICON,
                        new IssuerIcon(
                                createBitmap(/* dimensions= */ 10, /* color= */ 0xFFFF0000),
                                /* width= */ 5,
                                /* height= */ 5)));
        assertThat(mView.mIssuerIcon.getDrawable(), notNullValue());
    }

    @Test
    @SmallTest
    public void testIssuerIconFromCardArtUrl() {
        assertThat(mView.mIssuerIcon.getDrawable(), nullValue());
        int fakeIconResourceId = 1234;
        GURL fakeIconUrl = new GURL(/* uri= */ "https://example.test/card.png");
        Bitmap cardArtBitmapAtUrl = createBitmap(/* dimensions= */ 17, /* color= */ 0xFFFF0000);
        mFakeIconFetcher.putBitmap(
                new IssuerIcon(fakeIconResourceId, fakeIconUrl), cardArtBitmapAtUrl);

        bind(
                mModelBuilder.with(
                        AutofillVcnEnrollBottomSheetProperties.ISSUER_ICON,
                        new IssuerIcon(fakeIconResourceId, fakeIconUrl)));
        assertThat(
                (BitmapDrawable) mView.mIssuerIcon.getDrawable(),
                drawableWithSameBitmap(cardArtBitmapAtUrl));
    }

    Matcher<BitmapDrawable> drawableWithSameBitmap(Bitmap expectedBitmap) {
        return new TypeSafeMatcher<BitmapDrawable>() {
            @Override
            protected boolean matchesSafely(BitmapDrawable drawable) {
                return drawable.getBitmap().sameAs(expectedBitmap);
            }

            @Override
            public void describeTo(org.hamcrest.Description description) {
                description.appendText("a BitmapDrawable with bitmap(");
                description.appendValue(expectedBitmap);
                description.appendText(")");
            }

            @Override
            protected void describeMismatchSafely(
                    BitmapDrawable item, org.hamcrest.Description mismatchDescription) {
                mismatchDescription.appendText("was BitmapDrawable with bitmap(");
                mismatchDescription.appendValue(item.getBitmap());
                mismatchDescription.appendText(")");
            }
        };
    }

    private static class FakeIconFetcher {
        private final TreeMap<IssuerIcon, Bitmap> mIconBitmapLookup =
                new TreeMap<>(
                        Comparator.comparingInt((IssuerIcon issuerIcon) -> issuerIcon.mIconResource)
                                .thenComparing(
                                        (IssuerIcon issuerIcon) ->
                                                Optional.of(issuerIcon.mIconUrl)
                                                        .map(Object::toString)
                                                        .orElse(null)));

        private void putBitmap(IssuerIcon issuerIcon, Bitmap bitmap) {
            mIconBitmapLookup.put(issuerIcon, bitmap);
        }

        /**
         * Implements the icon fetcher function of the view binder.
         *
         * <p>See {@link AutofillVcnEnrollBottomSheetProperties#ISSUER_ICON_FETCH_CALLBACK}
         */
        private Drawable fetchIcon(IssuerIcon icon) {
            if (icon == null) {
                return null;
            }
            // IssuerIcon#mBitmap must not be set when
            // AutofillFeatures.AUTOFILL_ENABLE_VIRTUAL_CARD_JAVA_PAYMENTS_DATA_MANAGER is enabled.
            assert icon.mBitmap == null;
            return new BitmapDrawable(mIconBitmapLookup.get(icon));
        }
    }

    private static Bitmap createBitmap(int dimensions, int color) {
        int[] colors = new int[dimensions * dimensions];
        Arrays.fill(colors, color);
        return Bitmap.createBitmap(colors, dimensions, dimensions, Bitmap.Config.ARGB_8888);
    }

    @Test
    @SmallTest
    public void testCardLabel() {
        runTextViewTest(mView.mCardLabel, AutofillVcnEnrollBottomSheetProperties.CARD_LABEL);
    }

    @Test
    @SmallTest
    public void testCardDescription() {
        runTextViewTest(
                mView.mCardDescription, AutofillVcnEnrollBottomSheetProperties.CARD_DESCRIPTION);
    }

    private void runTextViewTest(TextView view, ReadableObjectPropertyKey<String> property) {
        assertThat(String.valueOf(view.getText()), isEmptyString());

        bind(mModelBuilder.with(property, null));
        assertThat(String.valueOf(view.getText()), isEmptyString());

        bind(mModelBuilder.with(property, "Text view content"));
        assertThat(String.valueOf(view.getText()), equalTo("Text view content"));
    }

    @Test
    @SmallTest
    public void testGoogleLegalMessages() {
        runLegalMessageTest(
                mView.mGoogleLegalMessage,
                AutofillVcnEnrollBottomSheetProperties.GOOGLE_LEGAL_MESSAGES);
    }

    @Test
    @SmallTest
    public void testIssuerLegalMessages() {
        runLegalMessageTest(
                mView.mIssuerLegalMessage,
                AutofillVcnEnrollBottomSheetProperties.ISSUER_LEGAL_MESSAGES);
    }

    private void runLegalMessageTest(
            TextView view, ReadableObjectPropertyKey<LegalMessages> property) {
        assertThat(String.valueOf(view.getText()), isEmptyString());

        bind(mModelBuilder.with(property, null));
        assertThat(String.valueOf(view.getText()), isEmptyString());
        assertThat(view.getVisibility(), equalTo(View.GONE));

        bind(
                mModelBuilder.with(
                        property,
                        new LegalMessages(
                                null,
                                VirtualCardEnrollmentLinkType
                                        .VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK,
                                /* linkOpener= */ null)));
        assertThat(String.valueOf(view.getText()), isEmptyString());
        assertThat(view.getVisibility(), equalTo(View.GONE));

        bind(
                mModelBuilder.with(
                        property,
                        new LegalMessages(
                                new LinkedList<LegalMessageLine>(),
                                VirtualCardEnrollmentLinkType
                                        .VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK,
                                /* linkOpener= */ this)));
        assertThat(String.valueOf(view.getText()), isEmptyString());
        assertThat(view.getVisibility(), equalTo(View.GONE));

        LinkedList<LegalMessageLine> lines = new LinkedList<>();
        lines.add(new LegalMessageLine("Legal message line"));
        bind(
                mModelBuilder.with(
                        property,
                        new LegalMessages(
                                lines,
                                VirtualCardEnrollmentLinkType
                                        .VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK,
                                /* linkOpener= */ this)));
        assertThat(String.valueOf(view.getText()), equalTo("Legal message line"));
        assertThat(view.getVisibility(), equalTo(View.VISIBLE));

        LegalMessageLine line = new LegalMessageLine("Legal message line");
        line.links.add(new Link(0, 5, "https://example.test"));
        lines = new LinkedList<>();
        lines.add(line);
        bind(
                mModelBuilder.with(
                        property,
                        new LegalMessages(
                                lines,
                                VirtualCardEnrollmentLinkType
                                        .VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK,
                                /* linkOpener= */ this)));
        assertThat(String.valueOf(view.getText()), equalTo("Legal message line"));
        assertThat(view.getVisibility(), equalTo(View.VISIBLE));
    }

    @Test
    @SmallTest
    public void testAcceptButtonLabel() {
        runButtonLabelTest(
                mView.mAcceptButton, AutofillVcnEnrollBottomSheetProperties.ACCEPT_BUTTON_LABEL);
    }

    @Test
    @SmallTest
    public void testCancelButtonLabel() {
        runButtonLabelTest(
                mView.mCancelButton, AutofillVcnEnrollBottomSheetProperties.CANCEL_BUTTON_LABEL);
    }

    private void runButtonLabelTest(Button button, ReadableObjectPropertyKey<String> property) {
        assertThat(String.valueOf(button.getText()), isEmptyString());

        bind(mModelBuilder.with(property, null));
        assertThat(String.valueOf(button.getText()), isEmptyString());

        bind(mModelBuilder.with(property, "Button Action Text"));
        assertThat(String.valueOf(button.getText()), equalTo("Button Action Text"));
    }

    @Test
    @SmallTest
    public void testShowLoadingState() throws TimeoutException {
        LoadingViewObserver observer = new LoadingViewObserver();
        mView.mLoadingView.addObserver(observer);

        assertEquals(View.GONE, mView.mLoadingViewContainer.getVisibility());
        assertEquals(View.GONE, mView.mLoadingView.getVisibility());
        assertEquals(View.VISIBLE, mView.mAcceptButton.getVisibility());
        assertEquals(View.VISIBLE, mView.mCancelButton.getVisibility());

        int onShowLoadingUiCompleteCount =
                observer.getOnShowLoadingUiCompleteHelper().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE, true));
        observer.getOnShowLoadingUiCompleteHelper().waitForCallback(onShowLoadingUiCompleteCount);
        assertEquals(View.VISIBLE, mView.mLoadingViewContainer.getVisibility());
        assertEquals(View.VISIBLE, mView.mLoadingView.getVisibility());
        assertEquals(View.GONE, mView.mAcceptButton.getVisibility());
        assertEquals(View.GONE, mView.mCancelButton.getVisibility());

        int onHideLoadingUiCompleteCount =
                observer.getOnHideLoadingUiCompleteHelper().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE, false));
        observer.getOnHideLoadingUiCompleteHelper().waitForCallback(onHideLoadingUiCompleteCount);
        assertEquals(View.GONE, mView.mLoadingViewContainer.getVisibility());
        assertEquals(View.GONE, mView.mLoadingView.getVisibility());
        assertEquals(View.VISIBLE, mView.mAcceptButton.getVisibility());
        assertEquals(View.VISIBLE, mView.mCancelButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testLoadingAccessibilityDescription() {
        ReadableObjectPropertyKey<String> loadingDescription =
                AutofillVcnEnrollBottomSheetProperties.LOADING_DESCRIPTION;

        bind(mModelBuilder.with(loadingDescription, ""));
        assertThat(
                String.valueOf(mView.mLoadingViewContainer.getContentDescription()),
                isEmptyString());

        bind(mModelBuilder.with(loadingDescription, "Loading description"));
        assertThat(
                String.valueOf(mView.mLoadingViewContainer.getContentDescription()),
                equalTo("Loading description"));
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.isEmptyString;
import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.components.autofill.payments.LegalMessage;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.autofill.payments.LegalMessageLine.Link;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.LoadingView;

import java.util.ArrayList;

/** Tests for {@link AutofillSaveCardBottomSheetViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillSaveCardBottomSheetViewBinderUnitTest {
    @DrawableRes private static final int TEST_DRAWABLE_RES = R.drawable.arrow_up;

    private Activity mActivity;
    private PropertyModel.Builder mModelBuilder;
    private PropertyModel mModel;
    private AutofillSaveCardBottomSheetView mView;

    private static class LoadingViewObserver implements LoadingView.Observer {
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

        private final CallbackHelper mOnShowHelper = new CallbackHelper();

        private final CallbackHelper mOnHideHelper = new CallbackHelper();
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mModelBuilder = new PropertyModel.Builder(AutofillSaveCardBottomSheetProperties.ALL_KEYS);
        mView = new AutofillSaveCardBottomSheetView(mActivity);
        mActivity.setContentView(mView.mContentView);
        bind(mModelBuilder);
    }

    @Test
    public void testScrollView() {
        assertEquals(R.id.autofill_save_card_scroll_view, mView.mScrollView.getId());
    }

    @Test
    public void testTitle() {
        assertEquals(R.id.autofill_save_card_title_text, mView.mTitle.getId());
        verifyPropertyBoundToTextView(mView.mTitle, AutofillSaveCardBottomSheetProperties.TITLE);
    }

    @Test
    public void testDescription() {
        assertEquals(R.id.autofill_save_card_description_text, mView.mDescription.getId());
        verifyPropertyBoundToTextView(
                mView.mDescription, AutofillSaveCardBottomSheetProperties.DESCRIPTION);
    }

    @Test
    public void testLogoIcon() {
        assertEquals(R.id.autofill_save_card_icon, mView.mLogoIcon.getId());
        assertThat(mView.mLogoIcon.getDrawable(), nullValue());

        bind(mModelBuilder.with(AutofillSaveCardBottomSheetProperties.LOGO_ICON, 0));
        assertThat(mView.mLogoIcon.getDrawable(), nullValue());
        assertEquals(View.GONE, mView.mLogoIcon.getVisibility());

        bind(
                mModelBuilder.with(
                        AutofillSaveCardBottomSheetProperties.LOGO_ICON, TEST_DRAWABLE_RES));
        assertThat(mView.mLogoIcon.getDrawable(), notNullValue());
        assertEquals(View.VISIBLE, mView.mLogoIcon.getVisibility());
    }

    @Test
    public void testLogoIconDescription() {
        bind(mModelBuilder.with(AutofillSaveCardBottomSheetProperties.LOGO_ICON_DESCRIPTION, ""));
        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO,
                mView.mLogoIcon.getImportantForAccessibility());
        assertThat(String.valueOf(mView.mLogoIcon.getContentDescription()), isEmptyString());

        String descriptionText = "Logo Icon";
        bind(
                mModelBuilder.with(
                        AutofillSaveCardBottomSheetProperties.LOGO_ICON_DESCRIPTION,
                        descriptionText));
        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_YES,
                mView.mLogoIcon.getImportantForAccessibility());
        assertEquals(descriptionText, String.valueOf(mView.mLogoIcon.getContentDescription()));
    }

    @Test
    public void testCard() {
        assertEquals(R.id.autofill_credit_card_chip, mView.mCardView.getId());
    }

    @Test
    public void testCardDescription() {
        bind(mModelBuilder.with(AutofillSaveCardBottomSheetProperties.CARD_DESCRIPTION, ""));
        assertThat(String.valueOf(mView.mCardView.getContentDescription()), isEmptyString());

        final String descriptionText = "Card Description";
        bind(
                mModelBuilder.with(
                        AutofillSaveCardBottomSheetProperties.CARD_DESCRIPTION, descriptionText));
        assertEquals(descriptionText, String.valueOf(mView.mCardView.getContentDescription()));
    }

    @Test
    public void testCardIcon() {
        assertEquals(R.id.autofill_save_card_credit_card_icon, mView.mCardIcon.getId());
        assertThat(mView.mCardIcon.getDrawable(), nullValue());

        bind(mModelBuilder.with(AutofillSaveCardBottomSheetProperties.CARD_ICON, 0));
        assertThat(mView.mCardIcon.getDrawable(), nullValue());

        bind(
                mModelBuilder.with(
                        AutofillSaveCardBottomSheetProperties.CARD_ICON, TEST_DRAWABLE_RES));
        assertThat(mView.mCardIcon.getDrawable(), notNullValue());
    }

    @Test
    public void testCardLabel() {
        assertEquals(R.id.autofill_save_card_credit_card_label, mView.mCardLabel.getId());
        verifyPropertyBoundToTextView(
                mView.mCardLabel, AutofillSaveCardBottomSheetProperties.CARD_LABEL);
    }

    @Test
    public void testCardSubLabel() {
        assertEquals(R.id.autofill_save_card_credit_card_sublabel, mView.mCardSubLabel.getId());
        verifyPropertyBoundToTextView(
                mView.mCardSubLabel, AutofillSaveCardBottomSheetProperties.CARD_SUB_LABEL);
    }

    @Test
    public void testLegalMessage() {
        assertEquals(R.id.legal_message, mView.mLegalMessage.getId());
        assertThat(String.valueOf(mView.mLegalMessage.getText()), isEmptyString());

        bind(
                mModelBuilder.with(
                        AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE,
                        new LegalMessage(ImmutableList.of(), this::openLink)));
        assertThat(String.valueOf(mView.mLegalMessage.getText()), isEmptyString());
        assertEquals(View.GONE, mView.mLegalMessage.getVisibility());

        final String messageText = "Legal message line";
        ArrayList<LegalMessageLine> legalMessageLines = new ArrayList<>();
        legalMessageLines.add(new LegalMessageLine(messageText));
        bind(
                mModelBuilder.with(
                        AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE,
                        new LegalMessage(ImmutableList.copyOf(legalMessageLines), this::openLink)));
        assertEquals(messageText, String.valueOf(mView.mLegalMessage.getText()));
        assertEquals(View.VISIBLE, mView.mLegalMessage.getVisibility());

        LegalMessageLine legalMessageLine = new LegalMessageLine(messageText);
        legalMessageLine.links.add(new Link(0, 5, "https://example.test"));
        legalMessageLines = new ArrayList<>();
        legalMessageLines.add(legalMessageLine);
        bind(
                mModelBuilder.with(
                        AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE,
                        new LegalMessage(ImmutableList.copyOf(legalMessageLines), this::openLink)));
        assertEquals(messageText, String.valueOf(mView.mLegalMessage.getText()));
        assertEquals(View.VISIBLE, mView.mLegalMessage.getVisibility());
    }

    @Test
    public void testAcceptButtonLabel() {
        assertEquals(R.id.autofill_save_card_confirm_button, mView.mAcceptButton.getId());
        verifyPropertyBoundToTextView(
                mView.mAcceptButton, AutofillSaveCardBottomSheetProperties.ACCEPT_BUTTON_LABEL);
    }

    @Test
    public void testCancelButtonLabel() {
        assertEquals(R.id.autofill_save_card_cancel_button, mView.mCancelButton.getId());
        verifyPropertyBoundToTextView(
                mView.mCancelButton, AutofillSaveCardBottomSheetProperties.CANCEL_BUTTON_LABEL);
    }

    @Test
    public void testShowLoadingState() {
        LoadingViewObserver observer = new LoadingViewObserver();
        mView.mLoadingView.addObserver(observer);

        assertEquals(View.GONE, mView.mLoadingViewContainer.getVisibility());
        assertEquals(View.GONE, mView.mLoadingView.getVisibility());
        assertEquals(View.VISIBLE, mView.mAcceptButton.getVisibility());
        assertEquals(View.VISIBLE, mView.mCancelButton.getVisibility());

        mModel.set(AutofillSaveCardBottomSheetProperties.SHOW_LOADING_STATE, true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(1, observer.getOnShowLoadingUiCompleteHelper().getCallCount());
        assertEquals(View.VISIBLE, mView.mLoadingViewContainer.getVisibility());
        assertEquals(View.VISIBLE, mView.mLoadingView.getVisibility());
        assertEquals(View.GONE, mView.mAcceptButton.getVisibility());
        assertEquals(View.GONE, mView.mCancelButton.getVisibility());

        mModel.set(AutofillSaveCardBottomSheetProperties.SHOW_LOADING_STATE, false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(1, observer.getOnHideLoadingUiCompleteHelper().getCallCount());
        assertEquals(View.GONE, mView.mLoadingViewContainer.getVisibility());
        assertEquals(View.GONE, mView.mLoadingView.getVisibility());
        assertEquals(View.VISIBLE, mView.mAcceptButton.getVisibility());
        assertEquals(View.VISIBLE, mView.mCancelButton.getVisibility());
    }

    @Test
    public void testLoadingDescription() {
        bind(mModelBuilder.with(AutofillSaveCardBottomSheetProperties.LOADING_DESCRIPTION, ""));
        assertThat(
                String.valueOf(mView.mLoadingViewContainer.getContentDescription()),
                isEmptyString());

        final String descriptionText = "Loading Description";
        bind(
                mModelBuilder.with(
                        AutofillSaveCardBottomSheetProperties.LOADING_DESCRIPTION,
                        descriptionText));
        assertEquals(
                descriptionText,
                String.valueOf(mView.mLoadingViewContainer.getContentDescription()));
    }

    public void openLink(String url) {}

    private void bind(PropertyModel.Builder modelBuilder) {
        mModel = modelBuilder.build();
        PropertyModelChangeProcessor.create(
                mModel, mView, AutofillSaveCardBottomSheetViewBinder::bind);
    }

    private void verifyPropertyBoundToTextView(
            TextView view, ReadableObjectPropertyKey<String> property) {
        bind(mModelBuilder.with(property, ""));
        assertEquals(View.GONE, view.getVisibility());
        assertThat(String.valueOf(view.getText()), isEmptyString());

        final String messageText = "Test Message";
        bind(mModelBuilder.with(property, messageText));
        assertEquals(View.VISIBLE, view.getVisibility());
        assertEquals(messageText, String.valueOf(view.getText()));
    }
}

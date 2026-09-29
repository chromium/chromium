// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.is;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.ItemType.TEXT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.SHEET_CLOSED_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.SHEET_FULL_HEIGHT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.HeaderProperties.SUBTITLE_BOTTOM_MARGIN;
import static org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.HeaderProperties.SUBTITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.HeaderProperties.TITLE_BOTTOM_MARGIN;
import static org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.HeaderProperties.TITLE_ID;

import android.app.Activity;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.touch_to_fill.R;
import org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillMediator.NoticeContent;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.autofill.PopupNoticeInteractions;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/** Tests for {@link TouchToFillAutofillCoordinator} and {@link TouchToFillAutofillMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
public class TouchToFillAutofillControllerRobolectricTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TouchToFillAutofillComponent.Delegate mDelegateMock;
    @Mock private BottomSheetFocusHelper mBottomSheetFocusHelper;
    @Captor private ArgumentCaptor<BottomSheetContent> mContentCaptor;

    private Activity mActivity;
    private TouchToFillAutofillCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        when(mBottomSheetController.requestShowContent(any(BottomSheetContent.class), anyBoolean()))
                .thenReturn(true);

        mCoordinator =
                new TouchToFillAutofillCoordinator(
                        mActivity, mBottomSheetController, mDelegateMock, mBottomSheetFocusHelper);
    }

    @Test
    public void testShowPersonalContextNotice() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NoticeContent.PERSONAL_CONTEXT.interactionsHistogram,
                        PopupNoticeInteractions.SHOWN);

        mCoordinator.showPersonalContextNotice();

        ModelList sheetItems = mCoordinator.getModelForTesting().get(SHEET_ITEMS);
        assertThat(sheetItems.size(), is(3));
        ListItem header = sheetItems.get(0);
        assertThat(header.type, is(TouchToFillAutofillProperties.ItemType.HEADER));
        assertThat(header.model.get(IMAGE_DRAWABLE_ID), is(R.drawable.fre_product_logo));
        assertThat(header.model.get(TITLE_ID), is(R.string.autofill_personal_context_notice_title));
        assertThat(
                header.model.get(TITLE_BOTTOM_MARGIN),
                is(R.dimen.ttf_notice_header_title_bottom_margin));
        assertThat(
                header.model.get(SUBTITLE_ID),
                is(R.string.autofill_personal_context_notice_description));
        assertThat(
                header.model.get(SUBTITLE_BOTTOM_MARGIN),
                is(R.dimen.ttf_notice_header_subtitle_bottom_margin));

        histogramWatcher.assertExpected();
        verify(mBottomSheetFocusHelper).registerForOneTimeUse();
        verify(mBottomSheetController).requestShowContent(any(BottomSheetContent.class), eq(true));
    }

    @Test
    public void testShowPrivateInferenceNotice() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NoticeContent.PRIVATE_INFERENCE.interactionsHistogram,
                        PopupNoticeInteractions.SHOWN);

        mCoordinator.showPrivateInferenceNotice();

        ModelList sheetItems = mCoordinator.getModelForTesting().get(SHEET_ITEMS);
        assertThat(sheetItems.size(), is(3));
        ListItem header = sheetItems.get(0);
        assertThat(header.type, is(TouchToFillAutofillProperties.ItemType.HEADER));
        assertThat(header.model.get(IMAGE_DRAWABLE_ID), is(R.drawable.fre_product_logo));
        assertThat(
                header.model.get(TITLE_ID),
                is(R.string.autofill_ai_private_inference_notice_sheet_title));
        assertThat(
                header.model.get(SUBTITLE_ID),
                is(R.string.autofill_ai_private_inference_notice_sheet_description));
        assertThat(sheetItems.get(1).type, is(FILL_BUTTON));
        assertThat(sheetItems.get(2).type, is(TEXT_BUTTON));
        assertThat(
                mCoordinator.getModelForTesting().get(SHEET_FULL_HEIGHT_DESCRIPTION_ID),
                is(R.string.autofill_ai_private_inference_notice_sheet_full_height));
        assertThat(
                mCoordinator.getModelForTesting().get(SHEET_CLOSED_DESCRIPTION_ID),
                is(R.string.autofill_ai_private_inference_notice_sheet_closed));

        histogramWatcher.assertExpected();
        verify(mBottomSheetFocusHelper).registerForOneTimeUse();
        verify(mBottomSheetController).requestShowContent(any(BottomSheetContent.class), eq(true));
    }

    @Test
    public void testAcknowledgePrivateInferenceNotice() {
        mCoordinator.showPrivateInferenceNotice();
        verify(mBottomSheetController).requestShowContent(mContentCaptor.capture(), eq(true));

        HistogramWatcher ackWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NoticeContent.PRIVATE_INFERENCE.interactionsHistogram,
                        PopupNoticeInteractions.ACKNOWLEDGED);

        View contentView = mContentCaptor.getValue().getContentView();
        RecyclerView recyclerView = contentView.findViewById(R.id.sheet_item_list);
        recyclerView.measure(0, 0);
        recyclerView.layout(0, 0, 100, 1000);
        contentView.findViewById(R.id.touch_to_fill_button_title).performClick();

        ackWatcher.assertExpected();
        verify(mDelegateMock).onNoticeAcknowledged();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), eq(true));
    }

    @Test
    public void testSettingsLinkPrivateInferenceNotice() {
        mCoordinator.showPrivateInferenceNotice();
        verify(mBottomSheetController).requestShowContent(mContentCaptor.capture(), eq(true));

        HistogramWatcher settingsWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NoticeContent.PRIVATE_INFERENCE.interactionsHistogram,
                        PopupNoticeInteractions.LINK_BUTTON_CLICKED);

        View contentView = mContentCaptor.getValue().getContentView();
        RecyclerView recyclerView = contentView.findViewById(R.id.sheet_item_list);
        recyclerView.measure(0, 0);
        recyclerView.layout(0, 0, 100, 1000);
        contentView.findViewById(R.id.touch_to_fill_text_button).performClick();

        settingsWatcher.assertExpected();
        verify(mDelegateMock).onSettingsLinkClicked();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), eq(true));
    }

    @Test
    public void testAcknowledgeNotice() {
        HistogramWatcher shownWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NoticeContent.PERSONAL_CONTEXT.interactionsHistogram,
                        PopupNoticeInteractions.SHOWN);
        mCoordinator.showPersonalContextNotice();
        shownWatcher.assertExpected();

        verify(mBottomSheetController).requestShowContent(mContentCaptor.capture(), eq(true));

        HistogramWatcher ackWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NoticeContent.PERSONAL_CONTEXT.interactionsHistogram,
                        PopupNoticeInteractions.ACKNOWLEDGED);
        ModelList sheetItems = mCoordinator.getModelForTesting().get(SHEET_ITEMS);
        assertThat(sheetItems.size(), is(3));
        ListItem okItem = sheetItems.get(1);
        assertThat(okItem.type, is(FILL_BUTTON));

        View contentView = mContentCaptor.getValue().getContentView();
        RecyclerView recyclerView = contentView.findViewById(R.id.sheet_item_list);
        recyclerView.measure(0, 0);
        recyclerView.layout(0, 0, 100, 1000);
        contentView.findViewById(R.id.touch_to_fill_button_title).performClick();

        ackWatcher.assertExpected();

        verify(mDelegateMock).onNoticeAcknowledged();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), eq(true));
    }

    @Test
    public void testSettingsLink() {
        HistogramWatcher shownWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NoticeContent.PERSONAL_CONTEXT.interactionsHistogram,
                        PopupNoticeInteractions.SHOWN);
        mCoordinator.showPersonalContextNotice();
        shownWatcher.assertExpected();

        verify(mBottomSheetController).requestShowContent(mContentCaptor.capture(), eq(true));

        HistogramWatcher settingsWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NoticeContent.PERSONAL_CONTEXT.interactionsHistogram,
                        PopupNoticeInteractions.LINK_BUTTON_CLICKED);
        ModelList sheetItems = mCoordinator.getModelForTesting().get(SHEET_ITEMS);
        assertThat(sheetItems.size(), is(3));
        ListItem settingItems = sheetItems.get(2);
        assertThat(settingItems.type, is(TEXT_BUTTON));

        View contentView = mContentCaptor.getValue().getContentView();
        RecyclerView recyclerView = contentView.findViewById(R.id.sheet_item_list);
        recyclerView.measure(0, 0);
        recyclerView.layout(0, 0, 100, 1000);
        contentView.findViewById(R.id.touch_to_fill_text_button).performClick();

        settingsWatcher.assertExpected();

        verify(mDelegateMock).onSettingsLinkClicked();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), eq(true));
    }

    @Test
    public void testHideSheet() {
        HistogramWatcher shownWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NoticeContent.PERSONAL_CONTEXT.interactionsHistogram,
                        PopupNoticeInteractions.SHOWN);
        mCoordinator.showPersonalContextNotice();
        shownWatcher.assertExpected();

        HistogramWatcher dismissedWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NoticeContent.PERSONAL_CONTEXT.interactionsHistogram,
                        PopupNoticeInteractions.DISMISSED);
        mCoordinator.hide();
        dismissedWatcher.assertExpected();

        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), eq(true));
        verify(mDelegateMock).onDismissed();
    }

    @Test
    public void testHideBeforeShowRecordsNoInteraction() {
        HistogramWatcher noInteractionWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(NoticeContent.PERSONAL_CONTEXT.interactionsHistogram)
                        .expectNoRecords(NoticeContent.PRIVATE_INFERENCE.interactionsHistogram)
                        .build();

        mCoordinator.hide();

        noInteractionWatcher.assertExpected();
        verify(mDelegateMock).onDismissed();
    }

    @Test
    public void testGetVerticalScrollOffset() {
        mCoordinator.showPersonalContextNotice();
        verify(mBottomSheetController).requestShowContent(mContentCaptor.capture(), eq(true));
        BottomSheetContent content = mContentCaptor.getValue();
        RecyclerView recyclerView = content.getContentView().findViewById(R.id.sheet_item_list);
        assertThat(
                content.getVerticalScrollOffset(), is(recyclerView.computeVerticalScrollOffset()));
    }
}

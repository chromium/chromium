// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.page_info_sheet.PageInfoBottomSheetCoordinator.PageInfoContents;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
public class PageInfoBottomSheetCoordinatorTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private PageInfoBottomSheetCoordinator.Delegate mPageInfoDelegate;

    @Captor private ArgumentCaptor<PageInfoBottomSheetContent> mSheetContentCaptor;

    private ObservableSupplierImpl<PageInfoContents> mPageInfoContentsSupplier;

    @Before
    public void setUp() {
        mPageInfoContentsSupplier = new ObservableSupplierImpl<>();
        when(mPageInfoDelegate.getContentSupplier()).thenReturn(mPageInfoContentsSupplier);
    }

    @Test
    public void testInitializingState() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            // Initialize coordinator and show UI.
                            PageInfoBottomSheetCoordinator coordinator =
                                    new PageInfoBottomSheetCoordinator(
                                            activity, mPageInfoDelegate, mBottomSheetController);
                            coordinator.requestShowContent();

                            // Ensure bottom sheet was opened.
                            verify(mBottomSheetController)
                                    .requestShowContent(mSheetContentCaptor.capture(), eq(true));

                            // Get UI elements from bottom sheet.
                            View acceptButton = getView(R.id.accept_button);
                            View cancelButton = getView(R.id.cancel_button);
                            View refreshButton = getView(R.id.refresh_button);
                            View loadingIndicator = getView(R.id.loading_indicator);
                            View contentTextView = getView(R.id.sheet_content);

                            // Initially the accept and cancel buttons should be disabled.
                            assertFalse(
                                    "Accept button shouldn't be enabled", acceptButton.isEnabled());
                            assertTrue("Cancel button should be enabled", cancelButton.isEnabled());
                            // Refresh button should not be visible until finished.
                            assertNotEquals(
                                    "Refresh button should be hidden",
                                    View.VISIBLE,
                                    refreshButton.getVisibility());
                            // Loading indicator should be visible initially.
                            assertEquals(
                                    "Loading indicator should be visible",
                                    View.VISIBLE,
                                    loadingIndicator.getVisibility());
                            // No text should be visible initially.
                            assertEquals(
                                    "No text should be shown",
                                    View.GONE,
                                    contentTextView.getVisibility());
                        });
    }

    @Test
    public void testLoadingState() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            PageInfoBottomSheetCoordinator coordinator =
                                    new PageInfoBottomSheetCoordinator(
                                            activity, mPageInfoDelegate, mBottomSheetController);
                            coordinator.requestShowContent();

                            verify(mBottomSheetController)
                                    .requestShowContent(mSheetContentCaptor.capture(), eq(true));

                            View acceptButton = getView(R.id.accept_button);
                            View cancelButton = getView(R.id.cancel_button);
                            View refreshButton = getView(R.id.refresh_button);
                            View loadingIndicator = getView(R.id.loading_indicator);
                            TextView contentTextView = (TextView) getView(R.id.sheet_content);

                            // Update page info while still loading.
                            mPageInfoContentsSupplier.set(
                                    new PageInfoContents(
                                            /* resultContents= */ "Foo", /* isLoading= */ true));

                            // Accept/cancel button should be disabled while loading.
                            assertFalse(
                                    "Accept button should be disabled", acceptButton.isEnabled());
                            assertTrue("Cancel button should be enabled", cancelButton.isEnabled());
                            // Refresh button should not be visible until finished.
                            assertNotEquals(
                                    "Refresh button shouldn't be visible",
                                    View.VISIBLE,
                                    refreshButton.getVisibility());
                            // Loading indicator should be hidden.
                            assertEquals(
                                    "Loading indicator shouldn't be visible",
                                    View.GONE,
                                    loadingIndicator.getVisibility());
                            // Instead we display the text from page info.
                            assertEquals(
                                    "Page info text should be visible",
                                    View.VISIBLE,
                                    contentTextView.getVisibility());
                            assertEquals(
                                    "Text shown should correspond to last page info update",
                                    "Foo",
                                    contentTextView.getText());
                        });
    }

    @Test
    public void testSuccessState() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            PageInfoBottomSheetCoordinator coordinator =
                                    new PageInfoBottomSheetCoordinator(
                                            activity, mPageInfoDelegate, mBottomSheetController);
                            coordinator.requestShowContent();

                            verify(mBottomSheetController)
                                    .requestShowContent(mSheetContentCaptor.capture(), eq(true));

                            View acceptButton = getView(R.id.accept_button);
                            View cancelButton = getView(R.id.cancel_button);
                            View refreshButton = getView(R.id.refresh_button);
                            View loadingIndicator = getView(R.id.loading_indicator);
                            TextView contentTextView = (TextView) getView(R.id.sheet_content);

                            // Update page info twice, the second update indicating we're finished
                            // loading.
                            mPageInfoContentsSupplier.set(
                                    new PageInfoContents(
                                            /* resultContents= */ "Foo", /* isLoading= */ true));
                            mPageInfoContentsSupplier.set(
                                    new PageInfoContents(
                                            /* resultContents= */ "Bar", /* isLoading= */ false));

                            // Accept/Cancel buttons should now be enabled.
                            assertTrue("Accept button should be enabled", acceptButton.isEnabled());
                            assertTrue("Cancel button should be enabled", cancelButton.isEnabled());
                            // Refresh button should now be visible.
                            assertEquals(
                                    "Refresh button should be visible",
                                    View.VISIBLE,
                                    refreshButton.getVisibility());
                            // Loading indicator should be hidden.
                            assertEquals(
                                    "Loading indicator should be hidden",
                                    View.GONE,
                                    loadingIndicator.getVisibility());
                            // Text from page info should be visible.
                            assertEquals(
                                    "Page info text should be visible",
                                    View.VISIBLE,
                                    contentTextView.getVisibility());
                            assertEquals(
                                    "Shown text should correspond to the last update",
                                    "Bar",
                                    contentTextView.getText());
                        });
    }

    @Test
    public void testSuccessState_clickHandlers() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            PageInfoBottomSheetCoordinator coordinator =
                                    new PageInfoBottomSheetCoordinator(
                                            activity, mPageInfoDelegate, mBottomSheetController);
                            coordinator.requestShowContent();

                            verify(mBottomSheetController)
                                    .requestShowContent(mSheetContentCaptor.capture(), eq(true));

                            View acceptButton = getView(R.id.accept_button);
                            View cancelButton = getView(R.id.cancel_button);
                            View refreshButton = getView(R.id.refresh_button);

                            // Set state to success to enable buttons.
                            mPageInfoContentsSupplier.set(
                                    new PageInfoContents(
                                            /* resultContents= */ "Bar", /* isLoading= */ false));

                            // Click accept button once.
                            acceptButton.performClick();
                            // Click cancel button twice.
                            cancelButton.performClick();
                            cancelButton.performClick();
                            // Click refresh button thrice.
                            refreshButton.performClick();
                            refreshButton.performClick();
                            refreshButton.performClick();

                            // Ensure delegate received click events as expected.
                            verify(mPageInfoDelegate, times(1)).onAccept();
                            verify(mPageInfoDelegate, times(2)).onCancel();
                            verify(mPageInfoDelegate, times(3)).onRefresh();
                        });
    }

    @Test
    public void testErrorState() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            PageInfoBottomSheetCoordinator coordinator =
                                    new PageInfoBottomSheetCoordinator(
                                            activity, mPageInfoDelegate, mBottomSheetController);
                            coordinator.requestShowContent();

                            verify(mBottomSheetController)
                                    .requestShowContent(mSheetContentCaptor.capture(), eq(true));

                            View acceptButton = getView(R.id.accept_button);
                            View cancelButton = getView(R.id.cancel_button);
                            View refreshButton = getView(R.id.refresh_button);
                            View loadingIndicator = getView(R.id.loading_indicator);
                            TextView contentTextView = (TextView) getView(R.id.sheet_content);

                            // Set state to error.
                            mPageInfoContentsSupplier.set(
                                    new PageInfoContents(
                                            /* errorMessage= */ "Something went wrong"));

                            // Accept button should be disabled.
                            assertFalse(
                                    "Accept button should be disabled", acceptButton.isEnabled());
                            // Cancel button should be enabled.
                            assertTrue("Cancel button should be enabled", cancelButton.isEnabled());
                            // Refresh button should not be visible.
                            assertNotEquals(
                                    "Refresh button should not be visible",
                                    View.VISIBLE,
                                    refreshButton.getVisibility());
                            // Loading indicator should be hidden.
                            assertEquals(
                                    "Loading indicator shouldn't be visible",
                                    View.GONE,
                                    loadingIndicator.getVisibility());
                            // Error message should be shown.
                            assertEquals(
                                    "Error message should be visible",
                                    View.VISIBLE,
                                    contentTextView.getVisibility());
                            assertEquals(
                                    "Error message text should be shown",
                                    "Something went wrong",
                                    contentTextView.getText());
                        });
    }

    private View getView(int viewId) {
        View view = mSheetContentCaptor.getValue().getContentView();
        assertNotNull(view);

        return view.findViewById(viewId);
    }
}

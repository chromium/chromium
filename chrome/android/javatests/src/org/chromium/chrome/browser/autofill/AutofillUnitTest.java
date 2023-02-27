// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillPopup;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests the Autofill's java code for creating the AutofillPopup object, opening and selecting
 * popups.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AutofillUnitTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private AutofillPopup mAutofillPopup;
    private MockAutofillCallback mMockAutofillCallback;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        mMockAutofillCallback = new MockAutofillCallback();
        final ViewAndroidDelegate viewDelegate = ViewAndroidDelegate.createBasicDelegate(
                sActivityTestRule.getActivity().findViewById(android.R.id.content));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            View anchorView = viewDelegate.acquireView();
            viewDelegate.setViewPosition(anchorView, 50f, 500f, 500f, 500f, 10, 10);

            mAutofillPopup = new AutofillPopup(
                    sActivityTestRule.getActivity(), anchorView, mMockAutofillCallback, null);
            mAutofillPopup.filterAndShow(new AutofillSuggestion[0], /* isRtl= */ false);
        });
    }

    private static final long CALLBACK_TIMEOUT_MS = 4000L;
    private static final int CHECK_INTERVAL_MS = 100;

    private class MockAutofillCallback implements AutofillDelegate {
        private final AtomicBoolean mGotPopupSelection = new AtomicBoolean(false);
        public int mListIndex = -1;

        @Override
        public void suggestionSelected(int listIndex) {
            mListIndex = listIndex;
            mAutofillPopup.dismiss();
            mGotPopupSelection.set(true);
        }

        @Override
        public void deleteSuggestion(int listIndex) {}

        public void waitForCallback() {
            CriteriaHelper.pollInstrumentationThread(
                    mGotPopupSelection::get, CALLBACK_TIMEOUT_MS, CHECK_INTERVAL_MS);
        }

        @Override
        public void dismissed() {}

        @Override
        public void accessibilityFocusCleared() {}
    }

    private AutofillSuggestion[] createTwoAutofillSuggestionArray() {
        return new AutofillSuggestion[] {
                new AutofillSuggestion("Sherlock Holmes", "221B Baker Street", /*itemTag=*/"",
                        DropdownItem.NO_ICON, false, 42, false, false, false,
                        /* featureForIPH= */ ""),
                new AutofillSuggestion("Arthur Dent", "West Country", /*itemTag=*/"",
                        DropdownItem.NO_ICON, false, 43, false, false, false,
                        /* featureForIPH= */ ""),
        };
    }

    private AutofillSuggestion[] createFiveAutofillSuggestionArray() {
        return new AutofillSuggestion[] {
                new AutofillSuggestion("Sherlock Holmes", "221B Baker Street", /*itemTag=*/"",
                        DropdownItem.NO_ICON, false, 42, false, false, false,
                        /* featureForIPH= */ ""),
                new AutofillSuggestion("Arthur Dent", "West Country", /*itemTag=*/"",
                        DropdownItem.NO_ICON, false, 43, false, false, false,
                        /* featureForIPH= */ ""),
                new AutofillSuggestion("Arthos", "France", /*itemTag=*/"", DropdownItem.NO_ICON,
                        false, 44, false, false, false, /* featureForIPH= */ ""),
                new AutofillSuggestion("Porthos", "France", /*itemTag=*/"", DropdownItem.NO_ICON,
                        false, 45, false, false, false, /* featureForIPH= */ ""),
                new AutofillSuggestion("Aramis", "France", /*itemTag=*/"", DropdownItem.NO_ICON,
                        false, 46, false, false, false, /* featureForIPH= */ ""),
        };
    }

    public void openAutofillPopupAndWaitUntilReady(final AutofillSuggestion[] suggestions) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAutofillPopup.filterAndShow(suggestions, /* isRtl= */ false));
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    mAutofillPopup.getListView().getChildCount(), Matchers.greaterThan(0));
        });
    }

    @Test
    @SmallTest
    @Feature({"autofill"})
    public void testAutofillWithDifferentNumberSuggestions() {
        openAutofillPopupAndWaitUntilReady(createTwoAutofillSuggestionArray());
        Assert.assertEquals(2, mAutofillPopup.getListView().getCount());

        openAutofillPopupAndWaitUntilReady(createFiveAutofillSuggestionArray());
        Assert.assertEquals(5, mAutofillPopup.getListView().getCount());
    }

    @Test
    @SmallTest
    @Feature({"autofill"})
    @DisabledTest(message = "https://crbug.com/1338184")
    public void testAutofillClickFirstSuggestion() {
        AutofillSuggestion[] suggestions = createTwoAutofillSuggestionArray();
        openAutofillPopupAndWaitUntilReady(suggestions);
        Assert.assertEquals(2, mAutofillPopup.getListView().getCount());

        TouchCommon.singleClickView(mAutofillPopup.getListView().getChildAt(0));
        mMockAutofillCallback.waitForCallback();

        Assert.assertEquals(0, mMockAutofillCallback.mListIndex);
    }
}

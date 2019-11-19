// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.support.test.filters.SmallTest;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillPopup;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests the Autofill's java code for creating the AutofillPopup object, opening and selecting
 * popups.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private AutofillPopup mAutofillPopup;
    private WindowAndroid mWindowAndroid;
    private MockAutofillCallback mMockAutofillCallback;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        mMockAutofillCallback = new MockAutofillCallback();
        final ChromeActivity activity = mActivityTestRule.getActivity();
        final ViewAndroidDelegate viewDelegate =
                ViewAndroidDelegate.createBasicDelegate(activity.getActivityTab().getContentView());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            View anchorView = viewDelegate.acquireView();
            viewDelegate.setViewPosition(anchorView, 50f, 500f, 500f, 500f, 10, 10);

            mWindowAndroid = new ActivityWindowAndroid(activity);
            mAutofillPopup = new AutofillPopup(activity, anchorView, mMockAutofillCallback);
            mAutofillPopup.filterAndShow(
                    new AutofillSuggestion[0], /* isRtl= */ false, /* isRefresh= */ false);
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
        public void deleteSuggestion(int listIndex) {
        }

        public void waitForCallback() {
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mGotPopupSelection.get();
                }
            }, CALLBACK_TIMEOUT_MS, CHECK_INTERVAL_MS);
        }

        @Override
        public void dismissed() {
        }

        @Override
        public void accessibilityFocusCleared() {
        }
    }

    private AutofillSuggestion[] createTwoAutofillSuggestionArray() {
        return new AutofillSuggestion[] {
                new AutofillSuggestion("Sherlock Holmes", "221B Baker Street", DropdownItem.NO_ICON,
                        false, 42, false, false, false),
                new AutofillSuggestion("Arthur Dent", "West Country", DropdownItem.NO_ICON,
                        false, 43, false, false, false),
        };
    }

    private AutofillSuggestion[] createFiveAutofillSuggestionArray() {
        return new AutofillSuggestion[] {
                new AutofillSuggestion("Sherlock Holmes", "221B Baker Street", DropdownItem.NO_ICON,
                        false, 42, false, false, false),
                new AutofillSuggestion("Arthur Dent", "West Country", DropdownItem.NO_ICON,
                        false, 43, false, false, false),
                new AutofillSuggestion("Arthos", "France", DropdownItem.NO_ICON,
                        false, 44, false, false, false),
                new AutofillSuggestion("Porthos", "France", DropdownItem.NO_ICON,
                        false, 45, false, false, false),
                new AutofillSuggestion("Aramis", "France", DropdownItem.NO_ICON,
                        false, 46, false, false, false),
        };
    }

    public void openAutofillPopupAndWaitUntilReady(final AutofillSuggestion[] suggestions) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mAutofillPopup.filterAndShow(
                                suggestions, /* isRtl= */ false, /* isRefresh= */ false));
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mAutofillPopup.getListView().getChildCount() > 0;
            }
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
    public void testAutofillClickFirstSuggestion() {
        AutofillSuggestion[] suggestions = createTwoAutofillSuggestionArray();
        openAutofillPopupAndWaitUntilReady(suggestions);
        Assert.assertEquals(2, mAutofillPopup.getListView().getCount());

        TouchCommon.singleClickView(mAutofillPopup.getListView().getChildAt(0));
        mMockAutofillCallback.waitForCallback();

        Assert.assertEquals(0, mMockAutofillCallback.mListIndex);
    }
}

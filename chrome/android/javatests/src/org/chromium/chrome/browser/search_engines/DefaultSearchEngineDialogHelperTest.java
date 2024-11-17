// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import android.content.Context;
import android.text.TextUtils;
import android.widget.Button;
import android.widget.RadioButton;

import androidx.test.annotation.UiThreadTest;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.RadioButtonLayout;
import org.chromium.components.search_engines.TemplateUrl;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for the class that backs the "Choose your search engine" dialogs.
 *
 * <p>TODO(dfalcantara): Add the MultiActivityTestRule() as a @Rule when it lands.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class DefaultSearchEngineDialogHelperTest {
    private class TestDelegate implements DefaultSearchEngineDialogHelper.Delegate {
        public String chosenKeyword;

        @Override
        public List<TemplateUrl> getSearchEnginesForPromoDialog(@SearchEnginePromoType int type) {
            return mTemplateUrls;
        }

        @Override
        public void onUserSearchEngineChoice(
                @SearchEnginePromoType int type, List<String> keywords, String keyword) {
            chosenKeyword = keyword;
        }
    }

    private static class TestTemplateUrl extends TemplateUrl {
        private String mShortName;
        private String mKeyword;

        public TestTemplateUrl(String shortName, String keyword) {
            super(0);
            mShortName = shortName;
            mKeyword = keyword;
        }

        @Override
        public String getShortName() {
            return mShortName;
        }

        @Override
        public String getKeyword() {
            return mKeyword;
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof TestTemplateUrl)) return false;
            TestTemplateUrl otherTemplateUrl = (TestTemplateUrl) other;
            return TextUtils.equals(mKeyword, otherTemplateUrl.mKeyword)
                    && TextUtils.equals(mShortName, otherTemplateUrl.mShortName);
        }
    }

    private static class TestDialogHelper extends DefaultSearchEngineDialogHelper {
        public final TestDelegate delegate;

        public TestDialogHelper(
                @SearchEnginePromoType int dialogType,
                TestDelegate delegate,
                RadioButtonLayout controls,
                Button confirmButton,
                Runnable finishRunnable) {
            super(dialogType, delegate, controls, confirmButton, finishRunnable);
            this.delegate = delegate;
        }
    }

    private static class DismissRunnable implements Runnable {
        public int runCount;

        @Override
        public void run() {
            runCount++;
        }
    }

    private final DismissRunnable mDismissRunnable = new DismissRunnable();
    private final List<TemplateUrl> mTemplateUrls = new ArrayList<>();
    private final TestDelegate mTestDelegate = new TestDelegate();

    private Context mContext;
    private @SearchEnginePromoType int mDialogType;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();

        mTemplateUrls.clear();
        mTemplateUrls.add(new TestTemplateUrl("Google: Search by Google", "keyword 1"));
        mTemplateUrls.add(new TestTemplateUrl("This", "keyword 2"));
        mTemplateUrls.add(new TestTemplateUrl("That", "keyword 3"));
        mTemplateUrls.add(new TestTemplateUrl("The Other Thing", "keyword 4"));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testInitialState() {
        mDialogType = SearchEnginePromoType.SHOW_EXISTING;

        RadioButtonLayout radioLayout = new RadioButtonLayout(mContext);
        Button okButton = new Button(mContext);
        TestDialogHelper helper =
                new TestDialogHelper(
                        mDialogType, mTestDelegate, radioLayout, okButton, mDismissRunnable);

        // Confirm that no radio buttons are marked as selected.
        for (int i = 0; i < radioLayout.getChildCount(); i++) {
            RadioButton radioButton = (RadioButton) radioLayout.getChildAt(i);
            Assert.assertFalse("Engine was pre-selected in dialog", radioButton.isChecked());
        }

        // Confirm that all engines appear in the dialog.
        Assert.assertEquals(mTemplateUrls.size(), radioLayout.getChildCount());
        List<TemplateUrl> foundTemplateUrls = new ArrayList<>();
        for (int i = 0; i < radioLayout.getChildCount(); i++) {
            RadioButton radioButton = (RadioButton) radioLayout.getChildAt(i);
            CharSequence engineName = radioButton.getText();
            String keyword = (String) radioButton.getTag();

            // Confirm that the engine is in the original list.
            boolean wasFound = false;
            for (int j = 0; j < mTemplateUrls.size(); j++) {
                TemplateUrl templateUrl = mTemplateUrls.get(j);
                if (TextUtils.equals(templateUrl.getShortName(), engineName)) {
                    wasFound = true;
                    Assert.assertEquals(templateUrl.getKeyword(), keyword);

                    Assert.assertFalse(foundTemplateUrls.contains(templateUrl));
                    foundTemplateUrls.add(templateUrl);
                }
            }
            Assert.assertTrue("Engine was missing: " + engineName, wasFound);
        }
        Assert.assertEquals("Engines were missing", mTemplateUrls.size(), foundTemplateUrls.size());

        Assert.assertNull("Engine was already selected", helper.getCurrentlySelectedKeyword());
        Assert.assertNull("Engine was set before user acted", helper.delegate.chosenKeyword);
        Assert.assertFalse("User could exit the dialog before selecting", okButton.isEnabled());
        Assert.assertEquals("Dialog was already dismissed", 0, mDismissRunnable.runCount);

        // User shouldn't be able to click the button successfully.
        okButton.performClick();
        Assert.assertEquals(
                "Dialog dismissed despite button being disabled", 0, mDismissRunnable.runCount);
    }

    /**
     * This test is testing the randomness of the search engine listing (which absolutely must be
     * shuffled). Although it is inherently flaky, reduce the chance of it failing randomly by
     * running the test a few times internally before declaring failure to the testing framework.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testListRandomization() {
        final int maxAttempts = 3;
        boolean succeeded = false;

        mDialogType = SearchEnginePromoType.SHOW_EXISTING;

        // Repeatedly create pairs of helpers and confirm that they are shuffled differently.  If
        // this test repeatedly iterates without succeeding, then something is terribly wrong.
        for (int i = 0; i < maxAttempts && !succeeded; i++) {
            RadioButtonLayout firstLayout = new RadioButtonLayout(mContext);
            Button firstButton = new Button(mContext);
            DismissRunnable firstRunnable = new DismissRunnable();
            new TestDialogHelper(
                    mDialogType, mTestDelegate, firstLayout, firstButton, firstRunnable);
            Assert.assertEquals(mTemplateUrls.size(), firstLayout.getChildCount());

            RadioButtonLayout secondLayout = new RadioButtonLayout(mContext);
            Button secondButton = new Button(mContext);
            DismissRunnable secondRunnable = new DismissRunnable();
            new TestDialogHelper(
                    mDialogType, mTestDelegate, secondLayout, secondButton, secondRunnable);
            Assert.assertEquals(mTemplateUrls.size(), firstLayout.getChildCount());

            boolean listsMatched = true;
            for (int j = 0; j < firstLayout.getChildCount() && listsMatched; j++) {
                RadioButton firstRadioButton = (RadioButton) firstLayout.getChildAt(j);
                RadioButton secondRadioButton = (RadioButton) secondLayout.getChildAt(j);
                if (!TextUtils.equals(firstRadioButton.getText(), secondRadioButton.getText())) {
                    listsMatched = false;
                }
            }

            if (!listsMatched) succeeded = true;
        }

        Assert.assertTrue("Lists were not randomized differently at least once", succeeded);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSelectEngine() {
        mDialogType = SearchEnginePromoType.SHOW_EXISTING;

        RadioButtonLayout radioLayout = new RadioButtonLayout(mContext);
        Button okButton = new Button(mContext);
        TestDialogHelper helper =
                new TestDialogHelper(
                        mDialogType, mTestDelegate, radioLayout, okButton, mDismissRunnable);

        Assert.assertNull(
                "Engine selected after construction", helper.getCurrentlySelectedKeyword());
        Assert.assertFalse("Button enabled before selection", okButton.isEnabled());
        Assert.assertEquals("Dialog was already dismissed", 0, mDismissRunnable.runCount);

        Assert.assertNull("Engine was already selected", helper.getCurrentlySelectedKeyword());
        // Select and confirm the first engine in the list.
        int index = 0;
        selectEngineFromList(helper, radioLayout, okButton, index);
        Assert.assertNotNull("Engine was already selected", helper.getCurrentlySelectedKeyword());
        Assert.assertNull("Engine was already selected", helper.getConfirmedKeyword());
        okButton.performClick();
        confirmEngineSelection(helper, radioLayout, index);
        Assert.assertNotNull("Engine was already selected", helper.getConfirmedKeyword());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testFlipFlopSelection() {
        mDialogType = SearchEnginePromoType.SHOW_EXISTING;

        RadioButtonLayout radioLayout = new RadioButtonLayout(mContext);
        Button okButton = new Button(mContext);
        TestDialogHelper helper =
                new TestDialogHelper(
                        mDialogType, mTestDelegate, radioLayout, okButton, mDismissRunnable);

        Assert.assertNull(
                "Engine selected after construction", helper.getCurrentlySelectedKeyword());
        Assert.assertFalse("Button enabled before selection", okButton.isEnabled());
        Assert.assertEquals("Dialog was already dismissed", 0, mDismissRunnable.runCount);

        // Select the first engine in the list.
        selectEngineFromList(helper, radioLayout, okButton, 0);

        // Whoops.  Meant to select the third in the list.
        selectEngineFromList(helper, radioLayout, okButton, 2);

        // Clicking OK should lock in the third item in the list, not the first.
        okButton.performClick();
        confirmEngineSelection(helper, radioLayout, 2);
    }

    private void selectEngineFromList(
            TestDialogHelper helper, RadioButtonLayout radioLayout, Button okButton, int index) {
        RadioButton radioButton = (RadioButton) radioLayout.getChildAt(index);
        String keyword = (String) radioButton.getTag();

        radioLayout.getChildAt(index).performClick();
        Assert.assertNotNull("Didn't update after selecting", helper.getCurrentlySelectedKeyword());
        Assert.assertTrue(
                "Engine keywords didn't match",
                TextUtils.equals(keyword, helper.getCurrentlySelectedKeyword()));
        Assert.assertNull("Engine set before explicitly hitting OK", helper.delegate.chosenKeyword);
        Assert.assertTrue("User can't proceed because button disabled", okButton.isEnabled());
        Assert.assertEquals("Dialog was already dismissed", 0, mDismissRunnable.runCount);
    }

    private void confirmEngineSelection(
            TestDialogHelper helper, RadioButtonLayout radioLayout, int index) {
        RadioButton radioButton = (RadioButton) radioLayout.getChildAt(index);
        String keyword = (String) radioButton.getTag();

        Assert.assertNotNull("Engine wasn't set after hitting OK", helper.delegate.chosenKeyword);
        Assert.assertTrue(
                "Engine keywords didn't match",
                TextUtils.equals(keyword, helper.delegate.chosenKeyword));
        Assert.assertEquals("Runnable failed to after hitting OK", 1, mDismissRunnable.runCount);
    }
}

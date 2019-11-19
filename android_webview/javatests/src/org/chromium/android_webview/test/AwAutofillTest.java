// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.os.LocaleList;
import android.os.Parcel;
import android.support.test.filters.SmallTest;
import android.util.Pair;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.ViewStructure.HtmlInfo.Builder;
import android.view.WindowManager;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillValue;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwAutofillManager;
import org.chromium.android_webview.AwAutofillProvider;
import org.chromium.android_webview.AwAutofillUMA;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.autofill.mojom.SubmissionSource;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;

import java.io.ByteArrayInputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.TimeoutException;

/**
 * Tests for WebView Autofill.
 */
@RunWith(AwJUnit4ClassRunner.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.O)
@SuppressLint("NewApi")
public class AwAutofillTest {
    public static final boolean DEBUG = false;
    public static final String TAG = "AutofillTest";

    public static final String FILE = "/login.html";
    public static final String FILE_URL = "file:///android_asset/autofill.html";

    public static final int AUTOFILL_VIEW_ENTERED = 1;
    public static final int AUTOFILL_VIEW_EXITED = 2;
    public static final int AUTOFILL_VALUE_CHANGED = 3;
    public static final int AUTOFILL_COMMIT = 4;
    public static final int AUTOFILL_CANCEL = 5;

    /**
     * This class only implements the necessary methods of ViewStructure for testing.
     */
    private static class TestViewStructure extends ViewStructure {
        /**
         * Implementation of HtmlInfo.
         */
        public static class AwHtmlInfo extends HtmlInfo {
            private String mTag;
            private List<Pair<String, String>> mAttributes;
            public AwHtmlInfo(String tag, List<Pair<String, String>> attributes) {
                mTag = tag;
                mAttributes = attributes;
            }

            @Override
            public List<Pair<String, String>> getAttributes() {
                return mAttributes;
            }

            public String getAttribute(String attribute) {
                for (Pair<String, String> pair : mAttributes) {
                    if (attribute.equals(pair.first)) {
                        return pair.second;
                    }
                }
                return null;
            }

            @Override
            public String getTag() {
                return mTag;
            }
        }

        /**
         * Implementation of Builder
         */
        public static class AwBuilder extends Builder {
            private String mTag;
            private ArrayList<Pair<String, String>> mAttributes;
            public AwBuilder(String tag) {
                mTag = tag;
                mAttributes = new ArrayList<Pair<String, String>>();
            }

            @Override
            public Builder addAttribute(String name, String value) {
                mAttributes.add(new Pair<String, String>(name, value));
                return this;
            }

            @Override
            public HtmlInfo build() {
                return new AwHtmlInfo(mTag, mAttributes);
            }
        }

        public TestViewStructure() {
            mChildren = new ArrayList<TestViewStructure>();
        }

        @Override
        public void setAlpha(float alpha) {}

        @Override
        public void setAccessibilityFocused(boolean state) {}

        @Override
        public void setCheckable(boolean state) {}

        @Override
        public void setChecked(boolean state) {
            mChecked = state;
        }

        public boolean getChecked() {
            return mChecked;
        }

        @Override
        public void setActivated(boolean state) {}

        @Override
        public CharSequence getText() {
            return null;
        }

        @Override
        public int getTextSelectionStart() {
            return 0;
        }

        @Override
        public int getTextSelectionEnd() {
            return 0;
        }

        @Override
        public CharSequence getHint() {
            return mHint;
        }

        @Override
        public Bundle getExtras() {
            return null;
        }

        @Override
        public boolean hasExtras() {
            return false;
        }

        @Override
        public void setChildCount(int num) {}

        @Override
        public int addChildCount(int num) {
            int index = mChildCount;
            mChildCount += num;
            mChildren.ensureCapacity(mChildCount);
            return index;
        }

        @Override
        public int getChildCount() {
            return mChildren.size();
        }

        @Override
        public ViewStructure newChild(int index) {
            TestViewStructure viewStructure = new TestViewStructure();
            if (index < mChildren.size()) {
                mChildren.set(index, viewStructure);
            } else if (index == mChildren.size()) {
                mChildren.add(viewStructure);
            } else {
                // Failed intentionally, we shouldn't run into this case.
                mChildren.add(index, viewStructure);
            }
            return viewStructure;
        }

        public TestViewStructure getChild(int index) {
            return mChildren.get(index);
        }

        @Override
        public ViewStructure asyncNewChild(int index) {
            return null;
        }

        @Override
        public void asyncCommit() {}

        @Override
        public AutofillId getAutofillId() {
            Parcel parcel = Parcel.obtain();
            // Check AutofillId(Parcel) in AutofillId.java, always set parent id as 0.
            parcel.writeInt(0);
            parcel.writeInt(1);
            parcel.writeInt(mId);

            return AutofillId.CREATOR.createFromParcel(parcel);
        }

        @Override
        public Builder newHtmlInfoBuilder(String tag) {
            return new AwBuilder(tag);
        }

        @Override
        public void setAutofillHints(String[] arg0) {
            mAutofillHints = arg0.clone();
        }

        public String[] getAutofillHints() {
            if (mAutofillHints == null) return null;
            return mAutofillHints.clone();
        }

        @Override
        public void setAutofillId(AutofillId arg0) {}

        @Override
        public void setAutofillId(AutofillId arg0, int arg1) {
            mId = arg1;
        }

        public int getId() {
            return mId;
        }

        @Override
        public void setAutofillOptions(CharSequence[] arg0) {
            mAutofillOptions = arg0.clone();
        }

        public CharSequence[] getAutofillOptions() {
            if (mAutofillOptions == null) return null;
            return mAutofillOptions.clone();
        }

        @Override
        public void setAutofillType(int arg0) {
            mAutofillType = arg0;
        }

        public int getAutofillType() {
            return mAutofillType;
        }

        @Override
        public void setAutofillValue(AutofillValue arg0) {
            mAutofillValue = arg0;
        }

        public AutofillValue getAutofillValue() {
            return mAutofillValue;
        }

        @Override
        public void setId(int id, String packageName, String typeName, String entryName) {}

        @Override
        public void setDimens(int left, int top, int scrollX, int scrollY, int width, int height) {}

        @Override
        public void setElevation(float elevation) {}

        @Override
        public void setEnabled(boolean state) {}

        @Override
        public void setClickable(boolean state) {}

        @Override
        public void setLongClickable(boolean state) {}

        @Override
        public void setContextClickable(boolean state) {}

        @Override
        public void setFocusable(boolean state) {}

        @Override
        public void setFocused(boolean state) {}

        @Override
        public void setClassName(String className) {
            mClassName = className;
        }

        public String getClassName() {
            return mClassName;
        }

        @Override
        public void setContentDescription(CharSequence contentDescription) {}

        @Override
        public void setDataIsSensitive(boolean arg0) {
            mDataIsSensitive = arg0;
        }

        public boolean getDataIsSensitive() {
            return mDataIsSensitive;
        }

        @Override
        public void setHint(CharSequence hint) {
            mHint = hint;
        }

        @Override
        public void setHtmlInfo(HtmlInfo arg0) {
            mAwHtmlInfo = (AwHtmlInfo) arg0;
        }

        public AwHtmlInfo getHtmlInfo() {
            return mAwHtmlInfo;
        }

        @Override
        public void setInputType(int arg0) {}

        @Override
        public void setLocaleList(LocaleList arg0) {}

        @Override
        public void setOpaque(boolean arg0) {}

        @Override
        public void setTransformation(Matrix matrix) {}

        @Override
        public void setVisibility(int visibility) {}

        @Override
        public void setSelected(boolean state) {}

        @Override
        public void setText(CharSequence text) {}

        @Override
        public void setText(CharSequence text, int selectionStart, int selectionEnd) {}

        @Override
        public void setTextStyle(float size, int fgColor, int bgColor, int style) {}

        @Override
        public void setTextLines(int[] charOffsets, int[] baselines) {}

        @Override
        public void setWebDomain(String webDomain) {
            mWebDomain = webDomain;
        }

        public String getWebDomain() {
            return mWebDomain;
        }

        private int mAutofillType;
        private CharSequence mHint;
        private String[] mAutofillHints;
        private int mId;
        private String mClassName;
        private String mWebDomain;
        private int mChildCount;
        private ArrayList<TestViewStructure> mChildren;
        private CharSequence[] mAutofillOptions;
        private AutofillValue mAutofillValue;
        private boolean mDataIsSensitive;
        private AwHtmlInfo mAwHtmlInfo;
        private boolean mChecked;
    }

    // crbug.com/776230: On Android L, declaring variables of unsupported classes causes an error.
    // Wrapped them in a class to avoid it.
    private static class TestValues {
        public TestViewStructure testViewStructure;
        public ArrayList<Pair<Integer, AutofillValue>> changedValues;
    }

    private class TestAwAutofillManager extends AwAutofillManager {
        private boolean mDisabled;

        public TestAwAutofillManager(Context context) {
            super(context);
        }

        public void setDisabled() {
            mDisabled = true;
        }

        @Override
        public boolean isDisabled() {
            return mDisabled;
        }

        @Override
        public void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
            if (DEBUG) Log.i(TAG, "notifyVirtualViewEntered");
            mEventQueue.add(AUTOFILL_VIEW_ENTERED);
            mCallbackHelper.notifyCalled();
        }

        @Override
        public void notifyVirtualViewExited(View parent, int childId) {
            if (DEBUG) Log.i(TAG, "notifyVirtualViewExited");
            mEventQueue.add(AUTOFILL_VIEW_EXITED);
            mCallbackHelper.notifyCalled();
        }

        @Override
        public void notifyVirtualValueChanged(View parent, int childId, AutofillValue value) {
            if (DEBUG) Log.i(TAG, "notifyVirtualValueChanged");
            if (mTestValues.changedValues == null) {
                mTestValues.changedValues = new ArrayList<Pair<Integer, AutofillValue>>();
            }
            mTestValues.changedValues.add(new Pair<Integer, AutofillValue>(childId, value));
            mEventQueue.add(AUTOFILL_VALUE_CHANGED);
            mCallbackHelper.notifyCalled();
        }

        @Override
        public void commit(int submissionSource) {
            if (DEBUG) Log.i(TAG, "commit");
            mEventQueue.add(AUTOFILL_COMMIT);
            mSubmissionSource = submissionSource;
            mCallbackHelper.notifyCalled();
        }

        @Override
        public void cancel() {
            if (DEBUG) Log.i(TAG, "cancel");
            mEventQueue.add(AUTOFILL_CANCEL);
            mCallbackHelper.notifyCalled();
        }
    }

    private static class AwAutofillTestClient extends TestAwContentsClient {
        public interface ShouldInterceptRequestImpl {
            AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request);
        }

        private ShouldInterceptRequestImpl mShouldInterceptRequestImpl;

        public void setShouldInterceptRequestImpl(ShouldInterceptRequestImpl impl) {
            mShouldInterceptRequestImpl = impl;
        }

        @Override
        public AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request) {
            AwWebResourceResponse response = null;
            if (mShouldInterceptRequestImpl != null) {
                response = mShouldInterceptRequestImpl.shouldInterceptRequest(request);
            }
            if (response != null) return response;
            return super.shouldInterceptRequest(request);
        }
    }

    private static class AwAutofillSessionUMATestHelper {
        private static final String DATA =
                "<html><head></head><body><form action='a.html' name='formname' id='formid'>"
                + "<label>User Name:</label>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "<input type='submit'>"
                + "</form>"
                + "<form><input type='text' id='text2'/></form></body></html>";

        private static final int TOTAL_CONTROLS = 1; // text1

        public static final int NO_FORM_SUBMISSION = -1;

        public AwAutofillSessionUMATestHelper(AwAutofillTest test) {
            mTest = test;
            initDeltaSamples();
        }

        public int getSessionValue() {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> { mSessionValue = getUMAEnumerateValue(mSessionDelta); });
            return mSessionValue;
        }

        public int getSubmissionSourceValue() {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> { mSourceValue = getUMAEnumerateValue(mSubmissionSourceDelta); });
            return mSourceValue;
        }

        private int getUMAEnumerateValue(HashMap<MetricsUtils.HistogramDelta, Integer> deltas) {
            int value = NO_FORM_SUBMISSION;
            for (MetricsUtils.HistogramDelta delta : deltas.keySet()) {
                if (delta.getDelta() != 0) {
                    assertEquals(NO_FORM_SUBMISSION, value);
                    value = deltas.get(delta);
                }
            }
            return value;
        }

        public void triggerAutofill(TestWebServer webServer) throws Throwable {
            final String url = webServer.setResponse(FILE, DATA, null);
            mTest.loadUrlSync(url);
            mTest.executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            mTest.dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            // Note that we currently call ENTER/EXIT one more time.
            mCnt += mTest.waitForCallbackAndVerifyTypes(mCnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
        }

        public void simulateUserSelectSuggestion() throws Throwable {
            // Simulate user select suggestion
            TestViewStructure viewStructure = mTest.mTestValues.testViewStructure;
            assertNotNull(viewStructure);
            assertEquals(TOTAL_CONTROLS, viewStructure.getChildCount());

            TestViewStructure child0 = viewStructure.getChild(0);

            // Autofill form and verify filled values.
            SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
            values.append(child0.getId(), AutofillValue.forText("example@example.com"));
            mCnt = mTest.getCallbackCount();
            mTest.clearChangedValues();
            mTest.invokeAutofill(values);
            mCnt += mTest.waitForCallbackAndVerifyTypes(
                    mCnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        }

        public void simulateUserChangeAutofilledField() throws Throwable {
            mTest.executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            mTest.dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
            mCnt += mTest.waitForCallbackAndVerifyTypes(
                    mCnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        }

        public void submitForm() throws Throwable {
            mTest.executeJavaScriptAndWaitForResult("document.getElementById('formid').submit();");
            mCnt += mTest.waitForCallbackAndVerifyTypes(
                    mCnt, new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT});
        }

        public void startNewSession() throws Throwable {
            // Start a new session by moving focus to another form.
            mTest.executeJavaScriptAndWaitForResult("document.getElementById('text2').select();");
            mTest.dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            // Note that we currently call ENTER/EXIT one more time.
            mCnt += mTest.waitForCallbackAndVerifyTypes(mCnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
        }

        public void simulateUserChangeField() throws Throwable {
            mTest.executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            mTest.dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
            mCnt += mTest.waitForCallbackAndVerifyTypes(
                    mCnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        }

        private void initDeltaSamples() {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mSessionDelta = new HashMap<MetricsUtils.HistogramDelta, Integer>();
                for (int i = 0; i < AwAutofillUMA.AUTOFILL_SESSION_HISTOGRAM_COUNT; i++) {
                    mSessionDelta.put(
                            new MetricsUtils.HistogramDelta(
                                    AwAutofillUMA.UMA_AUTOFILL_WEBVIEW_AUTOFILL_SESSION, i),
                            i);
                }
                mSubmissionSourceDelta = new HashMap<MetricsUtils.HistogramDelta, Integer>();
                for (int i = 0; i < AwAutofillUMA.SUBMISSION_SOURCE_HISTOGRAM_COUNT; i++) {
                    mSubmissionSourceDelta.put(
                            new MetricsUtils.HistogramDelta(
                                    AwAutofillUMA.UMA_AUTOFILL_WEBVIEW_SUBMISSION_SOURCE, i),
                            i);
                }
                mAutofillWebViewViewEnabled = new MetricsUtils.HistogramDelta(
                        AwAutofillUMA.UMA_AUTOFILL_WEBVIEW_ENABLED, 1 /*true*/);
                mAutofillWebViewViewDisabled = new MetricsUtils.HistogramDelta(
                        AwAutofillUMA.UMA_AUTOFILL_WEBVIEW_ENABLED, 0 /*false*/);
                mAutofillWebViewCreatedByActivityContext = new MetricsUtils.HistogramDelta(
                        AwAutofillUMA.UMA_AUTOFILL_WEBVIEW_CREATED_BY_ACTIVITY_CONTEXT, 1);
                mAutofillWebViewCreatedByAppContext = new MetricsUtils.HistogramDelta(
                        AwAutofillUMA.UMA_AUTOFILL_WEBVIEW_CREATED_BY_ACTIVITY_CONTEXT, 0);
                mUserChangedAutofilledField = new MetricsUtils.HistogramDelta(
                        AwAutofillUMA.UMA_AUTOFILL_USER_CHANGED_AUTOFILLED_FIELD, 1 /*true*/);
                mUserChangedNonAutofilledField = new MetricsUtils.HistogramDelta(
                        AwAutofillUMA.UMA_AUTOFILL_USER_CHANGED_AUTOFILLED_FIELD, 0 /*falsTe*/);
            });
        }

        public int getHistogramSampleCount(String name) {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mHistogramSimpleCount =
                        Integer.valueOf(RecordHistogram.getHistogramTotalCountForTesting(name));
            });
            return mHistogramSimpleCount;
        }

        public void verifyAutofillEnabled() {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                assertEquals(1, mAutofillWebViewViewEnabled.getDelta());
                assertEquals(0, mAutofillWebViewViewDisabled.getDelta());
            });
        }

        public void verifyAutofillDisabled() {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                assertEquals(0, mAutofillWebViewViewEnabled.getDelta());
                assertEquals(1, mAutofillWebViewViewDisabled.getDelta());
            });
        }

        public void verifyUserChangedAutofilledField() {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                assertEquals(0, mUserChangedNonAutofilledField.getDelta());
                assertEquals(1, mUserChangedAutofilledField.getDelta());
            });
        }

        public void verifyUserChangedNonAutofilledField() {
            // User changed the form, but not the autofilled field.
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                assertEquals(1, mUserChangedNonAutofilledField.getDelta());
                assertEquals(0, mUserChangedAutofilledField.getDelta());
            });
        }

        public void verifyUserDidntChangeForm() {
            // User didn't change the form at all.
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                assertEquals(0, mUserChangedNonAutofilledField.getDelta());
                assertEquals(0, mUserChangedAutofilledField.getDelta());
            });
        }

        public void verifyWebViewCreatedByActivityContext() {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                assertEquals(1, mAutofillWebViewCreatedByActivityContext.getDelta());
                assertEquals(0, mAutofillWebViewCreatedByAppContext.getDelta());
            });
        }

        private int mCnt;
        private AwAutofillTest mTest;
        private volatile Integer mSessionValue;
        private HashMap<MetricsUtils.HistogramDelta, Integer> mSessionDelta;
        private MetricsUtils.HistogramDelta mAutofillWebViewViewEnabled;
        private MetricsUtils.HistogramDelta mAutofillWebViewViewDisabled;
        private MetricsUtils.HistogramDelta mUserChangedAutofilledField;
        private MetricsUtils.HistogramDelta mUserChangedNonAutofilledField;
        private MetricsUtils.HistogramDelta mAutofillWebViewCreatedByActivityContext;
        private MetricsUtils.HistogramDelta mAutofillWebViewCreatedByAppContext;
        private volatile Integer mSourceValue;
        private HashMap<MetricsUtils.HistogramDelta, Integer> mSubmissionSourceDelta;
        private volatile Integer mHistogramSimpleCount;
    }

    @Rule
    public AwActivityTestRule mRule = new AwActivityTestRule();

    private AwTestContainerView mTestContainerView;
    private AwAutofillTestClient mContentsClient;
    private CallbackHelper mCallbackHelper = new CallbackHelper();
    private AwContents mAwContents;
    private ConcurrentLinkedQueue<Integer> mEventQueue = new ConcurrentLinkedQueue<>();
    private TestValues mTestValues = new TestValues();
    private int mSubmissionSource;
    private TestAwAutofillManager mTestAwAutofillManager;
    private AwAutofillSessionUMATestHelper mUMATestHelper;

    @Before
    public void setUp() {
        mUMATestHelper = new AwAutofillSessionUMATestHelper(this);
        mContentsClient = new AwAutofillTestClient();
        mTestContainerView = mRule.createAwTestContainerViewOnMainSync(
                mContentsClient, false, new TestDependencyFactory() {
                    @Override
                    public AutofillProvider createAutofillProvider(
                            Context context, ViewGroup containerView) {
                        mTestAwAutofillManager = new TestAwAutofillManager(context);
                        return new AwAutofillProvider(
                                containerView, mTestAwAutofillManager, context);
                    }
                });
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testTouchingFormWithAdjustResize() throws Throwable {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mRule.getActivity().getWindow().setSoftInputMode(
                    WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        });
        internalTestTriggerTest();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testTouchingFormWithAdjustPan() throws Throwable {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mRule.getActivity().getWindow().setSoftInputMode(
                    WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN);
        });
        internalTestTriggerTest();
    }

    private void internalTestTriggerTest() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "<input type='submit'>"
                + "</form></body></html>";
        try {
            int cnt = 0;
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "text1");
            // Note that we currently depend on keyboard app's behavior.
            // TODO(changwan): mock out IME interaction.
            Assert.assertTrue(DOMUtils.clickNode(mTestContainerView.getWebContents(), "text1"));
            cnt += waitForCallbackAndVerifyTypes(
                    cnt, new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED});
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});

            executeJavaScriptAndWaitForResult("document.getElementById('text1').blur();");
            waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VIEW_EXITED});
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testBasicAutofill() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<label>User Name:</label>"
                + "<input type='text' id='text1' name='name' maxlength='30'"
                + " placeholder='placeholder@placeholder.com' autocomplete='name given-name'>"
                + "<input type='checkbox' id='checkbox1' name='showpassword'>"
                + "<select id='select1' name='month'>"
                + "<option value='1'>Jan</option>"
                + "<option value='2'>Feb</option>"
                + "</select><textarea id='textarea1'></textarea>"
                + "<div contenteditable id='div1'>hello</div>"
                + "<input type='submit'>"
                + "<input type='reset' id='reset1'>"
                + "<input type='color' id='color1'><input type='file' id='file1'>"
                + "<input type='image' id='image1'>"
                + "</form></body></html>";
        final int totalControls = 4; // text1, checkbox1, select1, textarea1
        try {
            int cnt = 0;
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            invokeOnProvideAutoFillVirtualStructure();
            TestViewStructure viewStructure = mTestValues.testViewStructure;
            assertNotNull(viewStructure);
            assertEquals(totalControls, viewStructure.getChildCount());

            // Verify form filled correctly in ViewStructure.
            URL pageURL = new URL(url);
            String webDomain =
                    new URL(pageURL.getProtocol(), pageURL.getHost(), pageURL.getPort(), "/")
                            .toString();
            assertEquals(webDomain, viewStructure.getWebDomain());
            // WebView shouldn't set class name.
            assertNull(viewStructure.getClassName());
            TestViewStructure.AwHtmlInfo htmlInfoForm = viewStructure.getHtmlInfo();
            assertEquals("form", htmlInfoForm.getTag());
            assertEquals("formname", htmlInfoForm.getAttribute("name"));

            // Verify input text control filled correctly in ViewStructure.
            TestViewStructure child0 = viewStructure.getChild(0);
            assertEquals(View.AUTOFILL_TYPE_TEXT, child0.getAutofillType());
            assertEquals("placeholder@placeholder.com", child0.getHint());
            assertEquals("name", child0.getAutofillHints()[0]);
            assertEquals("given-name", child0.getAutofillHints()[1]);
            TestViewStructure.AwHtmlInfo htmlInfo0 = child0.getHtmlInfo();
            assertEquals("text", htmlInfo0.getAttribute("type"));
            assertEquals("text1", htmlInfo0.getAttribute("id"));
            assertEquals("name", htmlInfo0.getAttribute("name"));
            assertEquals("User Name:", htmlInfo0.getAttribute("label"));
            assertEquals("30", htmlInfo0.getAttribute("maxlength"));
            assertEquals("NAME_FIRST", htmlInfo0.getAttribute("ua-autofill-hints"));

            // Verify checkbox control filled correctly in ViewStructure.
            TestViewStructure child1 = viewStructure.getChild(1);
            assertEquals(View.AUTOFILL_TYPE_TOGGLE, child1.getAutofillType());
            assertEquals("", child1.getHint());
            assertNull(child1.getAutofillHints());
            TestViewStructure.AwHtmlInfo htmlInfo1 = child1.getHtmlInfo();
            assertEquals("checkbox", htmlInfo1.getAttribute("type"));
            assertEquals("checkbox1", htmlInfo1.getAttribute("id"));
            assertEquals("showpassword", htmlInfo1.getAttribute("name"));
            assertEquals("", htmlInfo1.getAttribute("label"));
            assertNull(htmlInfo1.getAttribute("maxlength"));
            assertNull(htmlInfo1.getAttribute("ua-autofill-hints"));

            // Verify select control filled correctly in ViewStructure.
            TestViewStructure child2 = viewStructure.getChild(2);
            assertEquals(View.AUTOFILL_TYPE_LIST, child2.getAutofillType());
            assertEquals("", child2.getHint());
            assertNull(child2.getAutofillHints());
            TestViewStructure.AwHtmlInfo htmlInfo2 = child2.getHtmlInfo();
            assertEquals("month", htmlInfo2.getAttribute("name"));
            assertEquals("select1", htmlInfo2.getAttribute("id"));
            CharSequence[] options = child2.getAutofillOptions();
            assertEquals("Jan", options[0]);
            assertEquals("Feb", options[1]);

            // Verify textarea control is filled correctly in ViewStructure.
            TestViewStructure child3 = viewStructure.getChild(3);
            assertEquals(View.AUTOFILL_TYPE_TEXT, child3.getAutofillType());
            assertEquals("", child3.getHint());
            assertNull(child3.getAutofillHints());
            TestViewStructure.AwHtmlInfo htmlInfo3 = child3.getHtmlInfo();
            assertEquals("textarea1", htmlInfo3.getAttribute("name"));

            // Autofill form and verify filled values.
            SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
            values.append(child0.getId(), AutofillValue.forText("example@example.com"));
            values.append(child1.getId(), AutofillValue.forToggle(true));
            values.append(child2.getId(), AutofillValue.forList(1));
            values.append(child3.getId(), AutofillValue.forText("aaa"));
            cnt = getCallbackCount();
            clearChangedValues();
            invokeAutofill(values);
            waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED,
                            AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED});

            // Verify form filled by Javascript
            String value0 =
                    executeJavaScriptAndWaitForResult("document.getElementById('text1').value;");
            assertEquals("\"example@example.com\"", value0);
            String value1 = executeJavaScriptAndWaitForResult(
                    "document.getElementById('checkbox1').value;");
            assertEquals("\"on\"", value1);
            String value2 =
                    executeJavaScriptAndWaitForResult("document.getElementById('select1').value;");
            assertEquals("\"2\"", value2);
            String value3 = executeJavaScriptAndWaitForResult(
                    "document.getElementById('textarea1').value;");
            assertEquals("\"aaa\"", value3);
            ArrayList<Pair<Integer, AutofillValue>> changedValues = getChangedValues();
            assertEquals("example@example.com", changedValues.get(0).second.getTextValue());
            assertTrue(changedValues.get(1).second.getToggleValue());
            assertEquals(1, changedValues.get(2).second.getListValue());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotifyVirtualValueChanged() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "</form></body></html>";
        try {
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            int cnt = 0;
            executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
            // Check if NotifyVirtualValueChanged() called and value is 'a'.
            assertEquals(1, values.size());
            assertEquals("a", values.get(0).second.getTextValue());

            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);

            // Check if NotifyVirtualValueChanged() called again, first value is 'a',
            // second value is 'ab', and both time has the same id.
            waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
            values = getChangedValues();
            assertEquals(2, values.size());
            assertEquals("a", values.get(0).second.getTextValue());
            assertEquals("ab", values.get(1).second.getTextValue());
            assertEquals(values.get(0).first, values.get(1).first);
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testJavascriptNotTriggerNotifyVirtualValueChanged() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "</form></body></html>";
        try {
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            int cnt = 0;
            executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
            // Check if NotifyVirtualValueChanged() called and value is 'a'.
            assertEquals(1, values.size());
            assertEquals("a", values.get(0).second.getTextValue());
            executeJavaScriptAndWaitForResult("document.getElementById('text1').value='c';");
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                // There is no AUTOFILL_CANCEL from Android P.
                assertEquals(2, getCallbackCount());
            } else {
                assertEquals(3, getCallbackCount());
            }
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
            // Check if NotifyVirtualValueChanged() called one more time and value is 'cb', this
            // means javascript change didn't trigger the NotifyVirtualValueChanged().
            waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
            values = getChangedValues();
            assertEquals(2, values.size());
            assertEquals("a", values.get(0).second.getTextValue());
            assertEquals("cb", values.get(1).second.getTextValue());
            assertEquals(values.get(0).first, values.get(1).first);
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCommit() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        final String data =
                "<html><head></head><body><form action='a.html' name='formname' id='formid'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "<input type='password' id='passwordid' name='passwordname'"
                + "<input type='submit'>"
                + "</form></body></html>";
        try {
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            int cnt = 0;
            executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            invokeOnProvideAutoFillVirtualStructure();
            // Fill the password.
            executeJavaScriptAndWaitForResult("document.getElementById('passwordid').select();");
            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {
                            AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
            cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
            clearChangedValues();
            // Submit form.
            executeJavaScriptAndWaitForResult("document.getElementById('formid').submit();");
            waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {
                            AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT});
            ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
            assertEquals(2, values.size());
            assertEquals("a", values.get(0).second.getTextValue());
            assertEquals("b", values.get(1).second.getTextValue());
            assertEquals(SubmissionSource.FORM_SUBMISSION, mSubmissionSource);
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLoadFileURL() throws Throwable {
        int cnt = 0;
        loadUrlSync(FILE_URL);
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        // Cancel called for the first query.
        waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMovingToOtherForm() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        final String data =
                "<html><head></head><body><form action='a.html' name='formname' id='formid'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "<input type='submit'></form>"
                + "<form action='a.html' name='formname' id='formid2'>"
                + "<input type='text' id='text2' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "<input type='submit'>"
                + "</form></body></html>";
        try {
            int cnt = 0;
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            // Move to form2, cancel() should be called again.
            executeJavaScriptAndWaitForResult("document.getElementById('text2').select();");
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * This test is verifying new session starts if frame change.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSwitchFromIFrame() throws Throwable {
        // we intentionally load main frame and iframe from the same URL and make both have the
        // similar form, so the new session is triggered by frame change
        TestWebServer webServer = TestWebServer.start();
        final String data = "<html><head></head><body><form name='formname' id='formid'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "<input type='submit'></form>"
                + "<iframe id='myframe' src='" + FILE + "'></iframe>"
                + "</body></html>";
        final String iframeData = "<html><head></head><body><form name='formname' id='formid'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name' "
                + " autofocus>"
                + "<input type='submit'></form>"
                + "</body></html>";
        try {
            final String url = webServer.setResponse(FILE, data, null);
            mContentsClient.setShouldInterceptRequestImpl(
                    new AwAutofillTestClient.ShouldInterceptRequestImpl() {
                        private int mCallCount;

                        @Override
                        public AwWebResourceResponse shouldInterceptRequest(
                                AwWebResourceRequest request) {
                            try {
                                if (url.equals(request.url)) {
                                    // Only intercept the iframe's request.
                                    if (mCallCount == 1) {
                                        final String encoding = "UTF-8";
                                        return new AwWebResourceResponse("text/html", encoding,
                                                new ByteArrayInputStream(
                                                        iframeData.getBytes(encoding)));
                                    }
                                    mCallCount++;
                                }
                                return null;
                            } catch (Exception e) {
                                throw new RuntimeException(e);
                            }
                        }
                    });
            loadUrlSync(url);

            // Trigger the autofill in iframe.
            int count = clearEventQueueAndGetCallCount();
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            // Verify autofill session triggered.
            count += waitForCallbackAndVerifyTypes(count,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            // Verify focus is in iframe.
            assertEquals("true",
                    executeJavaScriptAndWaitForResult(
                            "document.getElementById('myframe').contentDocument.hasFocus()"));
            // Move focus to the main frame form.
            clearChangedValues();
            executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            // The new session starts because cancel() has been called.
            waitForCallbackAndVerifyTypes(count,
                    new Integer[] {AUTOFILL_VIEW_EXITED, AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_VALUE_CHANGED});
            ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
            assertEquals(1, values.size());
            assertEquals("a", values.get(0).second.getTextValue());
            // Verify focus isn't in iframe now.
            assertEquals("false",
                    executeJavaScriptAndWaitForResult(
                            "document.getElementById('myframe').contentDocument.hasFocus()"));
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * This test is verifying new session starts if frame change.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testTouchingPasswordFieldTriggerQuery() throws Throwable {
        int cnt = 0;
        TestWebServer webServer = TestWebServer.start();
        final String data =
                "<html><head></head><body><form action='a.html' name='formname' id='formid'>"
                + "<input type='password' id='passwordid' name='passwordname'"
                + "<input type='submit'>"
                + "</form></body></html>";
        try {
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
        } finally {
            webServer.shutdown();
            DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "passwordid");
            // Note that we currently depend on keyboard app's behavior.
            // TODO(changwan): mock out IME interaction.
            Assert.assertTrue(
                    DOMUtils.clickNode(mTestContainerView.getWebContents(), "passwordid"));
            cnt += waitForCallbackAndVerifyTypes(
                    cnt, new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED});
        }
    }

    /**
     * This test is verifying the session is still alive after navigation.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSessionAliveAfterNavigation() throws Throwable {
        int cnt = 0;
        TestWebServer webServer = TestWebServer.start();
        final String data = "<!DOCTYPE html>"
                + "<html>"
                + "<body>"
                + "<form action='a.html' name='formname' id='formid'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "<input type='password' id='passwordid' name='passwordname'>"
                + "</form>"
                + "</body>"
                + "</html>";
        final String success = "<!DOCTYPE html>"
                + "<html>"
                + "<body>"
                + "</body>"
                + "</html>";
        try {
            webServer.setResponse("/success.html", success, null);
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            executeJavaScriptAndWaitForResult("window.location.href = 'success.html'; ");
            waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {
                            AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT});
            assertEquals(SubmissionSource.PROBABLY_FORM_SUBMITTED, mSubmissionSource);
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * This test is verifying there is no callback if there is no form change between two
     * navigations.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNoSubmissionWithoutFillingForm() throws Throwable {
        int cnt = 0;
        TestWebServer webServer = TestWebServer.start();
        final String data = "<!DOCTYPE html>"
                + "<html>"
                + "<body>"
                + "<form action='a.html' name='formname' id='formid'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "<input type='password' id='passwordid' name='passwordname'>"
                + "</form>"
                + "</body>"
                + "</html>";
        final String success = "<!DOCTYPE html>"
                + "<html>"
                + "<body>"
                + "</body>"
                + "</html>";
        try {
            final String successUrl = webServer.setResponse("/success.html", success, null);
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            executeJavaScriptAndWaitForResult("window.location.href = 'success.html'; ");
            // There is no callback. AUTOFILL_CANCEL shouldn't be invoked.
            assertEquals(0, getCallbackCount());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.P,
            message = "This test is disabled on Android O because of https://crbug.com/997362")
    public void
    testSelectControlChangeNotification() throws Throwable {
        int cnt = 0;
        TestWebServer webServer = TestWebServer.start();
        final String data = "<!DOCTYPE html>"
                + "<html>"
                + "<body>"
                + "<form action='a.html' name='formname' id='formid'>"
                + "<input type='text' id='text1' name='username'>"
                + "<select id='color' autofocus><option value='red'>red</option><option "
                + "value='blue' id='blue'>blue</option></select>"
                + "</form>"
                + "</body>"
                + "</html>";
        try {
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            clearChangedValues();
            executeJavaScriptAndWaitForResult("document.getElementById('color').focus();");
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_DPAD_CENTER);
            // Use key B to select 'blue'.
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {
                            AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
            assertEquals(1, values.size());
            assertTrue(values.get(0).second.isList());
            assertEquals(1, values.get(0).second.getListValue());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.P,
            message = "This test is disabled on Android O because of https://crbug.com/997362")
    public void
    testSelectControlChangeStartAutofillSession() throws Throwable {
        int cnt = 0;
        TestWebServer webServer = TestWebServer.start();
        final String data = "<!DOCTYPE html>"
                + "<html>"
                + "<body>"
                + "<form action='a.html' name='formname' id='formid'>"
                + "<input type='text' id='text1' name='username'>"
                + "<select id='color' autofocus><option value='red'>red</option><option "
                + "value='blue' id='blue'>blue</option></select>"
                + "</form>"
                + "</body>"
                + "</html>";
        try {
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            // Change select control first shall start autofill session.
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_DPAD_CENTER);
            // Use key B to select 'blue'.
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
            assertEquals(1, values.size());
            assertTrue(values.get(0).second.isList());
            assertEquals(1, values.get(0).second.getListValue());
        } finally {
            webServer.shutdown();
        }
    }
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUserInitiatedJavascriptSelectControlChangeNotification() throws Throwable {
        int cnt = 0;
        TestWebServer webServer = TestWebServer.start();
        final String data = "<!DOCTYPE html>"
                + "<html>"
                + "<body>"
                + "<script>"
                + "function myFunction() {"
                + "  document.getElementById('color').value = 'blue';"
                + "}"
                + "</script>"
                + "<form action='a.html' name='formname' id='formid'>"
                + "<button onclick='myFunction();' autofocus>button </button>"
                + "<select id='color' autofocus><option value='red'>red</option><option "
                + "value='blue' id='blue'>blue</option></select>"
                + "</form>"
                + "</body>"
                + "</html>";
        try {
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            // Change select control first shall start autofill session.
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_DPAD_CENTER);
            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
            assertEquals(1, values.size());
            assertTrue(values.get(0).second.isList());
            assertEquals(1, values.get(0).second.getListValue());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testJavascriptNotTriggerSelectControlChangeNotification() throws Throwable {
        int cnt = 0;
        TestWebServer webServer = TestWebServer.start();
        final String data = "<!DOCTYPE html>"
                + "<html>"
                + "<body onload='myFunction();'>"
                + "<script>"
                + "function myFunction() {"
                + "  document.getElementById('color').value = 'blue';"
                + "}"
                + "</script>"
                + "<form action='a.html' name='formname' id='formid'>"
                + "<button onclick='myFunction();' autofocus>button </button>"
                + "<select id='color' autofocus><option value='red'>red</option><option "
                + "value='blue' id='blue'>blue</option></select>"
                + "</form>"
                + "</body>"
                + "</html>";
        try {
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            // There is no good way to verify no callback occurred, we just simulate user trigger
            // the autofill and verify autofill is only triggered once, then this proves javascript
            // didn't trigger the autofill, since
            // testUserInitiatedJavascriptSelectControlChangeNotification verified user's triggering
            // work.
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_DPAD_CENTER);
            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
            assertEquals(1, values.size());
            assertTrue(values.get(0).second.isList());
            assertEquals(1, values.get(0).second.getListValue());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUaAutofillHints() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<label for=\"frmAddressB\">Address</label>"
                + "<input name=\"bill-address\" id=\"frmAddressB\">"
                + "<label for=\"frmCityB\">City</label>"
                + "<input name=\"bill-city\" id=\"frmCityB\">"
                + "<label for=\"frmStateB\">State</label>"
                + "<input name=\"bill-state\" id=\"frmStateB\">"
                + "<label for=\"frmZipB\">Zip</label>"
                + "<input name=\"bill-zip\" id=\"frmZipB\">"
                + "<input type='checkbox' id='checkbox1' name='showpassword'>"
                + "<label for=\"frmCountryB\">Country</label>"
                + "<input name=\"bill-country\" id=\"frmCountryB\">"
                + "<input type='submit'>"
                + "</form></body></html>";
        final int totalControls = 6;
        try {
            int cnt = 0;
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            executeJavaScriptAndWaitForResult("document.getElementById('frmAddressB').select();");
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
            invokeOnProvideAutoFillVirtualStructure();
            TestViewStructure viewStructure = mTestValues.testViewStructure;
            assertNotNull(viewStructure);
            assertEquals(totalControls, viewStructure.getChildCount());

            TestViewStructure child0 = viewStructure.getChild(0);
            TestViewStructure.AwHtmlInfo htmlInfo0 = child0.getHtmlInfo();
            assertEquals("ADDRESS_HOME_LINE1", htmlInfo0.getAttribute("ua-autofill-hints"));

            TestViewStructure child1 = viewStructure.getChild(1);
            TestViewStructure.AwHtmlInfo htmlInfo1 = child1.getHtmlInfo();
            assertEquals("ADDRESS_HOME_CITY", htmlInfo1.getAttribute("ua-autofill-hints"));

            TestViewStructure child2 = viewStructure.getChild(2);
            TestViewStructure.AwHtmlInfo htmlInfo2 = child2.getHtmlInfo();
            assertEquals("ADDRESS_HOME_STATE", htmlInfo2.getAttribute("ua-autofill-hints"));

            TestViewStructure child3 = viewStructure.getChild(3);
            TestViewStructure.AwHtmlInfo htmlInfo3 = child3.getHtmlInfo();
            assertEquals("ADDRESS_HOME_ZIP", htmlInfo3.getAttribute("ua-autofill-hints"));

            TestViewStructure child4 = viewStructure.getChild(4);
            TestViewStructure.AwHtmlInfo htmlInfo4 = child4.getHtmlInfo();
            assertNull(htmlInfo4.getAttribute("ua-autofill-hints"));

            TestViewStructure child5 = viewStructure.getChild(5);
            TestViewStructure.AwHtmlInfo htmlInfo5 = child5.getHtmlInfo();
            assertEquals("ADDRESS_HOME_COUNTRY", htmlInfo5.getAttribute("ua-autofill-hints"));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectSuggestionUserChangeFormFormSubmitted() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            invokeOnProvideAutoFillVirtualStructure();
            invokeOnInputUIShown();
            mUMATestHelper.simulateUserSelectSuggestion();
            mUMATestHelper.simulateUserChangeField();
            mUMATestHelper.submitForm();
            assertEquals(AwAutofillUMA.USER_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED,
                    mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectSuggestionUserChangeFormNoFormSubmitted() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            invokeOnProvideAutoFillVirtualStructure();
            invokeOnInputUIShown();
            mUMATestHelper.simulateUserSelectSuggestion();
            mUMATestHelper.simulateUserChangeField();
            mUMATestHelper.startNewSession();
            assertEquals(AwAutofillUMA.USER_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED,
                    mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                    mUMATestHelper.getSubmissionSourceValue());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectNotSuggestionUserChangeFormNoFormSubmitted() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            int count = mUMATestHelper.getHistogramSampleCount(
                    AwAutofillUMA.UMA_AUTOFILL_WEBVIEW_SUGGESTION_TIME);
            mUMATestHelper.triggerAutofill(webServer);
            invokeOnProvideAutoFillVirtualStructure();
            invokeOnInputUIShown();
            mUMATestHelper.simulateUserChangeField();
            mUMATestHelper.startNewSession();
            assertEquals(
                    AwAutofillUMA.USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED,
                    mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                    mUMATestHelper.getSubmissionSourceValue());
            assertEquals(count + 1,
                    mUMATestHelper.getHistogramSampleCount(
                            AwAutofillUMA.UMA_AUTOFILL_WEBVIEW_SUGGESTION_TIME));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserNotSelectSuggestionUserChangeFormFormSubmitted() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            invokeOnProvideAutoFillVirtualStructure();
            invokeOnInputUIShown();
            mUMATestHelper.simulateUserChangeField();
            mUMATestHelper.submitForm();
            assertEquals(AwAutofillUMA.USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED,
                    mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoSuggestionUserChangeFormNoFormSubmitted() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            invokeOnProvideAutoFillVirtualStructure();
            mUMATestHelper.simulateUserChangeField();
            mUMATestHelper.startNewSession();
            assertEquals(AwAutofillUMA.NO_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED,
                    mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                    mUMATestHelper.getSubmissionSourceValue());
            mUMATestHelper.verifyUserChangedNonAutofilledField();
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoSuggestionUserChangeFormFormSubmitted() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            invokeOnProvideAutoFillVirtualStructure();
            mUMATestHelper.simulateUserChangeField();
            mUMATestHelper.submitForm();
            assertEquals(AwAutofillUMA.NO_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED,
                    mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
            mUMATestHelper.verifyUserChangedNonAutofilledField();
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectSuggestionUserNotChangeFormFormSubmitted() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            invokeOnProvideAutoFillVirtualStructure();
            invokeOnInputUIShown();
            mUMATestHelper.simulateUserSelectSuggestion();
            mUMATestHelper.submitForm();
            assertEquals(AwAutofillUMA.USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED,
                    mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
            mUMATestHelper.verifyUserDidntChangeForm();
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectSuggestionUserNotChangeFormNoFormSubmitted() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            invokeOnProvideAutoFillVirtualStructure();
            invokeOnInputUIShown();
            mUMATestHelper.simulateUserSelectSuggestion();
            mUMATestHelper.startNewSession();
            assertEquals(
                    AwAutofillUMA.USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED,
                    mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                    mUMATestHelper.getSubmissionSourceValue());
            mUMATestHelper.verifyUserDidntChangeForm();
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectNotSuggestionUserNotChangeFormNoFormSubmitted() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            invokeOnProvideAutoFillVirtualStructure();
            invokeOnInputUIShown();
            mUMATestHelper.startNewSession();
            assertEquals(
                    AwAutofillUMA.USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED,
                    mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                    mUMATestHelper.getSubmissionSourceValue());
            mUMATestHelper.verifyUserDidntChangeForm();
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserNotSelectSuggestionUserNotChangeFormFormSubmitted() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            invokeOnProvideAutoFillVirtualStructure();
            invokeOnInputUIShown();
            mUMATestHelper.submitForm();
            assertEquals(
                    AwAutofillUMA.USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED,
                    mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
            mUMATestHelper.verifyUserDidntChangeForm();
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoSuggestionUserNotChangeFormNoFormSubmitted() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            invokeOnProvideAutoFillVirtualStructure();
            mUMATestHelper.startNewSession();
            assertEquals(AwAutofillUMA.NO_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED,
                    mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                    mUMATestHelper.getSubmissionSourceValue());
            mUMATestHelper.verifyUserDidntChangeForm();
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoSuggestionUserNotChangeFormFormSubmitted() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            invokeOnProvideAutoFillVirtualStructure();
            mUMATestHelper.submitForm();
            assertEquals(AwAutofillUMA.NO_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED,
                    mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
            mUMATestHelper.verifyUserDidntChangeForm();
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoCallbackFromFramework() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            mUMATestHelper.startNewSession();
            assertEquals(
                    AwAutofillUMA.NO_CALLBACK_FORM_FRAMEWORK, mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                    mUMATestHelper.getSubmissionSourceValue());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAAutofillDisabled() throws Throwable {
        mTestAwAutofillManager.setDisabled();
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            mUMATestHelper.verifyAutofillDisabled();
            assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                    mUMATestHelper.getSubmissionSourceValue());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAAutofillEnabled() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            mUMATestHelper.verifyAutofillEnabled();
            assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                    mUMATestHelper.getSubmissionSourceValue());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserChangeAutofilledField() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mUMATestHelper.triggerAutofill(webServer);
            invokeOnProvideAutoFillVirtualStructure();
            invokeOnInputUIShown();
            mUMATestHelper.simulateUserSelectSuggestion();
            mUMATestHelper.simulateUserChangeAutofilledField();
            mUMATestHelper.submitForm();
            assertEquals(AwAutofillUMA.USER_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED,
                    mUMATestHelper.getSessionValue());
            assertEquals(AwAutofillUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
            mUMATestHelper.verifyUserChangedAutofilledField();
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAAutofillCreatedByActivityContext() {
        mUMATestHelper.verifyWebViewCreatedByActivityContext();
    }

    @Test
    @SmallTest
    @DisabledTest
    @Feature({"AndroidWebView"})
    public void testPageScrollTriggerViewExitAndEnter() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "</form><p style='height: 100vh'>Hello</p></body></html>";
        try {
            final String url = webServer.setResponse(FILE, data, null);
            loadUrlSync(url);
            int cnt = 0;
            executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

            cnt += waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});

            // Moved view, the position change trigger additional AUTOFILL_VIEW_EXITED and
            // AUTOFILL_VIEW_ENTERED.
            scrollToBottom();
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);

            // Check if NotifyVirtualValueChanged() called again and with extra AUTOFILL_VIEW_EXITED
            // and AUTOFILL_VIEW_ENTERED
            waitForCallbackAndVerifyTypes(cnt,
                    new Integer[] {
                            AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
        } finally {
            webServer.shutdown();
        }
    }

    private void scrollToBottom() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTestContainerView.scrollTo(0, mTestContainerView.getHeight()); });
    }

    private void loadUrlSync(String url) throws Exception {
        mRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);
    }

    private String executeJavaScriptAndWaitForResult(String code) throws Throwable {
        return mRule.executeJavaScriptAndWaitForResult(
                mTestContainerView.getAwContents(), mContentsClient, code);
    }

    private ArrayList<Pair<Integer, AutofillValue>> getChangedValues() {
        return mTestValues.changedValues;
    }

    private void clearChangedValues() {
        if (mTestValues.changedValues != null) mTestValues.changedValues.clear();
    }

    private void invokeOnProvideAutoFillVirtualStructure() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTestValues.testViewStructure = new TestViewStructure();
            mAwContents.onProvideAutoFillVirtualStructure(mTestValues.testViewStructure, 1);
        });
    }

    private void invokeAutofill(SparseArray<AutofillValue> values) {
        TestThreadUtils.runOnUiThreadBlocking(() -> mAwContents.autofill(values));
    }

    private void invokeOnInputUIShown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mTestAwAutofillManager.notifyInputUIChange());
    }

    private int getCallbackCount() {
        return mCallbackHelper.getCallCount();
    }

    private int clearEventQueueAndGetCallCount() {
        mEventQueue.clear();
        return mCallbackHelper.getCallCount();
    }

    /**
     * Wait for expected callbacks to be called, and verify the types.
     *
     * @param currentCallCount The current call count to start from.
     * @param expectedEventArray The callback types that need to be verified.
     * @return The number of new callbacks since currentCallCount. This should be same as the length
     *         of expectedEventArray.
     * @throws TimeoutException
     */
    private int waitForCallbackAndVerifyTypes(int currentCallCount, Integer[] expectedEventArray)
            throws TimeoutException {
        Integer[] adjustedEventArray;
        // Didn't call cancel after Android P.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            ArrayList<Integer> adjusted = new ArrayList<Integer>();
            for (Integer event : expectedEventArray) {
                if (event != AUTOFILL_CANCEL) adjusted.add(event);
            }
            adjustedEventArray = new Integer[adjusted.size()];
            adjusted.toArray(adjustedEventArray);
        } else {
            adjustedEventArray = expectedEventArray;
        }
        try {
            // Check against the call count to avoid missing out a callback in between waits, while
            // exposing it so that the test can control where the call count starts.
            mCallbackHelper.waitForCallback(currentCallCount, adjustedEventArray.length);
            Object[] objectArray = mEventQueue.toArray();
            mEventQueue.clear();
            Integer[] resultArray = Arrays.copyOf(objectArray, objectArray.length, Integer[].class);
            Assert.assertArrayEquals("Expect: " + Arrays.toString(adjustedEventArray)
                            + " Result: " + Arrays.toString(resultArray),
                    adjustedEventArray, resultArray);
            return adjustedEventArray.length;
        } catch (TimeoutException e) {
            Object[] objectArray = mEventQueue.toArray();
            Integer[] resultArray = Arrays.copyOf(objectArray, objectArray.length, Integer[].class);
            Assert.assertArrayEquals("Expect:" + Arrays.toString(adjustedEventArray)
                            + " Result:" + Arrays.toString(resultArray),
                    adjustedEventArray, resultArray);
            throw e;
        }
    }

    private void dispatchDownAndUpKeyEvents(final int code) throws Throwable {
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, code));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, code));
    }

    private boolean dispatchKeyEvent(final KeyEvent event) throws Throwable {
        return TestThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mTestContainerView.dispatchKeyEvent(event);
            }
        });
    }
}

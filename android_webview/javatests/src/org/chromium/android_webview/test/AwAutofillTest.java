// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Bundle;
import android.os.IBinder;
import android.os.LocaleList;
import android.os.Parcel;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.text.TextUtils;
import android.util.Pair;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewStructure;
import android.view.WindowManager;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillValue;

import androidx.annotation.RequiresApi;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.autofill.mojom.SubmissionSource;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.components.autofill.AutofillHintsServiceTestHelper;
import org.chromium.components.autofill.AutofillManagerWrapper;
import org.chromium.components.autofill.AutofillPopup;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.autofill.AutofillProviderTestHelper;
import org.chromium.components.autofill.AutofillProviderUMA;
import org.chromium.components.autofill_public.ViewType;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;

import java.io.ByteArrayInputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.TimeoutException;

/**
 * Tests for WebView Autofill.
 */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@MinAndroidSdkLevel(Build.VERSION_CODES.O)
@RequiresApi(Build.VERSION_CODES.O)
public class AwAutofillTest {
    public static final boolean DEBUG = false;
    public static final String TAG = "AutofillTest";

    public static final String FILE = "/login.html";
    public static final String FILE_URL = "file:///android_asset/autofill.html";

    public static final int AUTOFILL_VIEW_ENTERED = 0;
    public static final int AUTOFILL_VIEW_EXITED = 1;
    public static final int AUTOFILL_VALUE_CHANGED = 2;
    public static final int AUTOFILL_COMMIT = 3;
    public static final int AUTOFILL_CANCEL = 4;
    public static final int AUTOFILL_SESSION_STARTED = 5;
    public static final int AUTOFILL_QUERY_DONE = 6;
    public static final int AUTOFILL_EVENT_MAX = 7;

    public static final String[] EVENT = {"VIEW_ENTERED", "VIEW_EXITED", "VALUE_CHANGED", "COMMIT",
            "CANCEL", "SESSION_STARTED", "QUERY_DONE"};

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
        public static class AwBuilder extends HtmlInfo.Builder {
            private String mTag;
            private ArrayList<Pair<String, String>> mAttributes;
            public AwBuilder(String tag) {
                mTag = tag;
                mAttributes = new ArrayList<Pair<String, String>>();
            }

            @Override
            public HtmlInfo.Builder addAttribute(String name, String value) {
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
            if (mBundle == null) mBundle = new Bundle();
            return mBundle;
        }

        @Override
        public boolean hasExtras() {
            return mBundle != null;
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
        public HtmlInfo.Builder newHtmlInfoBuilder(String tag) {
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
        public void setDimens(int left, int top, int scrollX, int scrollY, int width, int height) {
            mDimensRect = new Rect(left, top, width + left, height + top);
            mDimensScrollX = scrollX;
            mDimensScrollY = scrollY;
        }

        public Rect getDimensRect() {
            return mDimensRect;
        }

        public int getDimensScrollX() {
            return mDimensScrollX;
        }

        public int getDimensScrollY() {
            return mDimensScrollY;
        }

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
        public void setVisibility(int visibility) {
            mVisibility = visibility;
        }

        public int getVisibility() {
            return mVisibility;
        }

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
        private Rect mDimensRect;
        private int mDimensScrollX;
        private int mDimensScrollY;
        private Bundle mBundle;
        // Initializes to the value AutofillProvider will never use.
        private int mVisibility = View.GONE;
    }

    // crbug.com/776230: On Android L, declaring variables of unsupported classes causes an error.
    // Wrapped them in a class to avoid it.
    private static class TestValues {
        public TestViewStructure testViewStructure;
        public ArrayList<Pair<Integer, AutofillValue>> changedValues;
    }

    private class TestAutofillManagerWrapper extends AutofillManagerWrapper {
        private boolean mDisabled;
        private boolean mQuerySucceed;

        public TestAutofillManagerWrapper(Context context) {
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
        public boolean isAwGCurrentAutofillService() {
            return sIsAwGCurrentAutofillService;
        }

        public boolean isQuerySucceed() {
            return mQuerySucceed;
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

        @Override
        public void notifyNewSessionStarted(boolean hasServerPrediction) {
            if (DEBUG) Log.i(TAG, "notifyNewSessionStarted");
            mEventQueue.add(AUTOFILL_SESSION_STARTED);
            mCallbackHelper.notifyCalled();
        }

        @Override
        public void onQueryDone(boolean success) {
            mQuerySucceed = success;
            if (DEBUG) Log.i(TAG, "onQueryDone " + success);
            mEventQueue.add(AUTOFILL_QUERY_DONE);
            mCallbackHelper.notifyCalled();
        }
    }

    private static class AwAutofillTestClient extends TestAwContentsClient {
        public interface ShouldInterceptRequestImpl {
            WebResourceResponseInfo shouldInterceptRequest(AwWebResourceRequest request);
        }

        private ShouldInterceptRequestImpl mShouldInterceptRequestImpl;

        public void setShouldInterceptRequestImpl(ShouldInterceptRequestImpl impl) {
            mShouldInterceptRequestImpl = impl;
        }

        @Override
        public WebResourceResponseInfo shouldInterceptRequest(AwWebResourceRequest request) {
            WebResourceResponseInfo response = null;
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
        public static final int NO_RECORD = -1;

        private int mCnt;
        private AwAutofillTest mTest;
        private TestWebServer mWebServer;
        private volatile Integer mSessionValue;
        private HashMap<MetricsUtils.HistogramDelta, Integer> mSessionDelta;
        private MetricsUtils.HistogramDelta mAutofillWebViewViewEnabled;
        private MetricsUtils.HistogramDelta mAutofillWebViewViewDisabled;
        private MetricsUtils.HistogramDelta mUserChangedAutofilledField;
        private MetricsUtils.HistogramDelta mUserChangedNonAutofilledField;
        private MetricsUtils.HistogramDelta mAutofillWebViewCreatedByActivityContext;
        private MetricsUtils.HistogramDelta mAutofillWebViewCreatedByAppContext;
        private MetricsUtils.HistogramDelta mAutofillHasInvalidServerPrediction;
        private MetricsUtils.HistogramDelta mAutofillHasValidServerPrediction;
        private MetricsUtils.HistogramDelta mAwGIsCurrentService;
        private MetricsUtils.HistogramDelta mAwGIsNotCurrentService;
        private volatile Integer mSourceValue;
        private volatile Integer mServerPredictionAvailabilityValue;
        private volatile Integer mAwGSuggestionAvailabilityValue;
        private HashMap<MetricsUtils.HistogramDelta, Integer> mSubmissionSourceDelta;
        private HashMap<MetricsUtils.HistogramDelta, Integer> mServerPredictionAvailablityDelta;
        private HashMap<MetricsUtils.HistogramDelta, Integer> mAwGSuggestionAvailablityDelta;
        private volatile Integer mHistogramSimpleCount;

        public AwAutofillSessionUMATestHelper(AwAutofillTest test, TestWebServer webServer) {
            mTest = test;
            mWebServer = webServer;
            initDeltaSamples();
        }

        public int getSessionValue() {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> { mSessionValue = getUMAEnumerateValue(mSessionDelta, null); });
            return mSessionValue;
        }

        public int getSubmissionSourceValue() {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mSourceValue = getUMAEnumerateValue(mSubmissionSourceDelta, NO_FORM_SUBMISSION);
            });
            return mSourceValue;
        }

        public int getServerPredictionAvailabilityValue() {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mServerPredictionAvailabilityValue =
                        getUMAEnumerateValue(mServerPredictionAvailablityDelta, null);
            });
            return mServerPredictionAvailabilityValue;
        }

        public int getAwGSuggestionAvailabilityValue() {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mAwGSuggestionAvailabilityValue =
                        getUMAEnumerateValue(mAwGSuggestionAvailablityDelta, NO_RECORD);
            });
            return mAwGSuggestionAvailabilityValue;
        }

        private int getUMAEnumerateValue(
                HashMap<MetricsUtils.HistogramDelta, Integer> deltas, Integer defaultValue) {
            Integer value = null;
            for (MetricsUtils.HistogramDelta delta : deltas.keySet()) {
                if (delta.getDelta() != 0) {
                    assertNull(value);
                    value = deltas.get(delta);
                }
            }
            if (defaultValue == null) assertNotNull(value);
            return value != null ? value : defaultValue;
        }

        public void triggerAutofill() throws Throwable {
            final String url = mWebServer.setResponse(FILE, DATA, null);
            mTest.loadUrlSync(url);
            mTest.executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            mTest.dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            mCnt += mTest.waitForCallbackAndVerifyTypes(mCnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED});
        }

        public void simulateServerPredictionBeforeTriggeringAutofill(int serverType)
                throws Throwable {
            final String url = mWebServer.setResponse(FILE, DATA, null);
            mTest.loadUrlSync(url);
            simulateServerPrediction(serverType);
            mTest.executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            mTest.dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            mCnt += mTest.waitForCallbackAndVerifyTypes(mCnt,
                    new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED});
        }

        public void simulateServerPrediction(int serverType) throws Throwable {
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> AutofillProviderTestHelper
                                       .simulateMainFrameAutofillServerResponseForTesting(
                                               mTest.mAwContents.getWebContents(),
                                               new String[] {"text1"}, new int[] {serverType}));
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
            mCnt += mTest.waitForCallbackAndVerifyTypes(mCnt,
                    new Integer[] {AUTOFILL_VIEW_EXITED, AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED, AUTOFILL_VALUE_CHANGED});
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
                for (int i = 0; i < AutofillProviderUMA.AUTOFILL_SESSION_HISTOGRAM_COUNT; i++) {
                    mSessionDelta.put(new MetricsUtils.HistogramDelta(
                                              AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION, i),
                            i);
                }
                mSubmissionSourceDelta = new HashMap<MetricsUtils.HistogramDelta, Integer>();
                for (int i = 0; i < AutofillProviderUMA.SUBMISSION_SOURCE_HISTOGRAM_COUNT; i++) {
                    mSubmissionSourceDelta.put(
                            new MetricsUtils.HistogramDelta(
                                    AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE, i),
                            i);
                }
                mServerPredictionAvailablityDelta =
                        new HashMap<MetricsUtils.HistogramDelta, Integer>();
                for (int i = 0; i < AutofillProviderUMA.SERVER_PREDICTION_AVAILABLE_COUNT; i++) {
                    mServerPredictionAvailablityDelta.put(
                            new MetricsUtils.HistogramDelta(
                                    AutofillProviderUMA.UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY,
                                    i),
                            i);
                }
                mAwGSuggestionAvailablityDelta =
                        new HashMap<MetricsUtils.HistogramDelta, Integer>();
                for (int i = 0; i < AutofillProviderUMA.AWG_SUGGSTION_AVAILABLE_COUNT; i++) {
                    mAwGSuggestionAvailablityDelta.put(
                            new MetricsUtils.HistogramDelta(
                                    AutofillProviderUMA.UMA_AUTOFILL_AWG_SUGGSTION_AVAILABILITY, i),
                            i);
                }
                mAutofillWebViewViewEnabled = new MetricsUtils.HistogramDelta(
                        AutofillProviderUMA.UMA_AUTOFILL_ENABLED, 1 /*true*/);
                mAutofillWebViewViewDisabled = new MetricsUtils.HistogramDelta(
                        AutofillProviderUMA.UMA_AUTOFILL_ENABLED, 0 /*false*/);
                mAutofillWebViewCreatedByActivityContext = new MetricsUtils.HistogramDelta(
                        AutofillProviderUMA.UMA_AUTOFILL_CREATED_BY_ACTIVITY_CONTEXT, 1);
                mAutofillWebViewCreatedByAppContext = new MetricsUtils.HistogramDelta(
                        AutofillProviderUMA.UMA_AUTOFILL_CREATED_BY_ACTIVITY_CONTEXT, 0);
                mUserChangedAutofilledField = new MetricsUtils.HistogramDelta(
                        AutofillProviderUMA.UMA_AUTOFILL_USER_CHANGED_AUTOFILLED_FIELD, 1 /*true*/);
                mUserChangedNonAutofilledField = new MetricsUtils.HistogramDelta(
                        AutofillProviderUMA.UMA_AUTOFILL_USER_CHANGED_AUTOFILLED_FIELD,
                        0 /*false*/);
                mAutofillHasInvalidServerPrediction = new MetricsUtils.HistogramDelta(
                        AutofillProviderUMA.UMA_AUTOFILL_VALID_SERVER_PREDICTION, 0 /*false*/);
                mAutofillHasValidServerPrediction = new MetricsUtils.HistogramDelta(
                        AutofillProviderUMA.UMA_AUTOFILL_VALID_SERVER_PREDICTION, 1 /*true*/);
                mAwGIsNotCurrentService = new MetricsUtils.HistogramDelta(
                        AutofillProviderUMA.UMA_AUTOFILL_AWG_IS_CURRENT_SERVICE, 0 /*false*/);
                mAwGIsCurrentService = new MetricsUtils.HistogramDelta(
                        AutofillProviderUMA.UMA_AUTOFILL_AWG_IS_CURRENT_SERVICE, 1 /*true*/);
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

        public void verifyAwGIsCurrentService(boolean current) {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                assertEquals(current ? 0 : 1, mAwGIsNotCurrentService.getDelta());
                assertEquals(current ? 1 : 0, mAwGIsCurrentService.getDelta());
            });
        }

        public void verifyServerPredictionValid(boolean valid) {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                assertEquals(valid ? 0 : 1, mAutofillHasInvalidServerPrediction.getDelta());
                assertEquals(valid ? 1 : 0, mAutofillHasValidServerPrediction.getDelta());
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
    }

    private static boolean sIsAwGCurrentAutofillService = true;

    @Rule
    public AwActivityTestRule mRule = new AwActivityTestRule();

    private TestWebServer mWebServer;
    private EmbeddedTestServer mEmbeddedServer;
    private AwTestContainerView mTestContainerView;
    private AwAutofillTestClient mContentsClient;
    private CallbackHelper mCallbackHelper = new CallbackHelper();
    private AwContents mAwContents;
    private ConcurrentLinkedQueue<Integer> mEventQueue = new ConcurrentLinkedQueue<>();
    private TestValues mTestValues = new TestValues();
    private int mSubmissionSource;
    private TestAutofillManagerWrapper mTestAutofillManagerWrapper;
    private AwAutofillSessionUMATestHelper mUMATestHelper;
    private AutofillProvider mAutofillProvider;

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
        mEmbeddedServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        AutofillProvider.setAutofillManagerWrapperFactoryForTesting(
                new AutofillProvider.AutofillManagerWrapperFactoryForTesting() {
                    @Override
                    public AutofillManagerWrapper create(Context context) {
                        mTestAutofillManagerWrapper = new TestAutofillManagerWrapper(context);
                        return mTestAutofillManagerWrapper;
                    }
                });
        mUMATestHelper = new AwAutofillSessionUMATestHelper(this, mWebServer);
        mContentsClient = new AwAutofillTestClient();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> AutofillProviderTestHelper.disableDownloadServerForTesting());
        mTestContainerView = mRule.createAwTestContainerViewOnMainSync(
                mContentsClient, false, new TestDependencyFactory());
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        mAutofillProvider = mAwContents.getAutofillProviderForTesting();
    }

    public void setUpAwGNotCurrent() throws Exception {
        sIsAwGCurrentAutofillService = false;
        mWebServer.shutdown();
        mEmbeddedServer.stopAndDestroyServer();
        // Initialize everything again.
        setUp();
    }

    @After
    public void tearDown() {
        sIsAwGCurrentAutofillService = true;
        mWebServer.shutdown();
        mEmbeddedServer.stopAndDestroyServer();
        mAutofillProvider = null;
    }

    public String getAbsoluteTestPageUrl(String relativePageUrl) {
        return mEmbeddedServer.getURL("/android_webview/test/data/autofill/" + relativePageUrl);
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
        int cnt = 0;
        final String url = getAbsoluteTestPageUrl("form_username.html");
        loadUrlSync(url);
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "text1");
        // Note that we currently depend on keyboard app's behavior.
        // TODO(changwan): mock out IME interaction.
        Assert.assertTrue(DOMUtils.clickNode(mTestContainerView.getWebContents(), "text1"));
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED});
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});

        executeJavaScriptAndWaitForResult("document.getElementById('text1').blur();");
        waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VIEW_EXITED});
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testBasicAutofill() throws Throwable {
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
        int cnt = 0;
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(totalControls, viewStructure.getChildCount());

        // Verify form filled correctly in ViewStructure.
        URL pageURL = new URL(url);
        String webDomain = new URL(pageURL.getProtocol(), pageURL.getHost(), pageURL.getPort(), "/")
                                   .toString();
        assertEquals(webDomain, viewStructure.getWebDomain());
        // WebView shouldn't set class name.
        assertNull(viewStructure.getClassName());
        Bundle extras = viewStructure.getExtras();
        assertEquals("Android WebView", extras.getCharSequence("VIRTUAL_STRUCTURE_PROVIDER_NAME"));
        assertTrue(0 < extras.getCharSequence("VIRTUAL_STRUCTURE_PROVIDER_VERSION").length());
        TestViewStructure.AwHtmlInfo htmlInfoForm = viewStructure.getHtmlInfo();
        assertEquals("form", htmlInfoForm.getTag());
        assertEquals("formname", htmlInfoForm.getAttribute("name"));

        // Verify input text control filled correctly in ViewStructure.
        TestViewStructure child0 = viewStructure.getChild(0);
        assertEquals(View.AUTOFILL_TYPE_TEXT, child0.getAutofillType());
        assertEquals("placeholder@placeholder.com", child0.getHint());
        assertEquals("name", child0.getAutofillHints()[0]);
        assertEquals("given-name", child0.getAutofillHints()[1]);
        assertFalse(child0.getDimensRect().isEmpty());
        // The field has no scroll, should always be zero.
        assertEquals(0, child0.getDimensScrollX());
        assertEquals(0, child0.getDimensScrollY());
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
        assertFalse(child1.getDimensRect().isEmpty());
        // The field has no scroll, should always be zero.
        assertEquals(0, child1.getDimensScrollX());
        assertEquals(0, child1.getDimensScrollY());
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
        assertFalse(child2.getDimensRect().isEmpty());
        // The field has no scroll, should always be zero.
        assertEquals(0, child2.getDimensScrollX());
        assertEquals(0, child2.getDimensScrollY());
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
        assertFalse(child3.getDimensRect().isEmpty());
        // The field has no scroll, should always be zero.
        assertEquals(0, child3.getDimensScrollX());
        assertEquals(0, child3.getDimensScrollY());
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

        // Autofilling the select control will move the focus on it, and triggers a value change
        // callback, so we get additional AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED and
        // AUTOFILL_VALUE_CHANGED events at the end.
        waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED,
                        AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED, AUTOFILL_VIEW_EXITED,
                        AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});

        // Verify form filled by Javascript
        String value0 =
                executeJavaScriptAndWaitForResult("document.getElementById('text1').value;");
        assertEquals("\"example@example.com\"", value0);
        String value1 =
                executeJavaScriptAndWaitForResult("document.getElementById('checkbox1').value;");
        assertEquals("\"on\"", value1);
        String value2 =
                executeJavaScriptAndWaitForResult("document.getElementById('select1').value;");
        assertEquals("\"2\"", value2);
        String value3 =
                executeJavaScriptAndWaitForResult("document.getElementById('textarea1').value;");
        assertEquals("\"aaa\"", value3);
        ArrayList<Pair<Integer, AutofillValue>> changedValues = getChangedValues();
        assertEquals("example@example.com", changedValues.get(0).second.getTextValue());
        assertTrue(changedValues.get(1).second.getToggleValue());
        assertEquals(1, changedValues.get(2).second.getListValue());
    }

    /**
     * Tests that a frame-transcending form is filled correctly.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=AutofillAcrossIframes"})
    @DisabledTest(message = "https://crbug.com/1401726")
    public void testCrossFrameAutofill() throws Throwable {
        final String data = "<html><body><form>"
                + "<input autocomplete=cc-name>"
                + "<iframe srcdoc='<input autocomplete=cc-number>'></iframe>"
                + "<iframe srcdoc='<input autocomplete=cc-exp>'></iframe>"
                + "<iframe srcdoc='<input autocomplete=cc-csc>'></iframe>"
                + "</form></body></html>";
        loadUrlSync(mWebServer.setResponse(FILE, data, null));
        int cnt = 0;
        executeJavaScriptAndWaitForResult(
                "window.frames[0].document.body.firstElementChild.select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);

        // Autofill form and verify filled values.
        SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
        values.append(viewStructure.getChild(0).getId(), AutofillValue.forText("Barack Obama"));
        values.append(viewStructure.getChild(1).getId(), AutofillValue.forText("4444333322221111"));
        values.append(viewStructure.getChild(2).getId(), AutofillValue.forText("12 / 2035"));
        values.append(viewStructure.getChild(3).getId(), AutofillValue.forText("123"));
        invokeAutofill(values);
        waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED,
                        AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED});

        assertEquals("\"Barack Obama\"",
                executeJavaScriptAndWaitForResult("document.forms[0].elements[0].value;"));
        assertEquals("\"4444333322221111\"",
                executeJavaScriptAndWaitForResult(
                        "window.frames[0].document.body.firstElementChild.value;"));
        assertEquals("\"12 / 2035\"",
                executeJavaScriptAndWaitForResult(
                        "window.frames[1].document.body.firstElementChild.value;"));
        assertEquals("\"123\"",
                executeJavaScriptAndWaitForResult(
                        "window.frames[2].document.body.firstElementChild.value;"));
    }

    /**
     * This test is verifying that a user interacting with a form after reloading a webpage
     * triggers a new autofill session rather than continuing a session that was started before the
     * reload. This is necessary to ensure that autofill is properly triggered in this case (see
     * crbug.com/1117563 for details).
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAutofillTriggersAfterReload() throws Throwable {
        int cnt = 0;

        final String url = getAbsoluteTestPageUrl("form_username.html");
        loadUrlSync(url);
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "text1");
        // TODO(changwan): mock out IME interaction.
        Assert.assertTrue(DOMUtils.clickNode(mTestContainerView.getWebContents(), "text1"));
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED});

        // Reload the page and check that the user clicking on the same form field ends the current
        // autofill session and starts a new session.
        reloadSync();
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "text1");
        // TODO(changwan): mock out IME interaction.
        Assert.assertTrue(DOMUtils.clickNode(mTestContainerView.getWebContents(), "text1"));
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_VIEW_EXITED, AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED,
                        AUTOFILL_SESSION_STARTED});
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotifyVirtualValueChanged() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_username.html");
        loadUrlSync(url);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
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
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testJavascriptNotTriggerNotifyVirtualValueChanged() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_username.html");
        loadUrlSync(url);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        // Check if NotifyVirtualValueChanged() called and value is 'a'.
        assertEquals(1, values.size());
        assertEquals("a", values.get(0).second.getTextValue());
        executeJavaScriptAndWaitForResult("document.getElementById('text1').value='c';");
        // Check no new event occurs, this is best effort checking, the event here could be leaked
        // from previous dispatchDownAndUpKeyEvents().
        assertEquals("Events in the queue "
                        + buildEventList(mEventQueue.toArray(new Integer[mEventQueue.size()])),
                cnt, getCallbackCount());
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        // Check if NotifyVirtualValueChanged() called one more time and value is 'cb', this
        // means javascript change didn't trigger the NotifyVirtualValueChanged().
        waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        values = getChangedValues();
        assertEquals(2, values.size());
        assertEquals("a", values.get(0).second.getTextValue());
        assertEquals("cb", values.get(1).second.getTextValue());
        assertEquals(values.get(0).first, values.get(1).first);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCommit() throws Throwable {
        final String data =
                "<html><head></head><body><form action='a.html' name='formname' id='formid'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "<input type='password' id='passwordid' name='passwordname'"
                + "<input type='submit'>"
                + "</form></body></html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
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
                new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT});
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(2, values.size());
        assertEquals("a", values.get(0).second.getTextValue());
        assertEquals("b", values.get(1).second.getTextValue());
        assertEquals(SubmissionSource.FORM_SUBMISSION, mSubmissionSource);
    }

    /**
     * Tests that when a multi-frame form is submitted in a subframe, we register the submission of
     * the overall form.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=AutofillAcrossIframes"})
    public void testCrossFrameCommit() throws Throwable {
        // The only reason we use a <form> inside the iframe is that this makes it easiest to
        // trigger a form submission in that frame.
        // TODO(crbug.com/1385768): Need to set the "id" so GetSimilarFieldIndex() doesn't confuse
        // the fields.
        final String data = "<html><head></head><body><form>"
                + "<input id=name>"
                + "<iframe srcdoc='<form action=arbitrary.html method=GET>"
                + "                <input id=num></form>'></iframe>"
                + "<iframe srcdoc='<input id=exp>'></iframe>"
                + "<iframe srcdoc='<input id=csc>'></iframe>"
                + "</form></body></html>";
        loadUrlSync(mWebServer.setResponse(FILE, data, null));
        int cnt = 0;
        // Fill name field.
        executeJavaScriptAndWaitForResult("document.forms[0].elements[0].select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        invokeOnProvideAutoFillVirtualStructure();
        // Fill number field.
        executeJavaScriptAndWaitForResult(
                "window.frames[0].document.forms[0].elements[0].select();");
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {
                        AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        clearChangedValues();
        // Fill expiration date field.
        executeJavaScriptAndWaitForResult(
                "window.frames[1].document.body.firstElementChild.select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_C);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {
                        AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        clearChangedValues();
        // Fill CVC field.
        executeJavaScriptAndWaitForResult(
                "window.frames[2].document.body.firstElementChild.select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_D);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {
                        AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        clearChangedValues();
        // Submit a form in the subframe.
        executeJavaScriptAndWaitForResult("window.frames[0].document.forms[0].submit();");
        waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED,
                        AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT});
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(4, values.size());
        assertEquals("a", values.get(0).second.getTextValue());
        assertEquals("b", values.get(1).second.getTextValue());
        assertEquals("c", values.get(2).second.getTextValue());
        assertEquals("d", values.get(3).second.getTextValue());
        assertEquals(SubmissionSource.FORM_SUBMISSION, mSubmissionSource);
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
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMovingToOtherForm() throws Throwable {
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
        int cnt = 0;
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        // Move to form2, cancel() should be called again.
        executeJavaScriptAndWaitForResult("document.getElementById('text2').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_VIEW_EXITED, AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED,
                        AUTOFILL_SESSION_STARTED, AUTOFILL_VALUE_CHANGED});
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
        final String url = mWebServer.setResponse(FILE, data, null);
        mContentsClient.setShouldInterceptRequestImpl(
                new AwAutofillTestClient.ShouldInterceptRequestImpl() {
                    private int mCallCount;

                    @Override
                    public WebResourceResponseInfo shouldInterceptRequest(
                            AwWebResourceRequest request) {
                        try {
                            if (url.equals(request.url)) {
                                // Only intercept the iframe's request.
                                if (mCallCount == 1) {
                                    final String encoding = "UTF-8";
                                    return new WebResourceResponseInfo("text/html", encoding,
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
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
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
                        AUTOFILL_SESSION_STARTED, AUTOFILL_VALUE_CHANGED});
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(1, values.size());
        assertEquals("a", values.get(0).second.getTextValue());
        // Verify focus isn't in iframe now.
        assertEquals("false",
                executeJavaScriptAndWaitForResult(
                        "document.getElementById('myframe').contentDocument.hasFocus()"));
    }

    /**
     * This test is verifying new session starts if frame change.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testTouchingPasswordFieldTriggerQuery() throws Throwable {
        int cnt = 0;
        final String data =
                "<html><head></head><body><form action='a.html' name='formname' id='formid'>"
                + "<input type='password' id='passwordid' name='passwordname'"
                + "<input type='submit'>"
                + "</form></body></html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "passwordid");
        // Note that we currently depend on keyboard app's behavior.
        // TODO(changwan): mock out IME interaction.
        Assert.assertTrue(DOMUtils.clickNode(mTestContainerView.getWebContents(), "passwordid"));
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED});
    }

    /**
     * This test is verifying that AutofillProvider correctly processes the removal and restoring
     * of focus on a form element.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFocusRemovedAndRestored() throws Throwable {
        int cnt = 0;
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
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);

        // Start the session by clicking on the username element.
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "text1");
        // TODO(changwan): mock out IME interaction.
        Assert.assertTrue(DOMUtils.clickNode(mTestContainerView.getWebContents(), "text1"));
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED});

        // Removing focus from this element should cause a notification that the autofill view was
        // exited.
        executeJavaScriptAndWaitForResult("document.getElementById('text1').blur();");
        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VIEW_EXITED});

        // Restoring focus on the form element should cause notifications to the autofill framework
        // that the autofill view was entered and value changed (AutofillProvider sends the latter
        // as a safeguard whenever focus changes to a new form element in the current session; it
        // was not sent as part of the first click above because at that point focus didn't change
        // to a *new* form element but was still on the element whose focusing had caused the
        // autofill session to start).
        Assert.assertTrue(DOMUtils.clickNode(mTestContainerView.getWebContents(), "text1"));
        waitForCallbackAndVerifyTypes(
                cnt, new Integer[] {AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
    }

    /**
     * This test is verifying that a navigation occurring while there is a probably-submitted
     * form will trigger commit of the current autofill session.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNavigationAfterProbableSubmitResultsInSessionCommit() throws Throwable {
        int cnt = 0;
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
        mWebServer.setResponse("/success.html", success, null);
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        executeJavaScriptAndWaitForResult("window.location.href = 'success.html'; ");
        waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_VIEW_EXITED, AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED,
                        AUTOFILL_COMMIT});
        assertEquals(SubmissionSource.PROBABLY_FORM_SUBMITTED, mSubmissionSource);
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
        final String successUrl = mWebServer.setResponse("/success.html", success, null);
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("window.location.href = 'success.html'; ");
        // There is no callback. AUTOFILL_CANCEL shouldn't be invoked.
        assertEquals(0, getCallbackCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.P,
            message = "This test is disabled on Android O because of https://crbug.com/997362")
    public void
    testSelectControlChangeNotification() throws Throwable {
        int cnt = 0;
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
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        clearChangedValues();
        executeJavaScriptAndWaitForResult("document.getElementById('color').focus();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_SPACE);
        // Use key B to select 'blue'.
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {
                        AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(1, values.size());
        assertTrue(values.get(0).second.isList());
        assertEquals(1, values.get(0).second.getListValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.P,
            message = "This test is disabled on Android O because of https://crbug.com/997362")
    public void
    testSelectControlChangeStartAutofillSession() throws Throwable {
        int cnt = 0;
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
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        // Change select control first shall start autofill session.
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_SPACE);
        // Use key B to select 'blue'.
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(1, values.size());
        assertTrue(values.get(0).second.isList());
        assertEquals(1, values.get(0).second.getListValue());

        // Verify the autofill session started by select control has dimens filled.
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        assertFalse(viewStructure.getChild(0).getDimensRect().isEmpty());
        // The field has no scroll, should always be zero.
        assertEquals(0, viewStructure.getChild(0).getDimensScrollX());
        assertEquals(0, viewStructure.getChild(0).getDimensScrollY());
        assertFalse(viewStructure.getChild(1).getDimensRect().isEmpty());
        // The field has no scroll, should always be zero.
        assertEquals(0, viewStructure.getChild(1).getDimensScrollX());
        assertEquals(0, viewStructure.getChild(1).getDimensScrollY());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUserInitiatedJavascriptSelectControlChangeNotification() throws Throwable {
        int cnt = 0;
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
                + "<select id='color'><option value='red'>red</option><option "
                + "value='blue' id='blue'>blue</option></select>"
                + "</form>"
                + "</body>"
                + "</html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        // Change select control first shall start autofill session.
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_SPACE);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(1, values.size());
        assertTrue(values.get(0).second.isList());
        assertEquals(1, values.get(0).second.getListValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testJavascriptNotTriggerSelectControlChangeNotification() throws Throwable {
        int cnt = 0;
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
                + "<select id='color'><option value='red'>red</option><option "
                + "value='blue' id='blue'>blue</option></select>"
                + "</form>"
                + "</body>"
                + "</html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        // There is no good way to verify no callback occurred, we just simulate user trigger
        // the autofill and verify autofill is only triggered once, then this proves javascript
        // didn't trigger the autofill, since
        // testUserInitiatedJavascriptSelectControlChangeNotification verified user's triggering
        // work.
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_SPACE);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(1, values.size());
        assertTrue(values.get(0).second.isList());
        assertEquals(1, values.get(0).second.getListValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUaAutofillHints() throws Throwable {
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
        int cnt = 0;
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('frmAddressB').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
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
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectSuggestionUserChangeFormFormSubmitted() throws Throwable {
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserSelectSuggestion();
        mUMATestHelper.simulateUserChangeField();
        mUMATestHelper.submitForm();
        assertEquals(AutofillProviderUMA.USER_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED,
                mUMATestHelper.getSessionValue());
        assertEquals(
                AutofillProviderUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
        assertEquals(AutofillProviderUMA.AWG_HAS_SUGGESTION_AUTOFILLED,
                mUMATestHelper.getAwGSuggestionAvailabilityValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectSuggestionUserChangeFormNoFormSubmitted() throws Throwable {
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserSelectSuggestion();
        mUMATestHelper.simulateUserChangeField();
        mUMATestHelper.startNewSession();
        assertEquals(AutofillProviderUMA.USER_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED,
                mUMATestHelper.getSessionValue());
        assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                mUMATestHelper.getSubmissionSourceValue());
        assertEquals(AwAutofillSessionUMATestHelper.NO_RECORD,
                mUMATestHelper.getAwGSuggestionAvailabilityValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectNotSuggestionUserChangeFormNoFormSubmitted() throws Throwable {
        int count = mUMATestHelper.getHistogramSampleCount(
                AutofillProviderUMA.UMA_AUTOFILL_SUGGESTION_TIME);
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserChangeField();
        mUMATestHelper.startNewSession();
        assertEquals(
                AutofillProviderUMA.USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED,
                mUMATestHelper.getSessionValue());
        assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                mUMATestHelper.getSubmissionSourceValue());
        assertEquals(count + 1,
                mUMATestHelper.getHistogramSampleCount(
                        AutofillProviderUMA.UMA_AUTOFILL_SUGGESTION_TIME));
        assertEquals(AwAutofillSessionUMATestHelper.NO_RECORD,
                mUMATestHelper.getAwGSuggestionAvailabilityValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserNotSelectSuggestionUserChangeFormFormSubmitted() throws Throwable {
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserChangeField();
        mUMATestHelper.submitForm();
        assertEquals(AutofillProviderUMA.USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED,
                mUMATestHelper.getSessionValue());
        assertEquals(
                AutofillProviderUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
        assertEquals(AutofillProviderUMA.AWG_HAS_SUGGESTION_NO_AUTOFILL,
                mUMATestHelper.getAwGSuggestionAvailabilityValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoSuggestionUserChangeFormNoFormSubmitted() throws Throwable {
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        mUMATestHelper.simulateUserChangeField();
        mUMATestHelper.startNewSession();
        assertEquals(AutofillProviderUMA.NO_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED,
                mUMATestHelper.getSessionValue());
        assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                mUMATestHelper.getSubmissionSourceValue());
        mUMATestHelper.verifyUserChangedNonAutofilledField();
        assertEquals(AwAutofillSessionUMATestHelper.NO_RECORD,
                mUMATestHelper.getAwGSuggestionAvailabilityValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoSuggestionUserChangeFormFormSubmitted() throws Throwable {
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        mUMATestHelper.simulateUserChangeField();
        mUMATestHelper.submitForm();
        assertEquals(AutofillProviderUMA.NO_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED,
                mUMATestHelper.getSessionValue());
        assertEquals(
                AutofillProviderUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
        mUMATestHelper.verifyUserChangedNonAutofilledField();
        assertEquals(AutofillProviderUMA.AWG_NO_SUGGESTION,
                mUMATestHelper.getAwGSuggestionAvailabilityValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectSuggestionUserNotChangeFormFormSubmitted() throws Throwable {
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserSelectSuggestion();
        mUMATestHelper.submitForm();
        assertEquals(AutofillProviderUMA.USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED,
                mUMATestHelper.getSessionValue());
        assertEquals(
                AutofillProviderUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
        mUMATestHelper.verifyUserDidntChangeForm();
        assertEquals(AutofillProviderUMA.AWG_HAS_SUGGESTION_AUTOFILLED,
                mUMATestHelper.getAwGSuggestionAvailabilityValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectSuggestionUserNotChangeFormNoFormSubmitted() throws Throwable {
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserSelectSuggestion();
        mUMATestHelper.startNewSession();
        assertEquals(
                AutofillProviderUMA.USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED,
                mUMATestHelper.getSessionValue());
        assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                mUMATestHelper.getSubmissionSourceValue());
        mUMATestHelper.verifyUserDidntChangeForm();
        assertEquals(AwAutofillSessionUMATestHelper.NO_RECORD,
                mUMATestHelper.getAwGSuggestionAvailabilityValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserNotSelectSuggestionUserNotChangeFormNoFormSubmitted() throws Throwable {
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.startNewSession();
        assertEquals(AutofillProviderUMA
                             .USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED,
                mUMATestHelper.getSessionValue());
        assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                mUMATestHelper.getSubmissionSourceValue());
        mUMATestHelper.verifyUserDidntChangeForm();
        assertEquals(AwAutofillSessionUMATestHelper.NO_RECORD,
                mUMATestHelper.getAwGSuggestionAvailabilityValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserNotSelectSuggestionUserNotChangeFormFormSubmitted() throws Throwable {
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.submitForm();
        assertEquals(
                AutofillProviderUMA.USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED,
                mUMATestHelper.getSessionValue());
        assertEquals(
                AutofillProviderUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
        mUMATestHelper.verifyUserDidntChangeForm();
        assertEquals(AutofillProviderUMA.AWG_HAS_SUGGESTION_NO_AUTOFILL,
                mUMATestHelper.getAwGSuggestionAvailabilityValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoSuggestionUserNotChangeFormNoFormSubmitted() throws Throwable {
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        mUMATestHelper.startNewSession();
        assertEquals(AutofillProviderUMA.NO_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED,
                mUMATestHelper.getSessionValue());
        assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                mUMATestHelper.getSubmissionSourceValue());
        mUMATestHelper.verifyUserDidntChangeForm();
        assertEquals(AwAutofillSessionUMATestHelper.NO_RECORD,
                mUMATestHelper.getAwGSuggestionAvailabilityValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoSuggestionUserNotChangeFormFormSubmitted() throws Throwable {
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        mUMATestHelper.submitForm();
        assertEquals(AutofillProviderUMA.NO_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED,
                mUMATestHelper.getSessionValue());
        assertEquals(
                AutofillProviderUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
        mUMATestHelper.verifyUserDidntChangeForm();
        assertEquals(AutofillProviderUMA.AWG_NO_SUGGESTION,
                mUMATestHelper.getAwGSuggestionAvailabilityValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoCallbackFromFramework() throws Throwable {
        mUMATestHelper.triggerAutofill();
        mUMATestHelper.startNewSession();
        assertEquals(
                AutofillProviderUMA.NO_CALLBACK_FORM_FRAMEWORK, mUMATestHelper.getSessionValue());
        assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                mUMATestHelper.getSubmissionSourceValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.P)
    public void testUMAAwGIsCurrentService() throws Throwable {
        mUMATestHelper.triggerAutofill();
        mUMATestHelper.verifyAwGIsCurrentService(/*current=*/true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.P)
    public void testUMAAwGIsNotCurrentService() throws Throwable {
        setUpAwGNotCurrent();
        mUMATestHelper.triggerAutofill();
        mUMATestHelper.verifyAwGIsCurrentService(/*current=*/false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoServerPrediction() throws Throwable {
        mUMATestHelper.triggerAutofill();
        mUMATestHelper.startNewSession();
        assertEquals(AutofillProviderUMA.SERVER_PREDICTION_NOT_AVAILABLE,
                mUMATestHelper.getServerPredictionAvailabilityValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAServerPredictionArriveBeforeSessionStart() throws Throwable {
        mUMATestHelper.simulateServerPredictionBeforeTriggeringAutofill(/*USERNAME*/ 86);
        assertEquals(AutofillProviderUMA.SERVER_PREDICTION_AVAILABLE_ON_SESSION_STARTS,
                mUMATestHelper.getServerPredictionAvailabilityValue());
        mUMATestHelper.verifyServerPredictionValid(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAServerPredictionArriveAfterSessionStart() throws Throwable {
        mUMATestHelper.triggerAutofill();
        mUMATestHelper.simulateServerPrediction(/*NO_SERVER_DATA*/ 0);
        assertEquals(AutofillProviderUMA.SERVER_PREDICTION_AVAILABLE_AFTER_SESSION_STARTS,
                mUMATestHelper.getServerPredictionAvailabilityValue());
        mUMATestHelper.verifyServerPredictionValid(false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAAutofillDisabled() throws Throwable {
        mTestAutofillManagerWrapper.setDisabled();
        mUMATestHelper.triggerAutofill();
        mUMATestHelper.verifyAutofillDisabled();
        assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                mUMATestHelper.getSubmissionSourceValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAAutofillEnabled() throws Throwable {
        mUMATestHelper.triggerAutofill();
        mUMATestHelper.verifyAutofillEnabled();
        assertEquals(AwAutofillSessionUMATestHelper.NO_FORM_SUBMISSION,
                mUMATestHelper.getSubmissionSourceValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserChangeAutofilledField() throws Throwable {
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserSelectSuggestion();
        mUMATestHelper.simulateUserChangeAutofilledField();
        mUMATestHelper.submitForm();
        assertEquals(AutofillProviderUMA.USER_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED,
                mUMATestHelper.getSessionValue());
        assertEquals(
                AutofillProviderUMA.FORM_SUBMISSION, mUMATestHelper.getSubmissionSourceValue());
        mUMATestHelper.verifyUserChangedAutofilledField();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAAutofillCreatedByActivityContext() {
        mUMATestHelper.verifyWebViewCreatedByActivityContext();
    }

    @Test
    @SmallTest
    public void testUmaFunnelMetrics() throws Throwable {
        HistogramWatcher.Builder histogramWatcherBuilder = HistogramWatcher.newBuilder();

        histogramWatcherBuilder
                .expectBooleanRecord("Autofill.WebView.Funnel.ParsedAsType.Address", true)
                // Ignore histogram for pages without any forms.
                .allowExtraRecords("Autofill.WebView.Funnel.ParsedAsType.Address")
                .expectBooleanRecord(
                        "Autofill.WebView.Funnel.InteractionAfterParsedAsType.Address", true)
                .expectBooleanRecord("Autofill.WebView.Funnel.FillAfterInteraction.Address", true)
                .expectBooleanRecord("Autofill.WebView.Funnel.SubmissionAfterFill.Address", true)
                .expectBooleanRecord("Autofill.WebView.KeyMetrics.FillingCorrectness.Address", true)
                .expectBooleanRecord("Autofill.WebView.KeyMetrics.FillingAssistance.Address", true)
                .expectBooleanRecord(
                        "Autofill.WebView.KeyMetrics.FormSubmission.Autofilled.Address", true);

        histogramWatcherBuilder
                .expectBooleanRecord("Autofill.WebView.Funnel.ParsedAsType.CreditCard", true)
                // Ignore histogram for pages without any forms.
                .allowExtraRecords("Autofill.WebView.Funnel.ParsedAsType.CreditCard")
                .expectBooleanRecord(
                        "Autofill.WebView.Funnel.InteractionAfterParsedAsType.CreditCard", false);
        histogramWatcherBuilder.expectNoRecords(
                "Autofill.WebView.Funnel.SubmissionAfterFill.CreditCard");

        HistogramWatcher histogramWatcher = histogramWatcherBuilder.build();

        final String url = getAbsoluteTestPageUrl("page_address_credit_card_forms.html");
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('address1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        waitForEvents(new Integer[] {AUTOFILL_SESSION_STARTED, AUTOFILL_VALUE_CHANGED});

        invokeOnProvideAutoFillVirtualStructure();
        int address1Id = mTestValues.testViewStructure.getChild(0).getId();
        SparseArray<AutofillValue> autofillValues = new SparseArray<AutofillValue>();
        autofillValues.append(address1Id, AutofillValue.forText("Jane Doe"));
        invokeAutofill(autofillValues);
        executeJavaScriptAndWaitForResult("document.getElementById('addressFormId').submit();");

        // All of the metrics are recorded at the same time. Wait for one of the metrics to be
        // recorded.
        CriteriaHelper.pollUiThread(() -> {
            int numSamples = RecordHistogram.getHistogramValueCountForTesting(
                    "Autofill.WebView.Funnel.ParsedAsType.Address", /*true=*/1);
            return numSamples > 0;
        });

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPageScrollTriggerViewExitAndEnter() throws Throwable {
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "</form><p style='height: 100vh'>Hello</p></body></html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});

        // Moved view, the position change trigger additional AUTOFILL_VIEW_EXITED and
        // AUTOFILL_VIEW_ENTERED.
        scrollToBottom();
        pollJavascriptResultNotEqualTo("document.body.scrollTop;", "0");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        List<Integer> expectedValues = new ArrayList<>();

        // On Android version below P scroll triggers additional
        // AUTOFILL_VIEW_ENTERED (@see AutofillProvider#onTextFieldDidScroll).
        if (VERSION.SDK_INT < Build.VERSION_CODES.P) {
            expectedValues.add(AUTOFILL_VIEW_ENTERED);
        }
        // Check if NotifyVirtualValueChanged() called again and with extra AUTOFILL_VIEW_EXITED
        // and AUTOFILL_VIEW_ENTERED
        expectedValues.addAll(
                Arrays.asList(AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED));
        waitForCallbackAndVerifyTypes(cnt, expectedValues.toArray(new Integer[0]));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMismatchedAutofillValueWontCauseCrash() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_username.html");
        loadUrlSync(url);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(1, viewStructure.getChildCount());
        TestViewStructure child0 = viewStructure.getChild(0);

        // Autofill form and verify filled values.
        SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
        // Append wrong autofill value.
        values.append(child0.getId(), AutofillValue.forToggle(false));
        // If the test fail, the exception shall be thrown in below.
        invokeAutofill(values);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDatalistSentToAutofillService() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_with_datalist.html");
        loadUrlSync(url);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());

        // Verified the datalist has correctly been sent to AutofillService
        TestViewStructure child1 = viewStructure.getChild(1);
        assertEquals(2, child1.getAutofillOptions().length);
        assertEquals("A1", child1.getAutofillOptions()[0]);
        assertEquals("A2", child1.getAutofillOptions()[1]);

        // Simulate autofilling the datalist by the AutofillService.
        SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
        values.append(child1.getId(), AutofillValue.forText("example@example.com"));
        invokeAutofill(values);
        cnt += waitForCallbackAndVerifyTypes(
                cnt, new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED});
        String value1 =
                executeJavaScriptAndWaitForResult("document.getElementById('text2').value;");
        assertEquals("\"example@example.com\"", value1);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNoEventSentToAutofillServiceForFocusedDatalist() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_with_datalist.html");
        loadUrlSync(url);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text2').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        // Verify not notifying AUTOFILL_VIEW_ENTERED and AUTOFILL_VALUE_CHANGED events for the
        // datalist.
        cnt += waitForCallbackAndVerifyTypes(
                cnt, new Integer[] {AUTOFILL_CANCEL, AUTOFILL_SESSION_STARTED});
        // Verify input accepted.
        String value1 =
                executeJavaScriptAndWaitForResult("document.getElementById('text2').value;");
        assertEquals("\"a\"", value1);
        // Move cursor to text1 and enter something.
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        // Verify no AUTOFILL_VIEW_EXITED sent for datalist and autofill service shall get the
        // events from the change of text1.
        cnt += waitForCallbackAndVerifyTypes(
                cnt, new Integer[] {AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDatalistPopup() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_with_datalist.html");
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('text2').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        pollDatalistPopupShown(2);
        TouchCommon.singleClickView(
                mAutofillProvider.getDatalistPopupForTesting().getListView().getChildAt(1));
        // Verify the selection accepted by renderer.
        pollJavascriptResult("document.getElementById('text2').value;", "\"A2\"");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHideDatalistPopup() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_with_datalist.html");
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('text2').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        pollDatalistPopupShown(2);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mAwContents.hideAutofillPopup(); });
        assertNull(mAutofillProvider.getDatalistPopupForTesting());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testVisibility() throws Throwable {
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<input type='text' id='text1' name='username'>"
                + "<input type='text' name='email' id='text2' style='display: none;'/>"
                + "</form></body></html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        // Verifies the visibility set correctly.
        assertEquals(View.VISIBLE, viewStructure.getChild(0).getVisibility());
        assertEquals(View.INVISIBLE, viewStructure.getChild(1).getVisibility());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testServerPredictionArrivesBeforeAutofillStart() throws Throwable {
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<input type='text' id='text1' name='username'>"
                + "<input type='text' name='email' id='text2' autocomplete='email'/>"
                + "</form></body></html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillProviderTestHelper
                                   .simulateMainFramePredictionsAutofillServerResponseForTesting(
                                           mAwContents.getWebContents(),
                                           new String[] {"text1", "text2"},
                                           new int[][] {{/*USERNAME, EMAIL_ADDRESS*/ 86, 9},
                                                   {/*EMAIL_ADDRESS*/ 9}}));

        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        assertEquals("USERNAME",
                viewStructure.getChild(0).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("USERNAME",
                viewStructure.getChild(0).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals("USERNAME,EMAIL_ADDRESS",
                viewStructure.getChild(0).getHtmlInfo().getAttribute(
                        "crowdsourcing-predictions-autofill-hints"));
        assertEquals("EMAIL_ADDRESS",
                viewStructure.getChild(1).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("HTML_TYPE_EMAIL",
                viewStructure.getChild(1).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals("EMAIL_ADDRESS",
                viewStructure.getChild(1).getHtmlInfo().getAttribute(
                        "crowdsourcing-predictions-autofill-hints"));
        // Binder will not be set if the prediction already arrives.
        IBinder binder = viewStructure.getExtras().getBinder("AUTOFILL_HINTS_SERVICE");
        assertNull(binder);
    }

    /**
     * Tests that server predictions are mapped to the fields of a cross-frame form.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=AutofillAcrossIframes"})
    public void testCrossFrameServerPredictionArrivesBeforeAutofillStart() throws Throwable {
        final String data = "<html><head></head><body><form>"
                + "<input id=name>"
                + "<iframe srcdoc='<form action=arbitrary.html method=GET>"
                + "                <input id=num autocomplete=cc-number></form>'"
                + "        sandbox></iframe>"
                + "<iframe srcdoc='<input id=exp>'></iframe>"
                + "<iframe srcdoc='<input id=csc>'></iframe>"
                + "</form></body></html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillProviderTestHelper
                                   .simulateMainFramePredictionsAutofillServerResponseForTesting(
                                           mAwContents.getWebContents(),
                                           new String[] {"name", "num", "exp", "csc"},
                                           new int[][] {{/*CREDIT_CARD_NAME_FULL*/ 51},
                                                   {/*CREDIT_CARD_NUMBER*/ 52},
                                                   {/*CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR*/ 56,
                                                           /*CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR*/
                                                           57},
                                                   {/*CREDIT_CARD_VERIFICATION_CODE*/ 59}}));

        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.forms[0].elements[0].select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(4, viewStructure.getChildCount());
        // Name field.
        assertEquals("CREDIT_CARD_NAME_FULL",
                viewStructure.getChild(0).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("CREDIT_CARD_NAME_FULL",
                viewStructure.getChild(0).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals("CREDIT_CARD_NAME_FULL",
                viewStructure.getChild(0).getHtmlInfo().getAttribute(
                        "crowdsourcing-predictions-autofill-hints"));
        // Number field.
        assertEquals("CREDIT_CARD_NUMBER",
                viewStructure.getChild(1).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("HTML_TYPE_CREDIT_CARD_NUMBER",
                viewStructure.getChild(1).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals("CREDIT_CARD_NUMBER",
                viewStructure.getChild(1).getHtmlInfo().getAttribute(
                        "crowdsourcing-predictions-autofill-hints"));
        // Expiration date field.
        assertEquals("CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR",
                viewStructure.getChild(2).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR",
                viewStructure.getChild(2).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals("CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR",
                viewStructure.getChild(2).getHtmlInfo().getAttribute(
                        "crowdsourcing-predictions-autofill-hints"));
        // CVC field.
        assertEquals("CREDIT_CARD_VERIFICATION_CODE",
                viewStructure.getChild(3).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("CREDIT_CARD_VERIFICATION_CODE",
                viewStructure.getChild(3).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals("CREDIT_CARD_VERIFICATION_CODE",
                viewStructure.getChild(3).getHtmlInfo().getAttribute(
                        "crowdsourcing-predictions-autofill-hints"));
        // Binder is not set if the prediction has already arrived.
        IBinder binder = viewStructure.getExtras().getBinder("AUTOFILL_HINTS_SERVICE");
        assertNull(binder);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testServerPredictionPrimaryTypeArrivesBeforeAutofillStart() throws Throwable {
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<input type='text' id='text1' name='username'>"
                + "<input type='text' name='email' id='text2' autocomplete='email'/>"
                + "</form></body></html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillProviderTestHelper
                                   .simulateMainFrameAutofillServerResponseForTesting(
                                           mAwContents.getWebContents(),
                                           new String[] {"text1", "text2"},
                                           new int[] {/*USERNAME, EMAIL_ADDRESS*/ 86, 9}));

        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        assertEquals("USERNAME",
                viewStructure.getChild(0).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("USERNAME",
                viewStructure.getChild(0).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals("USERNAME",
                viewStructure.getChild(0).getHtmlInfo().getAttribute(
                        "crowdsourcing-predictions-autofill-hints"));
        assertEquals("EMAIL_ADDRESS",
                viewStructure.getChild(1).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("HTML_TYPE_EMAIL",
                viewStructure.getChild(1).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals("EMAIL_ADDRESS",
                viewStructure.getChild(1).getHtmlInfo().getAttribute(
                        "crowdsourcing-predictions-autofill-hints"));
        // Binder will not be set if the prediction already arrives.
        IBinder binder = viewStructure.getExtras().getBinder("AUTOFILL_HINTS_SERVICE");
        assertNull(binder);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testServerPredictionArrivesAfterAutofillStart() throws Throwable {
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<input type='text' id='text1' name='username'>"
                + "<input type='text' name='email' id='text2' autocomplete='email'/>"
                + "</form></body></html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);

        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        assertEquals("NO_SERVER_DATA",
                viewStructure.getChild(0).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("UNKNOWN_TYPE",
                viewStructure.getChild(0).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(viewStructure.getChild(0).getHtmlInfo().getAttribute(
                "crowdsourcing-predictions-autofill-hints"));
        assertEquals("NO_SERVER_DATA",
                viewStructure.getChild(1).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("HTML_TYPE_EMAIL",
                viewStructure.getChild(1).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(viewStructure.getChild(1).getHtmlInfo().getAttribute(
                "crowdsourcing-predictions-autofill-hints"));

        IBinder binder = viewStructure.getExtras().getBinder("AUTOFILL_HINTS_SERVICE");
        assertNotNull(binder);
        AutofillHintsServiceTestHelper autofillHintsServiceTestHelper =
                new AutofillHintsServiceTestHelper();
        autofillHintsServiceTestHelper.registerViewTypeService(binder);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillProviderTestHelper
                                   .simulateMainFramePredictionsAutofillServerResponseForTesting(
                                           mAwContents.getWebContents(),
                                           new String[] {"text1", "text2"},
                                           new int[][] {{/*USERNAME, EMAIL_ADDRESS*/ 86, 9},
                                                   {/*EMAIL_ADDRESS*/ 9}}));

        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_QUERY_DONE});
        assertTrue(mTestAutofillManagerWrapper.isQuerySucceed());
        autofillHintsServiceTestHelper.waitForCallbackInvoked();
        List<ViewType> viewTypes = autofillHintsServiceTestHelper.getViewTypes();
        assertEquals(2, viewTypes.size());
        assertEquals(viewStructure.getChild(0).getAutofillId(), viewTypes.get(0).mAutofillId);
        assertEquals("USERNAME", viewTypes.get(0).mServerType);
        assertEquals("USERNAME", viewTypes.get(0).mComputedType);
        assertArrayEquals(new String[] {"USERNAME", "EMAIL_ADDRESS"},
                viewTypes.get(0).getServerPredictions());
        assertEquals(viewStructure.getChild(1).getAutofillId(), viewTypes.get(1).mAutofillId);
        assertEquals("EMAIL_ADDRESS", viewTypes.get(1).mServerType);
        assertEquals("HTML_TYPE_EMAIL", viewTypes.get(1).mComputedType);
        assertArrayEquals(new String[] {"EMAIL_ADDRESS"}, viewTypes.get(1).getServerPredictions());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testServerPredictionPrimaryTypeArrivesAfterAutofillStart() throws Throwable {
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<input type='text' id='text1' name='username'>"
                + "<input type='text' name='email' id='text2' autocomplete='email'/>"
                + "</form></body></html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);

        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        assertEquals("NO_SERVER_DATA",
                viewStructure.getChild(0).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("UNKNOWN_TYPE",
                viewStructure.getChild(0).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(viewStructure.getChild(0).getHtmlInfo().getAttribute(
                "crowdsourcing-predictions-autofill-hints"));
        assertEquals("NO_SERVER_DATA",
                viewStructure.getChild(1).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("HTML_TYPE_EMAIL",
                viewStructure.getChild(1).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(viewStructure.getChild(1).getHtmlInfo().getAttribute(
                "crowdsourcing-predictions-autofill-hints"));

        IBinder binder = viewStructure.getExtras().getBinder("AUTOFILL_HINTS_SERVICE");
        assertNotNull(binder);
        AutofillHintsServiceTestHelper autofillHintsServiceTestHelper =
                new AutofillHintsServiceTestHelper();
        autofillHintsServiceTestHelper.registerViewTypeService(binder);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillProviderTestHelper
                                   .simulateMainFrameAutofillServerResponseForTesting(
                                           mAwContents.getWebContents(),
                                           new String[] {"text1", "text2"},
                                           new int[] {/*USERNAME, EMAIL_ADDRESS*/ 86, 9}));

        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_QUERY_DONE});
        assertTrue(mTestAutofillManagerWrapper.isQuerySucceed());
        autofillHintsServiceTestHelper.waitForCallbackInvoked();
        List<ViewType> viewTypes = autofillHintsServiceTestHelper.getViewTypes();
        assertEquals(2, viewTypes.size());
        assertEquals(viewStructure.getChild(0).getAutofillId(), viewTypes.get(0).mAutofillId);
        assertEquals("USERNAME", viewTypes.get(0).mServerType);
        assertEquals("USERNAME", viewTypes.get(0).mComputedType);
        assertArrayEquals(new String[] {"USERNAME"}, viewTypes.get(0).getServerPredictions());
        assertEquals(viewStructure.getChild(1).getAutofillId(), viewTypes.get(1).mAutofillId);
        assertEquals("EMAIL_ADDRESS", viewTypes.get(1).mServerType);
        assertEquals("HTML_TYPE_EMAIL", viewTypes.get(1).mComputedType);
        assertArrayEquals(new String[] {"EMAIL_ADDRESS"}, viewTypes.get(1).getServerPredictions());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testServerPredictionArrivesBeforeCallbackRegistered() throws Throwable {
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<input type='text' id='text1' name='username'>"
                + "<input type='text' name='email' id='text2' autocomplete='email'/>"
                + "</form></body></html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);

        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        assertEquals("NO_SERVER_DATA",
                viewStructure.getChild(0).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("UNKNOWN_TYPE",
                viewStructure.getChild(0).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(viewStructure.getChild(0).getHtmlInfo().getAttribute(
                "crowdsourcing-predictions-autofill-hints"));
        assertEquals("NO_SERVER_DATA",
                viewStructure.getChild(1).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("HTML_TYPE_EMAIL",
                viewStructure.getChild(1).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(viewStructure.getChild(1).getHtmlInfo().getAttribute(
                "crowdsourcing-predictions-autofill-hints"));

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillProviderTestHelper
                                   .simulateMainFramePredictionsAutofillServerResponseForTesting(
                                           mAwContents.getWebContents(),
                                           new String[] {"text1", "text2"},
                                           new int[][] {{/*USERNAME, EMAIL_ADDRESS*/ 86, 9},
                                                   {/*EMAIL_ADDRESS*/ 9}}));

        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_QUERY_DONE});
        assertTrue(mTestAutofillManagerWrapper.isQuerySucceed());

        IBinder binder = viewStructure.getExtras().getBinder("AUTOFILL_HINTS_SERVICE");
        assertNotNull(binder);
        AutofillHintsServiceTestHelper autofillHintsServiceTestHelper =
                new AutofillHintsServiceTestHelper();
        autofillHintsServiceTestHelper.registerViewTypeService(binder);
        autofillHintsServiceTestHelper.waitForCallbackInvoked();
        List<ViewType> viewTypes = autofillHintsServiceTestHelper.getViewTypes();
        assertEquals(2, viewTypes.size());
        assertEquals(viewStructure.getChild(0).getAutofillId(), viewTypes.get(0).mAutofillId);
        assertEquals("USERNAME", viewTypes.get(0).mServerType);
        assertEquals("USERNAME", viewTypes.get(0).mComputedType);
        assertArrayEquals(new String[] {"USERNAME", "EMAIL_ADDRESS"},
                viewTypes.get(0).getServerPredictions());
        assertEquals(viewStructure.getChild(1).getAutofillId(), viewTypes.get(1).mAutofillId);
        assertEquals("EMAIL_ADDRESS", viewTypes.get(1).mServerType);
        assertEquals("HTML_TYPE_EMAIL", viewTypes.get(1).mComputedType);
        assertArrayEquals(new String[] {"EMAIL_ADDRESS"}, viewTypes.get(1).getServerPredictions());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testServerQueryFailedAfterAutofillStart() throws Throwable {
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<input type='text' id='text1' name='username'>"
                + "<input type='text' name='email' id='text2' autocomplete='email'/>"
                + "</form></body></html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);

        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        assertEquals("NO_SERVER_DATA",
                viewStructure.getChild(0).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("UNKNOWN_TYPE",
                viewStructure.getChild(0).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(viewStructure.getChild(0).getHtmlInfo().getAttribute(
                "crowdsourcing-predictions-autofill-hints"));
        assertEquals("NO_SERVER_DATA",
                viewStructure.getChild(1).getHtmlInfo().getAttribute(
                        "crowdsourcing-autofill-hints"));
        assertEquals("HTML_TYPE_EMAIL",
                viewStructure.getChild(1).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(viewStructure.getChild(1).getHtmlInfo().getAttribute(
                "crowdsourcing-predictions-autofill-hints"));
        IBinder binder = viewStructure.getExtras().getBinder("AUTOFILL_HINTS_SERVICE");
        assertNotNull(binder);
        AutofillHintsServiceTestHelper autofillHintsServiceTestHelper =
                new AutofillHintsServiceTestHelper();
        autofillHintsServiceTestHelper.registerViewTypeService(binder);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillProviderTestHelper
                                   .simulateMainFrameAutofillQueryFailedForTesting(
                                           mAwContents.getWebContents()));

        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_QUERY_DONE});
        assertFalse(mTestAutofillManagerWrapper.isQuerySucceed());

        autofillHintsServiceTestHelper.waitForCallbackInvoked();
        assertTrue(autofillHintsServiceTestHelper.isQueryFailed());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFieldAddedBeforeSuggestionSelected() throws Throwable {
        // This test verifies that form filling works even in the case that the form has been
        // modified (field was added) in the DOM between the decision to fill and executing the
        // fill.
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<label>User Name:</label>"
                + "<input type='text' id='text1' name='name'/>"
                + "<label>Password:</label>"
                + "<input type='password' id='pwdid' name='pwd'/>"
                + "</form></body></html>";
        int cnt = 0;
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());

        // Append a field.
        executeJavaScriptAndWaitForResult("document.getElementById('pwdid').insertAdjacentHTML("
                + "'afterend', '<input type=\"password\" id=\"pwdid2\"/>');");

        // Autofill the original form.
        SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
        values.append(
                viewStructure.getChild(0).getId(), AutofillValue.forText("example@example.com"));
        values.append(viewStructure.getChild(1).getId(), AutofillValue.forText("password"));
        cnt = getCallbackCount();
        clearChangedValues();
        invokeAutofill(values);
        waitForCallbackAndVerifyTypes(
                cnt, new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED});

        String value0 =
                executeJavaScriptAndWaitForResult("document.getElementById('text1').value;");
        assertEquals("\"example@example.com\"", value0);
        String value1 =
                executeJavaScriptAndWaitForResult("document.getElementById('pwdid').value;");
        assertEquals("\"password\"", value1);
        String value2 =
                executeJavaScriptAndWaitForResult("document.getElementById('pwdid2').value;");
        assertEquals("\"\"", value2);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFirstFieldRemovedBeforeSuggestionSelected() throws Throwable {
        // This test verifies that form filling works even if an element of the form that was
        // supposed to be filled has been deleted between the time of decision to fill the form and
        // executing the fill.
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<label>User Name:</label>"
                + "<input type='text' id='text1' name='name'/>"
                + "<label>Password:</label>"
                + "<input type='password' id='pwdid' name='pwd'/>"
                + "</form></body></html>";
        int cnt = 0;
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        // Focus on the second element, since the first one is about to be removed. Removing the
        // element on which the fill was triggered would cancel the filling operation.
        executeJavaScriptAndWaitForResult("document.getElementById('pwdid').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());

        // Remove the first field.
        executeJavaScriptAndWaitForResult("document.getElementById('text1').remove()");

        // Autofill the original form.
        SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
        values.append(
                viewStructure.getChild(0).getId(), AutofillValue.forText("example@example.com"));
        values.append(viewStructure.getChild(1).getId(), AutofillValue.forText("password"));
        cnt = getCallbackCount();
        clearChangedValues();
        invokeAutofill(values);
        waitForCallbackAndVerifyTypes(
                cnt, new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED});

        String value1 =
                executeJavaScriptAndWaitForResult("document.getElementById('pwdid').value;");
        assertEquals("\"password\"", value1);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFrameDetachedOnFormSubmission() throws Throwable {
        final String mainFrame = "<html><body>"
                + "<script>"
                + "function receiveMessage(event) {"
                + "  var address_iframe = document.getElementById('address_iframe');"
                + "  address_iframe.parentNode.removeChild(address_iframe);"
                + "  setTimeout(delayedUpload, 0);"
                + "}"
                + "window.addEventListener('message', receiveMessage, false);"
                + "</script>"
                + "<iframe src='inner_frame_address_form.html' id='address_iframe'"
                + "    name='address_iframe'>"
                + "</iframe>"
                + "</body></html>";
        final String url = mWebServer.setResponse(FILE, mainFrame, null);
        final String subFrame = "<html><body>"
                + "<script>"
                + "function send_post() {"
                + "  window.parent.postMessage('SubmitComplete', '*');"
                + "}"
                + "</script>"
                + "<form action='inner_frame_address_form.html' id='deleting_form'"
                + "    onsubmit='send_post(); return false;'>"
                + "  <input type='text' id='address_field' name='address' autocomplete='on'>"
                + "   <input type='submit' id='submit_button' name='submit_button'>"
                + "</form>"
                + "</body></html>";
        final String subFrameURL =
                mWebServer.setResponse("/inner_frame_address_form.html", subFrame, null);
        assertTrue(Uri.parse(subFrameURL).getPath().equals("/inner_frame_address_form.html"));
        int cnt = 0;
        loadUrlSync(url);
        pollJavascriptResult("var iframe = document.getElementById('address_iframe');"
                        + "var frame_doc = iframe.contentDocument;"
                        + "frame_doc.getElementById('address_field').focus();"
                        + "frame_doc.activeElement.id;",
                "\"address_field\"");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        executeJavaScriptAndWaitForResult("var iframe = document.getElementById('address_iframe');"
                + "var frame_doc = iframe.contentDocument;"
                + "frame_doc.getElementById('submit_button').click();");
        waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT});
        assertEquals(SubmissionSource.FORM_SUBMISSION, mSubmissionSource);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFrameDetachedOnFormlessSubmission() throws Throwable {
        final String mainFrame = "<html><body>"
                + "<script>"
                + "function receiveMessage(event) {"
                + "  var address_iframe = document.getElementById('address_iframe');"
                + "  address_iframe.parentNode.removeChild(address_iframe);"
                + "}"
                + "window.addEventListener('message', receiveMessage, false);"
                + "</script>"
                + "<iframe src='inner_frame_address_formless.html' id='address_iframe'"
                + "    name='address_iframe'>"
                + "</iframe>"
                + "</body></html>";
        final String url = mWebServer.setResponse(FILE, mainFrame, null);
        final String subFrame = "<html><body>"
                + "<script>"
                + "function send_post() {"
                + "  window.parent.postMessage('SubmitComplete', '*');"
                + "}"
                + "</script>"
                + "<input type='text' id='address_field' name='address' autocomplete='on'>"
                + "<input type='button' id='submit_button' name='submit_button'"
                + "    onclick='send_post()'>"
                + "</body></html>";
        final String subFrameURL =
                mWebServer.setResponse("/inner_frame_address_formless.html", subFrame, null);
        assertTrue(Uri.parse(subFrameURL).getPath().equals("/inner_frame_address_formless.html"));
        int cnt = 0;
        loadUrlSync(url);
        pollJavascriptResult("var iframe = document.getElementById('address_iframe');"
                        + "var frame_doc = iframe.contentDocument;"
                        + "frame_doc.getElementById('address_field').focus();"
                        + "frame_doc.activeElement.id;",
                "\"address_field\"");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        executeJavaScriptAndWaitForResult("var iframe = document.getElementById('address_iframe');"
                + "var frame_doc = iframe.contentDocument;"
                + "frame_doc.getElementById('submit_button').click();");
        // The additional AUTOFILL_VIEW_EXITED event caused by 'click' of the button.
        waitForCallbackAndVerifyTypes(
                cnt, new Integer[] {AUTOFILL_VIEW_EXITED, AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT});
        assertEquals(SubmissionSource.FRAME_DETACHED, mSubmissionSource);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLabelChange() throws Throwable {
        final String data = "<html><head></head><body>"
                + "<form action='a.html'>"
                + "<label id='label_id'> Address </label>"
                + "<input type='text' id='address' name='address' autocomplete='on'/>"
                + "<p id='p_id'>Address 1</p>"
                + "<input type='text' name='address1' autocomplete='on'/>"
                + "<input type='submit' id='submit_button' name='submit_button'/>"
                + "</form>"
                + "</body></html>";
        int cnt = 0;
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('address').focus();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED,
                        AUTOFILL_VALUE_CHANGED});
        // Verify label change shall trigger new session.
        executeJavaScriptAndWaitForResult(
                "document.getElementById('label_id').innerHTML='address change';");
        executeJavaScriptAndWaitForResult("document.getElementById('address').focus();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        cnt += waitForCallbackAndVerifyTypes(cnt,
                new Integer[] {AUTOFILL_CANCEL, AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED,
                        AUTOFILL_SESSION_STARTED, AUTOFILL_VALUE_CHANGED});
        // Verify inferred label change won't trigger new session.
        executeJavaScriptAndWaitForResult(
                "document.getElementById('p_id').innerHTML='address change';");
        executeJavaScriptAndWaitForResult("document.getElementById('address').focus();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
    }

    private void pollJavascriptResult(String script, String expectedResult) throws Throwable {
        AwActivityTestRule.pollInstrumentationThread(() -> {
            try {
                return expectedResult.equals(executeJavaScriptAndWaitForResult(script));
            } catch (Throwable e) {
                return false;
            }
        });
    }

    private void pollJavascriptResultNotEqualTo(String script, String result) throws Throwable {
        AwActivityTestRule.pollInstrumentationThread(() -> {
            try {
                return !result.equals(executeJavaScriptAndWaitForResult(script));
            } catch (Throwable e) {
                return false;
            }
        });
    }

    private void pollDatalistPopupShown(int expectedTotalChildren) {
        AwActivityTestRule.pollInstrumentationThread(() -> {
            AutofillPopup popup = mAutofillProvider.getDatalistPopupForTesting();
            boolean isShown = popup != null && popup.getListView() != null
                    && popup.getListView().getChildCount() == expectedTotalChildren;
            for (int i = 0; i < expectedTotalChildren && isShown; i++) {
                isShown = popup.getListView().getChildAt(i).getWidth() > 0
                        && popup.getListView().getChildAt(i).isAttachedToWindow();
            }
            return isShown;
        });
    }

    private void scrollToBottom() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTestContainerView.scrollTo(0, mTestContainerView.getHeight()); });
    }

    private void loadUrlSync(String url) throws Exception {
        CallbackHelper done = mContentsClient.getOnPageCommitVisibleHelper();
        int callCount = done.getCallCount();
        mRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);
        done.waitForCallback(callCount);
    }

    private void reloadSync() throws Exception {
        CallbackHelper done = mContentsClient.getOnPageCommitVisibleHelper();
        int callCount = done.getCallCount();
        mRule.reloadSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper());
        done.waitForCallback(callCount);
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTestAutofillManagerWrapper.notifyInputUIChange());
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
            Assert.assertArrayEquals("Expect: " + buildEventList(adjustedEventArray)
                            + " Result: " + buildEventList(resultArray),
                    adjustedEventArray, resultArray);
            return adjustedEventArray.length;
        } catch (TimeoutException e) {
            Object[] objectArray = mEventQueue.toArray();
            Integer[] resultArray = Arrays.copyOf(objectArray, objectArray.length, Integer[].class);
            Assert.assertArrayEquals("Expect:" + buildEventList(adjustedEventArray)
                            + " Result:" + buildEventList(resultArray),
                    adjustedEventArray, resultArray);
            throw e;
        }
    }

    /**
     * Consumes all observed events from {@link mEventQueue} until the
     * {@code expectedEvents} have been observed (in proper order). Calls
     * {@code mCallbackHelper.waitForNext();} in case the {@link mEventQueue}
     * runs out of events. Unexpected events are just ignored.
     *
     * @param expectedEvents the events that need to happen.
     * @return Whether the {@code expectedEvents} were observed.
     * @throws TimeoutException
     */
    private boolean waitForEvents(Integer[] expectedEvents) throws TimeoutException {
        // Chosen arbitrarily.
        final int maxCallsToWaitFor = 20;
        int numCallsToWaitFor = 0;

        LinkedList<Integer> expectedEventsQueue =
                new LinkedList<Integer>(Arrays.asList(expectedEvents));

        while (!expectedEventsQueue.isEmpty() && numCallsToWaitFor < maxCallsToWaitFor) {
            if (mEventQueue.isEmpty()) {
                // Wait for new events.
                ++numCallsToWaitFor;
                mCallbackHelper.waitForNext();
                continue;
            }

            int nextExpectedEvent = expectedEventsQueue.peek();
            // Actually consumes the event.
            int nextObservedEvent = mEventQueue.poll();
            if (nextExpectedEvent == nextObservedEvent) {
                expectedEventsQueue.poll();
            }
        }
        return expectedEventsQueue.isEmpty();
    }

    private static String buildEventList(Integer[] eventArray) {
        Assert.assertEquals(EVENT.length, AUTOFILL_EVENT_MAX);
        List<String> result = new ArrayList<String>(eventArray.length);
        for (Integer event : eventArray) result.add(EVENT[event]);
        return TextUtils.join(",", result);
    }

    private void dispatchDownAndUpKeyEvents(final int code) throws Throwable {
        long eventTime = SystemClock.uptimeMillis();
        dispatchKeyEvent(new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_DOWN, code, 0));
        dispatchKeyEvent(new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_UP, code, 0));
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

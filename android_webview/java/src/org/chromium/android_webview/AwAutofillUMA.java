// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;

import org.chromium.autofill.mojom.SubmissionSource;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.util.concurrent.TimeUnit;

/**
 * The class for WebView autofill UMA.
 */
public class AwAutofillUMA {
    // Records whether the Autofill service is enabled or not.
    public static final String UMA_AUTOFILL_WEBVIEW_ENABLED = "Autofill.WebView.Enabled";

    // Records whether the Autofill provider is created by activity context or not.
    public static final String UMA_AUTOFILL_WEBVIEW_CREATED_BY_ACTIVITY_CONTEXT =
            "Autofill.WebView.CreatedByActivityContext";

    // Records what happened in an autofill session.
    public static final String UMA_AUTOFILL_WEBVIEW_AUTOFILL_SESSION =
            "Autofill.WebView.AutofillSession";
    // The possible value of UMA_AUTOFILL_WEBVIEW_AUTOFILL_SESSION.
    public static final int SESSION_UNKNOWN = 0;
    public static final int NO_CALLBACK_FORM_FRAMEWORK = 1;
    public static final int NO_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED = 2;
    public static final int NO_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED = 3;
    public static final int NO_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED = 4;
    public static final int NO_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED = 5;
    public static final int USER_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED = 6;
    public static final int USER_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED = 7;
    public static final int USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED = 8;
    public static final int USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED = 9;
    public static final int USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED = 10;
    public static final int USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED = 11;
    public static final int USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED = 12;
    public static final int USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED = 13;
    public static final int AUTOFILL_SESSION_HISTOGRAM_COUNT = 14;

    // Records whether user changed autofilled field if user ever changed the form. The action isn't
    // recorded if user didn't change form at all.
    public static final String UMA_AUTOFILL_USER_CHANGED_AUTOFILLED_FIELD =
            "Autofill.WebView.UserChangedAutofilledField";

    public static final String UMA_AUTOFILL_WEBVIEW_SUBMISSION_SOURCE =
            "Autofill.WebView.SubmissionSource";
    // The possible value of UMA_AUTOFILL_WEBVIEW_SUBMISSION_SOURCE.
    public static final int SAME_DOCUMENT_NAVIGATION = 0;
    public static final int XHR_SUCCEEDED = 1;
    public static final int FRAME_DETACHED = 2;
    public static final int DOM_MUTATION_AFTER_XHR = 3;
    public static final int PROBABLY_FORM_SUBMITTED = 4;
    public static final int FORM_SUBMISSION = 5;
    public static final int SUBMISSION_SOURCE_HISTOGRAM_COUNT = 6;

    // The million seconds from user touched the field to WebView started autofill session.
    public static final String UMA_AUTOFILL_WEBVIEW_TRIGGERING_TIME =
            "Autofill.WebView.TriggeringTime";

    // The million seconds from WebView started autofill session to suggestion was displayed.
    public static final String UMA_AUTOFILL_WEBVIEW_SUGGESTION_TIME =
            "Autofill.WebView.SuggestionTime";

    // The expected time range of time is from 10ms to 2 seconds, and 50 buckets is sufficient.
    private static final long MIN_TIME_MILLIS = 10;
    private static final long MAX_TIME_MILLIS = TimeUnit.SECONDS.toMillis(2);
    private static final int NUM_OF_BUCKETS = 50;

    private static void recordTimesHistogram(String name, long durationMillis) {
        RecordHistogram.recordCustomTimesHistogram(
                name, durationMillis, MIN_TIME_MILLIS, MAX_TIME_MILLIS, NUM_OF_BUCKETS);
    }

    private static class SessionRecorder {
        public static final int EVENT_VIRTUAL_STRUCTURE_PROVIDED = 0x1 << 0;
        public static final int EVENT_SUGGESTION_DISPLAYED = 0x1 << 1;
        public static final int EVENT_FORM_AUTOFILLED = 0x1 << 2;
        public static final int EVENT_USER_CHANGED_FIELD_VALUE = 0x1 << 3;
        public static final int EVENT_FORM_SUBMITTED = 0x1 << 4;
        public static final int EVENT_USER_CHANGED_AUTOFILLED_FIELD = 0x1 << 5;

        private Long mSuggestionTimeMillis;

        public void record(int event) {
            // Not record any event until we get EVENT_VIRTUAL_STRUCTURE_PROVIDED which makes the
            // following events meaningful.
            if (event != EVENT_VIRTUAL_STRUCTURE_PROVIDED && mState == 0) return;
            if (EVENT_USER_CHANGED_FIELD_VALUE == event && mUserChangedAutofilledField == null) {
                mUserChangedAutofilledField = Boolean.valueOf(false);
            } else if (EVENT_USER_CHANGED_AUTOFILLED_FIELD == event) {
                if (mUserChangedAutofilledField == null) {
                    mUserChangedAutofilledField = Boolean.valueOf(true);
                }
                mUserChangedAutofilledField = true;
                event = EVENT_USER_CHANGED_FIELD_VALUE;
            }
            mState |= event;
        }

        public void setSuggestionTimeMillis(long suggestionTimeMillis) {
            // Only record first suggestion.
            if (mSuggestionTimeMillis == null) {
                mSuggestionTimeMillis = Long.valueOf(suggestionTimeMillis);
            }
        }

        public void recordHistogram() {
            RecordHistogram.recordEnumeratedHistogram(UMA_AUTOFILL_WEBVIEW_AUTOFILL_SESSION,
                    toUMAAutofillSessionValue(), AUTOFILL_SESSION_HISTOGRAM_COUNT);
            // Only record if user ever changed form.
            if (mUserChangedAutofilledField != null) {
                RecordHistogram.recordBooleanHistogram(
                        UMA_AUTOFILL_USER_CHANGED_AUTOFILLED_FIELD, mUserChangedAutofilledField);
            }
            if (mSuggestionTimeMillis != null) {
                recordTimesHistogram(UMA_AUTOFILL_WEBVIEW_SUGGESTION_TIME, mSuggestionTimeMillis);
            }
        }

        private int toUMAAutofillSessionValue() {
            if (mState == 0) {
                return NO_CALLBACK_FORM_FRAMEWORK;
            } else if (mState == EVENT_VIRTUAL_STRUCTURE_PROVIDED) {
                return NO_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_USER_CHANGED_FIELD_VALUE)) {
                return NO_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED;
            } else if (mState == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_FORM_SUBMITTED)) {
                return NO_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_USER_CHANGED_FIELD_VALUE
                            | EVENT_FORM_SUBMITTED)) {
                return NO_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_FORM_AUTOFILLED)) {
                return USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_FORM_AUTOFILLED | EVENT_FORM_SUBMITTED)) {
                return USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_FORM_AUTOFILLED | EVENT_USER_CHANGED_FIELD_VALUE
                            | EVENT_FORM_SUBMITTED)) {
                return USER_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_FORM_AUTOFILLED | EVENT_USER_CHANGED_FIELD_VALUE)) {
                return USER_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED;
            } else if (mState == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED)) {
                return USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_FORM_SUBMITTED)) {
                return USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_USER_CHANGED_FIELD_VALUE | EVENT_FORM_SUBMITTED)) {
                return USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_USER_CHANGED_FIELD_VALUE)) {
                return USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED;
            } else {
                return SESSION_UNKNOWN;
            }
        }

        private int mState;
        private Boolean mUserChangedAutofilledField;
    }

    private SessionRecorder mRecorder;
    private Boolean mAutofillDisabled;

    public AwAutofillUMA(Context context) {
        RecordHistogram.recordBooleanHistogram(UMA_AUTOFILL_WEBVIEW_CREATED_BY_ACTIVITY_CONTEXT,
                ContextUtils.activityFromContext(context) != null);
    }

    public void onFormSubmitted(int submissionSource) {
        if (mRecorder != null) mRecorder.record(SessionRecorder.EVENT_FORM_SUBMITTED);
        recordSession();
        // We record this no matter autofill service is disabled or not.
        RecordHistogram.recordEnumeratedHistogram(UMA_AUTOFILL_WEBVIEW_SUBMISSION_SOURCE,
                toUMASubmissionSource(submissionSource), SUBMISSION_SOURCE_HISTOGRAM_COUNT);
    }

    public void onSessionStarted(boolean autofillDisabled) {
        // Record autofill status once per instance and only if user triggers the autofill.
        if (mAutofillDisabled == null || mAutofillDisabled.booleanValue() != autofillDisabled) {
            RecordHistogram.recordBooleanHistogram(UMA_AUTOFILL_WEBVIEW_ENABLED, !autofillDisabled);
            mAutofillDisabled = Boolean.valueOf(autofillDisabled);
        }

        if (mRecorder != null) recordSession();
        mRecorder = new SessionRecorder();
    }

    public void onVirtualStructureProvided() {
        if (mRecorder != null) mRecorder.record(SessionRecorder.EVENT_VIRTUAL_STRUCTURE_PROVIDED);
    }

    public void onSuggestionDisplayed(long suggestionTimeMillis) {
        if (mRecorder != null) {
            mRecorder.record(SessionRecorder.EVENT_SUGGESTION_DISPLAYED);
            mRecorder.setSuggestionTimeMillis(suggestionTimeMillis);
        }
    }

    public void onAutofill() {
        if (mRecorder != null) mRecorder.record(SessionRecorder.EVENT_FORM_AUTOFILLED);
    }

    public void onUserChangeFieldValue(boolean isPreviouslyAutofilled) {
        if (mRecorder == null) return;
        if (isPreviouslyAutofilled) {
            mRecorder.record(SessionRecorder.EVENT_USER_CHANGED_AUTOFILLED_FIELD);
        } else {
            mRecorder.record(SessionRecorder.EVENT_USER_CHANGED_FIELD_VALUE);
        }
    }

    private void recordSession() {
        if (mAutofillDisabled != null && !mAutofillDisabled.booleanValue() && mRecorder != null) {
            mRecorder.recordHistogram();
        }
        mRecorder = null;
    }

    private int toUMASubmissionSource(int source) {
        switch (source) {
            case SubmissionSource.SAME_DOCUMENT_NAVIGATION:
                return SAME_DOCUMENT_NAVIGATION;
            case SubmissionSource.XHR_SUCCEEDED:
                return XHR_SUCCEEDED;
            case SubmissionSource.FRAME_DETACHED:
                return FRAME_DETACHED;
            case SubmissionSource.DOM_MUTATION_AFTER_XHR:
                return DOM_MUTATION_AFTER_XHR;
            case SubmissionSource.PROBABLY_FORM_SUBMITTED:
                return PROBABLY_FORM_SUBMITTED;
            case SubmissionSource.FORM_SUBMISSION:
                return FORM_SUBMISSION;
            default:
                return SUBMISSION_SOURCE_HISTOGRAM_COUNT;
        }
    }
}

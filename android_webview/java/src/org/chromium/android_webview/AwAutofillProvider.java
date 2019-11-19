// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.DoNotInline;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.autofill.FormData;
import org.chromium.components.autofill.FormFieldData;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

/**
 * This class uses Android autofill service to fill web form. All methods are
 * supposed to be called in UI thread.
 *
 * This class doesn't have 1:1 mapping to native AutofillProviderAndroid and is
 * same as how AwContents.java mapping to native AwContents, AwAutofillProvider
 * is owned by AwContents.java and AutofillProviderAndroid is owned by native
 * AwContents.
 *
 * DoNotInline since it causes class verification errors, see crbug.com/991851.
 */
@DoNotInline
@TargetApi(Build.VERSION_CODES.O)
public class AwAutofillProvider extends AutofillProvider {
    private static class FocusField {
        public final short fieldIndex;
        public final Rect absBound;

        public FocusField(short fieldIndex, Rect absBound) {
            this.fieldIndex = fieldIndex;
            this.absBound = absBound;
        }
    }
    /**
     * The class to wrap the request to framework.
     *
     * Though framework guarantees always giving us the autofill value of current
     * session, we still want to verify this by using unique virtual id which is
     * composed of sessionId and form field index, we don't use the request id
     * which comes from renderer as session id because it is not unique.
     */
    private static class AutofillRequest {
        private static final int INIT_ID = 1; // ID can't be 0 in Android.
        private static int sSessionId = INIT_ID;
        public final int sessionId;
        private FormData mFormData;
        private FocusField mFocusField;

        public AutofillRequest(FormData formData, FocusField focus) {
            sessionId = getNextClientId();
            mFormData = formData;
            mFocusField = focus;
        }

        public void fillViewStructure(ViewStructure structure) {
            structure.setWebDomain(mFormData.mHost);
            structure.setHtmlInfo(structure.newHtmlInfoBuilder("form")
                                          .addAttribute("name", mFormData.mName)
                                          .build());
            int index = structure.addChildCount(mFormData.mFields.size());
            short fieldIndex = 0;
            for (FormFieldData field : mFormData.mFields) {
                ViewStructure child = structure.newChild(index++);
                int virtualId = toVirtualId(sessionId, fieldIndex++);
                child.setAutofillId(structure.getAutofillId(), virtualId);
                if (field.mAutocompleteAttr != null && !field.mAutocompleteAttr.isEmpty()) {
                    child.setAutofillHints(field.mAutocompleteAttr.split(" +"));
                }
                child.setHint(field.mPlaceholder);
                ViewStructure.HtmlInfo.Builder builder =
                        child.newHtmlInfoBuilder("input")
                                .addAttribute("name", field.mName)
                                .addAttribute("type", field.mType)
                                .addAttribute("label", field.mLabel)
                                .addAttribute("ua-autofill-hints", field.mHeuristicType)
                                .addAttribute("id", field.mId);

                switch (field.getControlType()) {
                    case FormFieldData.ControlType.LIST:
                        child.setAutofillType(View.AUTOFILL_TYPE_LIST);
                        child.setAutofillOptions(field.mOptionContents);
                        int i = findIndex(field.mOptionValues, field.getValue());
                        if (i != -1) {
                            child.setAutofillValue(AutofillValue.forList(i));
                        }
                        break;
                    case FormFieldData.ControlType.TOGGLE:
                        child.setAutofillType(View.AUTOFILL_TYPE_TOGGLE);
                        child.setAutofillValue(AutofillValue.forToggle(field.isChecked()));
                        break;
                    case FormFieldData.ControlType.TEXT:
                        child.setAutofillType(View.AUTOFILL_TYPE_TEXT);
                        child.setAutofillValue(AutofillValue.forText(field.getValue()));
                        if (field.mMaxLength != 0) {
                            builder.addAttribute("maxlength", String.valueOf(field.mMaxLength));
                        }
                        break;
                    default:
                        break;
                }
                child.setHtmlInfo(builder.build());
            }
        }

        public boolean autofill(final SparseArray<AutofillValue> values) {
            for (int i = 0; i < values.size(); ++i) {
                int id = values.keyAt(i);
                if (toSessionId(id) != sessionId) return false;
                AutofillValue value = values.get(id);
                if (value == null) continue;
                short index = toIndex(id);
                if (index < 0 || index >= mFormData.mFields.size()) return false;
                FormFieldData field = mFormData.mFields.get(index);
                if (field == null) return false;
                switch (field.getControlType()) {
                    case FormFieldData.ControlType.LIST:
                        int j = value.getListValue();
                        if (j < 0 && j >= field.mOptionValues.length) continue;
                        field.setAutofillValue(field.mOptionValues[j]);
                        break;
                    case FormFieldData.ControlType.TOGGLE:
                        field.setChecked(value.getToggleValue());
                        break;
                    case FormFieldData.ControlType.TEXT:
                        field.setAutofillValue((String) value.getTextValue());
                        break;
                    default:
                        break;
                }
            }
            return true;
        }

        public void setFocusField(FocusField focusField) {
            mFocusField = focusField;
        }

        public FocusField getFocusField() {
            return mFocusField;
        }

        public int getFieldCount() {
            return mFormData.mFields.size();
        }

        public AutofillValue getFieldNewValue(int index) {
            FormFieldData field = mFormData.mFields.get(index);
            if (field == null) return null;
            switch (field.getControlType()) {
                case FormFieldData.ControlType.LIST:
                    int i = findIndex(field.mOptionValues, field.getValue());
                    if (i == -1) return null;
                    return AutofillValue.forList(i);
                case FormFieldData.ControlType.TOGGLE:
                    return AutofillValue.forToggle(field.isChecked());
                case FormFieldData.ControlType.TEXT:
                    return AutofillValue.forText(field.getValue());
                default:
                    return null;
            }
        }

        public int getVirtualId(short index) {
            return toVirtualId(sessionId, index);
        }

        public FormFieldData getField(short index) {
            return mFormData.mFields.get(index);
        }

        private static int findIndex(String[] values, String value) {
            if (values != null && value != null) {
                for (int i = 0; i < values.length; i++) {
                    if (value.equals(values[i])) return i;
                }
            }
            return -1;
        }

        private static int getNextClientId() {
            ThreadUtils.assertOnUiThread();
            if (sSessionId == 0xffff) sSessionId = INIT_ID;
            return sSessionId++;
        }

        private static int toSessionId(int virtualId) {
            return (virtualId & 0xffff0000) >> 16;
        }

        private static short toIndex(int virtualId) {
            return (short) (virtualId & 0xffff);
        }

        private static int toVirtualId(int clientId, short index) {
            return (clientId << 16) | index;
        }
    }

    private AwAutofillManager mAutofillManager;
    private ViewGroup mContainerView;
    private WebContents mWebContents;

    private AutofillRequest mRequest;
    private long mNativeAutofillProvider;
    private AwAutofillUMA mAutofillUMA;
    private AwAutofillManager.InputUIObserver mInputUIObserver;
    private long mAutofillTriggeredTimeMillis;

    public AwAutofillProvider(Context context, ViewGroup containerView) {
        this(containerView, new AwAutofillManager(context), context);
    }

    @VisibleForTesting
    public AwAutofillProvider(ViewGroup containerView, AwAutofillManager manager, Context context) {
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped("AwAutofillProvider.constructor")) {
            assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
            mAutofillManager = manager;
            mContainerView = containerView;
            mAutofillUMA = new AwAutofillUMA(context);
            mInputUIObserver = new AwAutofillManager.InputUIObserver() {
                @Override
                public void onInputUIShown() {
                    // Not need to report suggestion window displayed if there is no live autofill
                    // session.
                    if (mRequest == null) return;
                    mAutofillUMA.onSuggestionDisplayed(
                            System.currentTimeMillis() - mAutofillTriggeredTimeMillis);
                }
            };
            mAutofillManager.addInputUIObserver(mInputUIObserver);
        }
    }

    @Override
    public void onContainerViewChanged(ViewGroup containerView) {
        mContainerView = containerView;
    }

    @Override
    public void onProvideAutoFillVirtualStructure(ViewStructure structure, int flags) {
        // This method could be called for the session started by the native
        // control outside of WebView, e.g. the URL bar, in this case, we simply
        // return.
        if (mRequest == null) return;
        mRequest.fillViewStructure(structure);
        if (AwAutofillManager.isLoggable()) {
            AwAutofillManager.log(
                    "onProvideAutoFillVirtualStructure fields:" + structure.getChildCount());
        }
        mAutofillUMA.onVirtualStructureProvided();
    }

    @Override
    public void autofill(final SparseArray<AutofillValue> values) {
        if (mNativeAutofillProvider != 0 && mRequest != null && mRequest.autofill((values))) {
            autofill(mNativeAutofillProvider, mRequest.mFormData);
            if (AwAutofillManager.isLoggable()) {
                AwAutofillManager.log("autofill values:" + values.size());
            }
            mAutofillUMA.onAutofill();
        }
    }

    @Override
    public boolean shouldQueryAutofillSuggestion() {
        return mRequest != null && mRequest.getFocusField() != null
                && !mAutofillManager.isAutofillInputUIShowing();
    }

    @Override
    public void queryAutofillSuggestion() {
        if (shouldQueryAutofillSuggestion()) {
            FocusField focusField = mRequest.getFocusField();
            mAutofillManager.requestAutofill(mContainerView,
                    mRequest.getVirtualId(focusField.fieldIndex), focusField.absBound);
        }
    }

    @Override
    public void startAutofillSession(
            FormData formData, int focus, float x, float y, float width, float height) {
        // Check focusField inside short value?
        // Autofill Manager might have session that wasn't started by WebView,
        // we just always cancel existing session here.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            mAutofillManager.cancel();
        }
        mAutofillManager.notifyNewSessionStarted();
        Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
        if (mRequest != null) notifyViewExitBeforeDestoryRequest();
        mRequest = new AutofillRequest(formData, new FocusField((short) focus, absBound));
        int virtualId = mRequest.getVirtualId((short) focus);
        mAutofillManager.notifyVirtualViewEntered(mContainerView, virtualId, absBound);
        mAutofillUMA.onSessionStarted(mAutofillManager.isDisabled());
        mAutofillTriggeredTimeMillis = System.currentTimeMillis();
    }

    @Override
    public void onFormFieldDidChange(int index, float x, float y, float width, float height) {
        // Check index inside short value?
        if (mRequest == null) return;

        short sIndex = (short) index;
        FocusField focusField = mRequest.getFocusField();
        if (focusField == null || sIndex != focusField.fieldIndex) {
            onFocusChangedImpl(true, index, x, y, width, height, true /*causedByValueChange*/);
        } else {
            // Currently there is no api to notify both value and position
            // change, before the API is available, we still need to call
            // notifyVirtualViewEntered() to tell current coordinates because
            // the position could be changed.
            int virtualId = mRequest.getVirtualId(sIndex);
            Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
            if (!focusField.absBound.equals(absBound)) {
                mAutofillManager.notifyVirtualViewExited(mContainerView, virtualId);
                mAutofillManager.notifyVirtualViewEntered(mContainerView, virtualId, absBound);
                // Update focus field position.
                mRequest.setFocusField(new FocusField(focusField.fieldIndex, absBound));
            }
        }
        notifyVirtualValueChanged(index);
        mAutofillUMA.onUserChangeFieldValue(mRequest.getField(sIndex).hasPreviouslyAutofilled());
    }

    @Override
    public void onTextFieldDidScroll(int index, float x, float y, float width, float height) {
        // crbug.com/730764 - from P and above, Android framework listens to the onScrollChanged()
        // and repositions the autofill UI automatically.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) return;
        if (mRequest == null) return;

        short sIndex = (short) index;
        FocusField focusField = mRequest.getFocusField();
        if (focusField == null || sIndex != focusField.fieldIndex) return;

        int virtualId = mRequest.getVirtualId(sIndex);
        Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
        // Notify the new position to the Android framework. Note that we do not call
        // notifyVirtualViewExited() here intentionally to avoid flickering.
        mAutofillManager.notifyVirtualViewEntered(mContainerView, virtualId, absBound);

        // Update focus field position.
        mRequest.setFocusField(new FocusField(focusField.fieldIndex, absBound));
    }

    private void notifyVirtualValueChanged(int index) {
        AutofillValue autofillValue = mRequest.getFieldNewValue(index);
        if (autofillValue == null) return;
        mAutofillManager.notifyVirtualValueChanged(
                mContainerView, mRequest.getVirtualId((short) index), autofillValue);
    }

    @Override
    public void onFormSubmitted(int submissionSource) {
        // The changes could be missing, like those made by Javascript, we'd better to notify
        // AutofillManager current values. also see crbug.com/353001 and crbug.com/732856.
        notifyFormValues();
        mAutofillManager.commit(submissionSource);
        mRequest = null;
        mAutofillUMA.onFormSubmitted(submissionSource);
    }

    @Override
    public void onFocusChanged(
            boolean focusOnForm, int focusField, float x, float y, float width, float height) {
        onFocusChangedImpl(
                focusOnForm, focusField, x, y, width, height, false /*causedByValueChange*/);
    }

    private void notifyViewExitBeforeDestoryRequest() {
        if (mRequest == null) return;
        FocusField focusField = mRequest.getFocusField();
        if (focusField == null) return;
        mAutofillManager.notifyVirtualViewExited(
                mContainerView, mRequest.getVirtualId(focusField.fieldIndex));
        mRequest.setFocusField(null);
    }

    private void onFocusChangedImpl(boolean focusOnForm, int focusField, float x, float y,
            float width, float height, boolean causedByValueChange) {
        // Check focusField inside short value?
        // FocusNoLongerOnForm is called after form submitted.
        if (mRequest == null) return;
        FocusField prev = mRequest.getFocusField();
        if (focusOnForm) {
            Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
            if (prev != null && prev.fieldIndex == focusField && absBound.equals(prev.absBound)) {
                return;
            }

            // Notify focus changed.
            if (prev != null) {
                mAutofillManager.notifyVirtualViewExited(
                        mContainerView, mRequest.getVirtualId(prev.fieldIndex));
            }

            mAutofillManager.notifyVirtualViewEntered(
                    mContainerView, mRequest.getVirtualId((short) focusField), absBound);

            if (!causedByValueChange) {
                // The focus field value might not sync with platform's
                // AutofillManager, just notify it value changed.
                notifyVirtualValueChanged(focusField);
                mAutofillTriggeredTimeMillis = System.currentTimeMillis();
            }
            mRequest.setFocusField(new FocusField((short) focusField, absBound));
        } else {
            if (prev == null) return;
            // Notify focus changed.
            mAutofillManager.notifyVirtualViewExited(
                    mContainerView, mRequest.getVirtualId(prev.fieldIndex));
            mRequest.setFocusField(null);
        }
    }

    @Override
    protected void reset() {
        // We don't need to reset anything here, it should be safe to cancel
        // current autofill session when new one starts in
        // startAutofillSession().
    }

    @Override
    protected void setNativeAutofillProvider(long nativeAutofillProvider) {
        if (nativeAutofillProvider == mNativeAutofillProvider) return;
        // Setting the mNativeAutofillProvider to 0 may occur as a
        // result of WebView.destroy, or because a WebView has been
        // gc'ed. In the former case we can go ahead and clean up the
        // frameworks autofill manager, but in the latter case the
        // binder connection has already been dropped in a framework
        // finalizer, and so the methods we call will throw. It's not
        // possible to know which case we're in, so just catch and
        // ignore the exception.
        try {
            if (mNativeAutofillProvider != 0) mRequest = null;
            mNativeAutofillProvider = nativeAutofillProvider;
            if (nativeAutofillProvider == 0) mAutofillManager.destroy();
        } catch (IllegalStateException e) {
        }
    }

    @Override
    public void setWebContents(WebContents webContents) {
        if (webContents == mWebContents) return;
        if (mWebContents != null) mRequest = null;
        mWebContents = webContents;
    }

    @Override
    protected void onDidFillAutofillFormData() {
        notifyFormValues();
    }

    private void notifyFormValues() {
        if (mRequest == null) return;
        for (int i = 0; i < mRequest.getFieldCount(); ++i) notifyVirtualValueChanged(i);
    }

    private Rect transformToWindowBounds(RectF rect) {
        // Convert bounds to device pixel.
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        DisplayAndroid displayAndroid = windowAndroid.getDisplay();
        float dipScale = displayAndroid.getDipScale();
        RectF bounds = new RectF(rect);
        Matrix matrix = new Matrix();
        matrix.setScale(dipScale, dipScale);
        int[] location = new int[2];
        mContainerView.getLocationOnScreen(location);
        matrix.postTranslate(location[0], location[1]);
        matrix.mapRect(bounds);
        return new Rect(
                (int) bounds.left, (int) bounds.top, (int) bounds.right, (int) bounds.bottom);
    }
}

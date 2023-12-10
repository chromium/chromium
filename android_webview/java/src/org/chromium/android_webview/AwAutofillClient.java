// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ContextUtils;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillPopup;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.PopupItemId;

/**
 * Java counterpart to the AwAutofillClient. This class is owned by AwContents and has
 * a weak reference from native side.
 */
@Lifetime.WebView
@JNINamespace("android_webview")
public class AwAutofillClient {
    private final long mNativeAwAutofillClient;
    private AutofillPopup mAutofillPopup;
    private Context mContext;

    @CalledByNative
    public static AwAutofillClient create(long nativeClient) {
        return new AwAutofillClient(nativeClient);
    }

    private AwAutofillClient(long nativeAwAutofillClient) {
        mNativeAwAutofillClient = nativeAwAutofillClient;
    }

    public void init(Context context) {
        mContext = context;
    }

    @CalledByNative
    private void showAutofillPopup(
            View anchorView, boolean isRtl, AutofillSuggestion[] suggestions) {

        if (mAutofillPopup == null) {
            if (ContextUtils.activityFromContext(mContext) == null) {
                AwAutofillClientJni.get().dismissed(mNativeAwAutofillClient, AwAutofillClient.this);
                return;
            }
            try {
                mAutofillPopup =
                        new AutofillPopup(
                                mContext,
                                anchorView,
                                new AutofillDelegate() {
                                    @Override
                                    public void dismissed() {
                                        AwAutofillClientJni.get()
                                                .dismissed(
                                                        mNativeAwAutofillClient,
                                                        AwAutofillClient.this);
                                    }

                                    @Override
                                    public void suggestionSelected(int listIndex) {
                                        AwAutofillClientJni.get()
                                                .suggestionSelected(
                                                        mNativeAwAutofillClient,
                                                        AwAutofillClient.this,
                                                        listIndex);
                                    }

                                    @Override
                                    public void deleteSuggestion(int listIndex) {}

                                    @Override
                                    public void accessibilityFocusCleared() {}
                                },
                                null);
            } catch (RuntimeException e) {
                // Deliberately swallowing exception because bad fraemwork implementation can
                // throw exceptions in ListPopupWindow constructor.
                AwAutofillClientJni.get().dismissed(mNativeAwAutofillClient, AwAutofillClient.this);
                return;
            }
        }
        mAutofillPopup.filterAndShow(suggestions, isRtl);
    }

    @CalledByNative
    public void hideAutofillPopup() {
        if (mAutofillPopup == null) return;
        mAutofillPopup.dismiss();
        mAutofillPopup = null;
    }

    @CalledByNative
    private static AutofillSuggestion[] createAutofillSuggestionArray(int size) {
        return new AutofillSuggestion[size];
    }

    /**
     * @param array AutofillSuggestion array that should get a new suggestion added.
     * @param index Index in the array where to place a new suggestion.
     * @param name Name of the suggestion.
     * @param label Label of the suggestion.
     * @param uniqueId Unique suggestion id.
     */
    @CalledByNative
    private static void addToAutofillSuggestionArray(
            AutofillSuggestion[] array,
            int index,
            String name,
            String label,
            @PopupItemId int popupItemId) {
        array[index] =
                new AutofillSuggestion.Builder()
                        .setLabel(name)
                        .setSecondarySubLabel(label)
                        .setItemTag("")
                        .setPopupItemId(popupItemId)
                        .setFeatureForIPH("")
                        .build();
    }

    @NativeMethods
    interface Natives {
        void dismissed(long nativeAwAutofillClient, AwAutofillClient caller);

        void suggestionSelected(long nativeAwAutofillClient, AwAutofillClient caller, int position);
    }
}

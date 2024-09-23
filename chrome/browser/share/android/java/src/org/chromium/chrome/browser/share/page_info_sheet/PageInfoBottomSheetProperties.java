// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Data properties for the page info bottom sheet contents. */
final class PageInfoBottomSheetProperties {

    static final WritableIntPropertyKey STATE = new WritableIntPropertyKey();

    static final WritableObjectPropertyKey<String> CONTENT_TEXT = new WritableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> ON_ACCEPT_CLICKED =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> ON_CANCEL_CLICKED =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<Callback<View>> ON_LEARN_MORE_CLICKED =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> ON_POSITIVE_FEEDBACK_CLICKED =
            new ReadableObjectPropertyKey<>();

    static final ReadableObjectPropertyKey<OnClickListener> ON_NEGATIVE_FEEDBACK_CLICKED =
            new ReadableObjectPropertyKey<>();

    /**
     * Possible states for the bottom sheet UI, used to show and hide different elements inside the
     * bottom sheet (e.g. loading indicator, feedback buttons).
     */
    @IntDef({
        PageInfoState.INITIALIZING,
        PageInfoState.LOADING,
        PageInfoState.SUCCESS,
        PageInfoState.ERROR
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface PageInfoState {
        /** Page info fetch is initializing, show a loading indicator. */
        int INITIALIZING = 0;

        /** Page info is loading, show progress message. */
        int LOADING = 1;

        /** Page info loaded successfully, show info and enable accept button. */
        int SUCCESS = 2;

        /** Page info failed to load, show error message. */
        int ERROR = 3;
    }

    private PageInfoBottomSheetProperties() {}

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                STATE,
                CONTENT_TEXT,
                ON_ACCEPT_CLICKED,
                ON_CANCEL_CLICKED,
                ON_LEARN_MORE_CLICKED,
                ON_POSITIVE_FEEDBACK_CLICKED,
                ON_NEGATIVE_FEEDBACK_CLICKED,
            };

    static PropertyModel.Builder defaultModelBuilder() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(STATE, PageInfoState.INITIALIZING)
                .with(CONTENT_TEXT, "");
    }


}

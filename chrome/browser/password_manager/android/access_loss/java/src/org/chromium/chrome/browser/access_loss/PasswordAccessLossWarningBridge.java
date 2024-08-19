// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.ALL_KEYS;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.BUTTON_TITLE;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TEXT;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TITLE;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

class PasswordAccessLossWarningBridge {
    final Context mContext;
    final BottomSheetController mBottomSheetController;

    public PasswordAccessLossWarningBridge(
            Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
    }

    @CalledByNative
    @Nullable
    static PasswordAccessLossWarningBridge create(WindowAndroid windowAndroid) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) {
            return null;
        }
        Context context = windowAndroid.getContext().get();
        if (context == null) {
            return null;
        }
        return new PasswordAccessLossWarningBridge(context, bottomSheetController);
    }

    @CalledByNative
    public void show(@PasswordAccessLossWarningType int warningType) {
        SimpleNoticeSheetCoordinator coordinator =
                new SimpleNoticeSheetCoordinator(mContext, mBottomSheetController);
        // TODO: crbug.com/353283268 - Use the warningType to show the sheet with specific looks.
        PropertyModel model =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(
                                SHEET_TITLE,
                                mContext.getString(
                                        R.string.pwd_access_loss_warning_no_gms_core_title))
                        .with(SHEET_TEXT, getBottomSheetTextWithLink())
                        .with(
                                BUTTON_TITLE,
                                mContext.getString(
                                        R.string.pwd_access_loss_warning_no_gms_core_button_text))
                        .build();
        coordinator.showSheet(model);
    }

    private String getBottomSheetTextWithLink() {
        String sheetText = mContext.getString(R.string.pwd_access_loss_warning_no_gms_core_text);
        sheetText =
                SpanApplier.applySpans(
                                sheetText,
                                new SpanApplier.SpanInfo(
                                        "<link>",
                                        "</link>",
                                        new NoUnderlineClickableSpan(
                                                mContext, this::onLearnMoreClicked)))
                        .toString();
        return sheetText;
    }

    private void onLearnMoreClicked(View view) {
        // TODO: crbug.com/360346943 - Open the help centre article.
    }
}

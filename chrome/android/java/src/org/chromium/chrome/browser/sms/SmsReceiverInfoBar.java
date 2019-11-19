// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sms;

import android.app.Activity;
import android.content.Context;
import android.os.SystemClock;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.infobar.ConfirmInfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainerLayout.Item.InfoBarPriority;
import org.chromium.chrome.browser.infobar.InfoBarControlLayout;
import org.chromium.chrome.browser.infobar.InfoBarLayout;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * An InfoBar that asks for the user's permission to share the SMS with the page.
 */
public class SmsReceiverInfoBar extends ConfirmInfoBar {
    private static final String TAG = "SmsReceiverInfoBar";
    private static final boolean DEBUG = false;
    private String mMessage;
    private WindowAndroid mWindowAndroid;
    private Long mKeyboardDismissedTime;

    @VisibleForTesting
    @CalledByNative
    static SmsReceiverInfoBar create(WindowAndroid windowAndroid, int enumeratedIconId,
            String title, String message, String okButtonLabel) {
        if (DEBUG) Log.d(TAG, "SmsReceiverInfoBar.create()");
        return new SmsReceiverInfoBar(
                windowAndroid, enumeratedIconId, title, message, okButtonLabel);
    }

    private SmsReceiverInfoBar(WindowAndroid windowAndroid, int enumeratedIconId, String title,
            String message, String okButtonLabel) {
        super(ResourceId.mapToDrawableId(enumeratedIconId), R.color.infobar_icon_drawable_color,
                /*iconBitmap=*/null, /*message=*/title, /*linkText=*/null, okButtonLabel,
                /*secondaryButtonText=*/null);
        mMessage = message;
        mWindowAndroid = windowAndroid;
    }

    @Override
    public int getPriority() {
        return InfoBarPriority.USER_TRIGGERED;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        SmsReceiverUma.recordInfobarAction(SmsReceiverUma.InfobarAction.SHOWN);

        Activity activity = mWindowAndroid.getActivity().get();
        if (activity != null) {
            View focusedView = activity.getCurrentFocus();
            KeyboardVisibilityDelegate keyboardVisibilityDelegate =
                    KeyboardVisibilityDelegate.getInstance();
            if (focusedView != null
                    && keyboardVisibilityDelegate.isKeyboardShowing(activity, focusedView)) {
                keyboardVisibilityDelegate.hideKeyboard(focusedView);
                SmsReceiverUma.recordInfobarAction(SmsReceiverUma.InfobarAction.KEYBOARD_DISMISSED);
                mKeyboardDismissedTime = SystemClock.uptimeMillis();
            }
        }

        Context context = layout.getContext();
        InfoBarControlLayout control = layout.addControlLayout();
        control.addDescription(mMessage);
    }

    @Override
    public void onCloseButtonClicked() {
        super.onCloseButtonClicked();

        if (mKeyboardDismissedTime != null) {
            SmsReceiverUma.recordCancelTimeAfterKeyboardDismissal(
                    SystemClock.uptimeMillis() - mKeyboardDismissedTime);
        }
    }
}

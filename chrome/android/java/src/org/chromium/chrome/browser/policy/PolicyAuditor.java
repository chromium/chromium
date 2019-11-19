// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Base class for policy auditors providing an empty implementation.
 */
public class PolicyAuditor {
    /**
     * Events that a policy administrator may want to track.
     */
    @IntDef({AuditEvent.OPEN_URL_SUCCESS, AuditEvent.OPEN_URL_FAILURE, AuditEvent.OPEN_URL_BLOCKED,
            AuditEvent.OPEN_POPUP_URL_SUCCESS, AuditEvent.AUTOFILL_SELECTED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AuditEvent {
        int OPEN_URL_SUCCESS = 0;
        int OPEN_URL_FAILURE = 1;
        int OPEN_URL_BLOCKED = 2;
        int OPEN_POPUP_URL_SUCCESS = 3;
        int AUTOFILL_SELECTED = 4;
    }

    /**
     * Make it non-obvious to accidentally instantiate this outside of ChromeApplication.
     */
    protected PolicyAuditor() {}

    public void notifyAuditEvent(
            Context context, @AuditEvent int event, String url, String message) {}

    public void notifyCertificateFailure(int certificateFailure, Context context) {}

    @NativeMethods
    public interface Natives {
        int getCertificateFailure(WebContents webContents);
    }
}

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.content.Context;

import androidx.annotation.IntDef;

import org.jni_zero.NativeMethods;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Base class for policy auditors providing an empty implementation. */
@NullMarked
public class PolicyAuditor {

    /** Events that a policy administrator may want to track. */
    @IntDef({
        AuditEvent.OPEN_URL_SUCCESS,
        AuditEvent.OPEN_URL_FAILURE,
        AuditEvent.OPEN_URL_BLOCKED,
        AuditEvent.OPEN_POPUP_URL_SUCCESS,
        AuditEvent.AUTOFILL_SELECTED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AuditEvent {
        int OPEN_URL_SUCCESS = 0;
        int OPEN_URL_FAILURE = 1;
        int OPEN_URL_BLOCKED = 2;
        int OPEN_POPUP_URL_SUCCESS = 3;
        int AUTOFILL_SELECTED = 4;
    }

    private static @Nullable PolicyAuditor sInstance;

    /**
     * Returns an instance of PolicyAuditor if it is enabled, otherwise returns null.
     *
     * <p>This method is used to get the PolicyAuditor instance in a way that is compatible with
     * ChromeApplicationImpl.
     */
    public static @Nullable PolicyAuditor maybeGetInstance() {
        if (sInstance == null) {
            sInstance = ServiceLoaderUtil.maybeCreate(PolicyAuditor.class);
        }
        return sInstance;
    }

    /** Make it non-obvious to accidentally instantiate this outside of ChromeApplicationImpl. */
    protected PolicyAuditor() {}

    public void notifyAuditEvent(
            Context context, @AuditEvent int event, String url, String message) {}

    public void notifyCertificateFailure(int certificateFailure, Context context) {}

    @NativeMethods
    public interface Natives {
        int getCertificateFailure(WebContents webContents);
    }
}

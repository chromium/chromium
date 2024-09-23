// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.net.NetError;
import org.chromium.url.GURL;

/** Provides native with methods to call to audit events during navigations. */
public class PolicyAuditorBridge {
    private static void recordErrorInPolicyAuditor(
            String failingUrl, String description, int errorCode, PolicyAuditor policyAuditor) {
        assert description != null;

        policyAuditor.notifyAuditEvent(
                ContextUtils.getApplicationContext(),
                PolicyAuditor.AuditEvent.OPEN_URL_FAILURE,
                failingUrl,
                description);
        if (errorCode == NetError.ERR_BLOCKED_BY_ADMINISTRATOR) {
            policyAuditor.notifyAuditEvent(
                    ContextUtils.getApplicationContext(),
                    PolicyAuditor.AuditEvent.OPEN_URL_BLOCKED,
                    failingUrl,
                    "");
        }
    }

    @CalledByNative
    public static @Nullable PolicyAuditor getPolicyAuditor() {
        return PolicyAuditor.maybeCreate();
    }

    @CalledByNative
    public static void notifyAuditEventForDidFinishNavigation(
            NavigationHandle navigationHandle, PolicyAuditor policyAuditor) {
        if (navigationHandle.errorCode() != NetError.OK) {
            recordErrorInPolicyAuditor(
                    navigationHandle.getUrl().getSpec(),
                    navigationHandle.errorDescription(),
                    navigationHandle.errorCode(),
                    policyAuditor);
        }
    }

    @CalledByNative
    public static void notifyAuditEventForDidFinishLoad(GURL url, PolicyAuditor policyAuditor) {
        policyAuditor.notifyAuditEvent(
                ContextUtils.getApplicationContext(),
                PolicyAuditor.AuditEvent.OPEN_URL_SUCCESS,
                url.getSpec(),
                "");
    }
}

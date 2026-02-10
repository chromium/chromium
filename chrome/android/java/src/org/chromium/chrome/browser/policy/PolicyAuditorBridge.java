// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.net.NetError;
import org.chromium.url.GURL;

/** Provides native with methods to call to audit events during navigations. */
@NullMarked
public class PolicyAuditorBridge {
    @CalledByNative
    public static @Nullable PolicyAuditor maybeGetPolicyAuditorInstance() {
        return PolicyAuditor.maybeGetInstance();
    }

    @CalledByNative
    public static void notifyAuditEventForDidFinishNavigation(NavigationHandle navigationHandle) {
        PolicyAuditor policyAuditor = PolicyAuditor.maybeGetInstance();
        if (policyAuditor == null) return;

        if (navigationHandle.errorCode() != NetError.OK) {
            policyAuditor.notifyAuditEvent(
                    ContextUtils.getApplicationContext(),
                    PolicyAuditor.AuditEvent.OPEN_URL_FAILURE,
                    navigationHandle.getUrl().getSpec(),
                    navigationHandle.errorDescription() != null
                            ? navigationHandle.errorDescription()
                            : "");
            if (navigationHandle.errorCode() == NetError.ERR_BLOCKED_BY_ADMINISTRATOR) {
                policyAuditor.notifyAuditEvent(
                        ContextUtils.getApplicationContext(),
                        PolicyAuditor.AuditEvent.OPEN_URL_BLOCKED,
                        navigationHandle.getUrl().getSpec(),
                        "");
            }
        }
    }

    @CalledByNative
    public static void notifyAuditEventForDidFinishLoad(GURL url) {
        PolicyAuditor policyAuditor = PolicyAuditor.maybeGetInstance();
        if (policyAuditor == null) return;

        policyAuditor.notifyAuditEvent(
                ContextUtils.getApplicationContext(),
                PolicyAuditor.AuditEvent.OPEN_URL_SUCCESS,
                url.getSpec(),
                "");
    }
}

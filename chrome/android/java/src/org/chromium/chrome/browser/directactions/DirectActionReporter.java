// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import android.annotation.TargetApi;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A helper for reporting simple direct actions to {@code Activity.onGetDirectActions}.
 *
 * <p>This is not a generic API; it only supports the subset of action schema Chrome actually needs.
 *
 * <h2>Usage example</h2>
 *
 * <pre>
 * void onGetDirectActions(..., Consumer<List> callback) {
 *   DirectActionReporter reporter = new ...
 *   reporter.addDirectAction("noarg");
 *   reporter.addDirectAction("withargs")
 *       .withParameter("arg1_required", Type.STRING, true)
 *       .withParameter("arg2_optional", Type.STRING, false);
 *   reporter.addDirectAction("return_bool").withResult("result", Type.BOOLEAN);
 *   reporter.report();
 * }
 * </pre>
 *
 * <p>This class produces lists of {@code android.app.DirectAction}. Since this class is only
 * available starting with API 29, this class accesses these as {@link List}, without specifying the
 * instance type, to allow compiling against older SDKs. TODO(crbug.com/973781): Clean it up once
 * Chromium is compiled against Android Q SDK.
 */
@TargetApi(29)
public interface DirectActionReporter {
    /** Parameter or result type. */
    @IntDef({Type.STRING, Type.BOOLEAN, Type.INT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        int STRING = 0;
        int BOOLEAN = 1;
        int INT = 2;

        /** The number of types. Increment this value if you add a type. */
        int NUM_ENTRIES = 3;
    }

    /** Report the direct actions to the callback. */
    void report();

    /**
     * Adds a direct action to report. The returned definition can be modified until the actions are
     * reported.
     *
     * <p>Adding a definition for direct action with the same name as an existing one replaces the
     * previous definition.
     */
    Definition addDirectAction(String name);

    /** Definition of a direct action to which arguments and results can be added. */
    interface Definition {
        /** Declares a parameter for the current action. */
        Definition withParameter(String name, @Type int type, boolean required);

        /** Declares a result for the current action. */
        Definition withResult(String name, @Type int type);
    }
}

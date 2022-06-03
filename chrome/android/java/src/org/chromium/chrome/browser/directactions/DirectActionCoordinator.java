// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import android.annotation.TargetApi;
import android.os.Bundle;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;

import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Consumer;

/**
 * Reports available direct actions and executes them.
 *
 * <p>Handlers can define any direct action they want, but the client of direct actions need to be
 * aware of them to make use of them. For this to work, if you add a new action here, please list it
 * and document it in http://go.ext.google.com/chrome-direct-action-list
 */
@TargetApi(29)
public abstract class DirectActionCoordinator {
    private final Set<DirectActionHandler> mHandlers = new LinkedHashSet<>();

    /**
     * Dynamic check of whether direct actions should be enabled. This is run before every call.
     *
     * <p>During startup, assume direct actions are disabled until initialized.
     */
    private Supplier<Boolean> mIsEnabled = () -> false;

    /** Registers a handler. */
    public void register(DirectActionHandler handler) {
        mHandlers.add(handler);
    }

    /** Unregisters a handler. */
    public void unregister(DirectActionHandler handler) {
        mHandlers.remove(handler);
    }

    /** Sends a list of supported actions to the given callback. */
    public final void onGetDirectActions(Consumer<List> callback) {
        DirectActionReporter reporter = createReporter(callback);
        if (mIsEnabled.get()) {
            for (DirectActionHandler handler : mHandlers) {
                handler.reportAvailableDirectActions(reporter);
            }
        }
        reporter.report();
        RecordUserAction.record("Android.DirectAction.List");
    }

    /** Performs an action and reports the result to the callback. */
    public final void onPerformDirectAction(
            String actionId, Bundle arguments, Consumer<Bundle> consumer) {
        if (!mIsEnabled.get()) return;

        boolean performedAction = false;
        Callback<Bundle> callback = (result) -> consumer.accept(result);
        for (DirectActionHandler handler : mHandlers) {
            if (handler.performDirectAction(actionId, arguments, callback)) {
                performedAction = true;
                break;
            }
        }
        if (performedAction) {
            DirectActionUsageHistogram.record(actionId);
        } else {
            DirectActionUsageHistogram.recordUnknown();
        }
    }

    /** Subclasses should provide an implementation of a DirectActionReporter. */
    protected abstract DirectActionReporter createReporter(Consumer<List> callback);

    /** Initializes the coordinator. */
    void init(@NonNull Supplier<Boolean> isEnabled) {
        mIsEnabled = isEnabled;
    }
}

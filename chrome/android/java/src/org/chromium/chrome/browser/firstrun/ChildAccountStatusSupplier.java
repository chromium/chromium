// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.os.SystemClock;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;

/**
 * Fetches the child account status to be used by other FRE components.
 *
 * TODO(https://crbug.com/1320374): Check app restrictions to speed up this listener.
 */
public class ChildAccountStatusSupplier implements OneshotSupplier<Boolean> {
    private final OneshotSupplierImpl<Boolean> mValue = new OneshotSupplierImpl<>();

    /**
     * Creates ChildAccountStatusSupplier. This doesn't actually start fetching the child account
     * status until {@link #startFetchingChildAccountStatus()} is invoked.
     */
    public ChildAccountStatusSupplier() {}

    /**
     * Starts the process to obtain the child account status from {@link AccountManagerFacade}.
     * Should be invoked after AccountManagerFacade instance has been already set.
     *
     * TODO(https://crbug.com/1320487): Add Supplier to AccountManagerFacadeProvider and remove this
     *                                  method.
     */
    public void startFetchingChildAccountStatus() {
        long childAccountStatusStart = SystemClock.elapsedRealtime();
        AccountManagerFacadeProvider.getInstance().getAccounts().then(accounts -> {
            AccountUtils.checkChildAccountStatus(
                    AccountManagerFacadeProvider.getInstance(), accounts, (isChild, account) -> {
                        RecordHistogram.recordTimesHistogram("MobileFre.ChildAccountStatusDuration",
                                SystemClock.elapsedRealtime() - childAccountStatusStart);
                        mValue.set(isChild);
                    });
        });
    }

    @Override
    public Boolean onAvailable(Callback<Boolean> callback) {
        return mValue.onAvailable(callback);
    }

    @Override
    public Boolean get() {
        return mValue.get();
    }
}

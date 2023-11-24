// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.os.SystemClock;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountUtils;

/**
 * Fetches the child account status to be used by other FRE components.
 *
 * This class checks app restrictions for Chrome to obtain the child account status faster. This
 * optimisation leverages the fact that FamilyLink always pushes some policies for Chrome on
 * supervised devices. So, if there are no app restrictions specified for Chrome -
 * {@link ChildAccountStatusSupplier} will consider that the child account status is false.
 * Note: this optimisation creates a potential conflict if there are no app restrictions on
 * a supervised device. However, this should never happen on real devices.
 */
public class ChildAccountStatusSupplier implements OneshotSupplier<Boolean> {
    private final OneshotSupplierImpl<Boolean> mValue = new OneshotSupplierImpl<>();
    private final long mChildAccountStatusStartTime;

    private Boolean mHasRestriction;
    private Boolean mChildAccountStatusFromAccountManagerFacade;

    /**
     * Creates ChildAccountStatusSupplier and starts fetching the child account status.
     * @param accountManagerFacade {@link AccountManagerFacade} instance to use for getting accounts
     * @param appRestrictionInfo instance of {@link FirstRunAppRestrictionInfo} that can
     *         be used to check app restrictions (see class-level JavaDoc).
     */
    public ChildAccountStatusSupplier(
            AccountManagerFacade accountManagerFacade,
            FirstRunAppRestrictionInfo appRestrictionInfo) {
        mChildAccountStatusStartTime = SystemClock.elapsedRealtime();

        appRestrictionInfo.getHasAppRestriction(this::onAppRestrictionDetected);

        accountManagerFacade
                .getCoreAccountInfos()
                .then(
                        coreAccountInfos -> {
                            AccountUtils.checkChildAccountStatus(
                                    accountManagerFacade,
                                    coreAccountInfos,
                                    (isChild, account) -> onChildAccountStatusReady(isChild));
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

    private void onAppRestrictionDetected(boolean hasAppRestriction) {
        mHasRestriction = hasAppRestriction;
        setSupplierIfDecidable();
    }

    private void onChildAccountStatusReady(boolean isChild) {
        mChildAccountStatusFromAccountManagerFacade = isChild;
        setSupplierIfDecidable();
    }

    private void setSupplierIfDecidable() {
        // Early return if the value has been set.
        if (mValue.get() != null) return;

        Boolean value = tryCalculateSupplierValue();
        if (value == null) return;

        RecordHistogram.recordTimesHistogram(
                "MobileFre.ChildAccountStatusDuration",
                SystemClock.elapsedRealtime() - mChildAccountStatusStartTime);
        mValue.set(value);
    }

    private @Nullable Boolean tryCalculateSupplierValue() {
        if (mChildAccountStatusFromAccountManagerFacade != null) {
            // Child account status from AccountManagerFacade is more reliable than app
            // restrictions, so use it if available.
            return mChildAccountStatusFromAccountManagerFacade;
        }

        boolean confirmedNoAppRestriction = mHasRestriction != null && !mHasRestriction;
        if (confirmedNoAppRestriction) {
            // No app restriction is found. On real devices this means that there are no child
            // accounts on the device, as FamilyLink pushes some policies for supervised devices.
            return false;
        }
        // Otherwise, we can't determine the supplier value yet.
        return null;
    }
}

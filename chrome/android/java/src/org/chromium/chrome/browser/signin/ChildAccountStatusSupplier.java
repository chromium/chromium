// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.os.SystemClock;

import org.chromium.base.Callback;
import org.chromium.base.TriState;
import org.chromium.base.TriStateUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountUtils;

/**
 * Fetches the child account status to be used by other fullscreen signin flows components.
 *
 * <p>This class checks app restrictions for Chrome to obtain the child account status faster. This
 * optimisation leverages the fact that FamilyLink always pushes some policies for Chrome on
 * supervised devices. So, if there are no app restrictions specified for Chrome - {@link
 * ChildAccountStatusSupplier} will consider that the child account status is false. Note: this
 * optimisation creates a potential conflict if there are no app restrictions on a supervised
 * device. However, this should never happen on real devices.
 */
@NullMarked
public class ChildAccountStatusSupplier implements OneshotSupplier<Boolean> {
    private final OneshotSupplierImpl<Boolean> mValue = new OneshotSupplierImpl<>();
    private final long mChildAccountStatusStartTime;

    private @TriState int mHasRestriction;
    private @TriState int mChildAccountStatusFromAccountManagerFacade;

    /**
     * Creates ChildAccountStatusSupplier and starts fetching the child account status.
     *
     * @param accountManagerFacade {@link AccountManagerFacade} instance to use for getting accounts
     * @param appRestrictionInfo instance of {@link AppRestrictionSupplier} that can be used to
     *     check app restrictions (see class-level JavaDoc).
     */
    public ChildAccountStatusSupplier(
            AccountManagerFacade accountManagerFacade, AppRestrictionSupplier appRestrictionInfo) {
        mChildAccountStatusStartTime = SystemClock.elapsedRealtime();

        appRestrictionInfo.onAvailable(this::onAppRestrictionDetected);

        accountManagerFacade
                .getAccounts()
                .then(
                        accounts -> {
                            AccountUtils.checkIsSubjectToParentalControls(
                                    accountManagerFacade,
                                    accounts,
                                    (isChild, account) -> onChildAccountStatusReady(isChild));
                        });
    }

    @Override
    public @Nullable Boolean onAvailable(Callback<Boolean> callback) {
        return mValue.onAvailable(callback);
    }

    @Override
    @SuppressWarnings("NullAway") // https://github.com/uber/NullAway/issues/1209
    public @Nullable Boolean get() {
        return mValue.get();
    }

    private void onAppRestrictionDetected(boolean hasAppRestriction) {
        mHasRestriction = TriStateUtils.from(hasAppRestriction);
        setSupplierIfDecidable();
    }

    private void onChildAccountStatusReady(boolean isChild) {
        mChildAccountStatusFromAccountManagerFacade = TriStateUtils.from(isChild);
        setSupplierIfDecidable();
    }

    private void setSupplierIfDecidable() {
        // Early return if the value has been set.
        if (mValue.get() != null) return;

        @TriState int value = tryCalculateSupplierValue();
        if (value == TriState.NOT_SET) return;

        RecordHistogram.recordTimesHistogram(
                "MobileFre.ChildAccountStatusDuration",
                SystemClock.elapsedRealtime() - mChildAccountStatusStartTime);
        mValue.set(value == TriState.TRUE);
    }

    private @TriState int tryCalculateSupplierValue() {
        if (mChildAccountStatusFromAccountManagerFacade != TriState.NOT_SET) {
            // Child account status from AccountManagerFacade is more reliable than app
            // restrictions, so use it if available.
            return mChildAccountStatusFromAccountManagerFacade;
        }

        if (mHasRestriction == TriState.FALSE) {
            // No app restriction is found. On real devices this means that there are no child
            // accounts on the device, as FamilyLink pushes some policies for supervised devices.
            return TriState.FALSE;
        }
        // Otherwise, we can't determine the supplier value yet.
        return TriState.NOT_SET;
    }
}

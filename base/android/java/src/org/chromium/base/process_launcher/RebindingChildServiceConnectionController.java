// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.Context;
import android.content.Intent;
import android.os.Build;

import androidx.annotation.ChecksSdkIntAtLeast;

import org.chromium.base.BaseFeatureList;
import org.chromium.base.ChildBindingState;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import javax.annotation.concurrent.GuardedBy;

/**
 * Manages the bindings of a {@link ChildProcessConnection} using a single service connection and
 * the rebindService() API.
 */
@NullMarked
/* package */ class RebindingChildServiceConnectionController
        implements ChildServiceConnectionController {

    // Upgrades to this binding state or above are sent to the system immediately instead of being
    // deferred by an enclosing ScopedServiceBindingBatch. A process bound at VISIBLE or STRONG is
    // expected to produce content the user can see, so keeping it on its previous binding flags -
    // and therefore its previous oom_score_adj and scheduler group - directly delays painting.
    // Upgrades to WAIVED and NOT_PERCEPTIBLE are not user visible and stay batched, which keeps
    // the bulk of the batching win (e.g. the many demotions issued during a tab switch).
    // See crbug.com/465607095.
    private static final @ChildBindingState int MIN_URGENT_BINDING_STATE =
            ChildBindingState.VISIBLE;

    private final ChildServiceConnectionFactory mConnectionFactory;
    // The service binding flags for the default binding (i.e. visible binding).
    private final int mDefaultBindFlags;
    // Instance named used on Android 10 and above to create separate instances from the same
    // <service> manifest declaration.
    private final @Nullable String mInstanceName;
    // ChildServiceConnectionDelegate for this class which is responsible for posting callbacks to
    // the launcher thread, if needed.
    private final ChildServiceConnectionDelegate mConnectionDelegate;
    private Intent mBindIntent;

    private @Nullable ChildServiceConnection mConnection;

    // Set to true once unbind() was called.
    private boolean mUnbound;

    private final Object mBindingStateLock = new Object();

    // Binding state of this connection.
    @GuardedBy("mBindingStateLock")
    private @ChildBindingState int mBindingState;

    // Same as above except it no longer updates after |unbind()|.
    @GuardedBy("mBindingStateLock")
    private @ChildBindingState int mBindingStateCurrentOrWhenDied;

    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.CINNAMON_BUN)
    public static boolean isEnabled() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.CINNAMON_BUN
                && BaseFeatureList.sRebindingChildServiceConnectionController.isEnabled();
    }

    RebindingChildServiceConnectionController(
            ChildServiceConnectionFactory connectionFactory,
            Intent bindIntent,
            int defaultBindFlags,
            ChildServiceConnectionDelegate connectionDelegate,
            @Nullable String instanceName) {
        mConnectionFactory = connectionFactory;
        mBindIntent = bindIntent;
        mDefaultBindFlags = defaultBindFlags;
        mConnectionDelegate = connectionDelegate;
        mInstanceName = instanceName;
    }

    @Override
    public boolean bind(@ChildBindingState int initialBindingState) {
        if (initialBindingState == ChildBindingState.UNBOUND) {
            return false;
        }
        assert !mUnbound;
        assert mConnection == null;

        mConnection =
                mConnectionFactory.createConnection(
                        mBindIntent,
                        getBindFlags(initialBindingState),
                        mConnectionDelegate,
                        mInstanceName);
        boolean success = mConnection.bindServiceConnection();
        if (success) {
            updateBindingState(initialBindingState);
        }
        return success;
    }

    @Override
    public void unbind() {
        assert mConnection != null;
        mUnbound = true;
        updateBindingState(ChildBindingState.UNBOUND);
        mConnection.unbindServiceConnection(null);
    }

    @Override
    public void rebind() {
        assert mConnection != null;
        @ChildBindingState int bindingState = getBindingState();
        if (bindingState == ChildBindingState.UNBOUND) {
            return;
        }
        // This re-applies the flags of the current binding state rather than raising it, so it is
        // not latency sensitive and can stay batched.
        mConnection.rebindService(getBindFlags(bindingState), /* urgent= */ false);
    }

    @Override
    public boolean updateGroupImportance(int group, int importanceInGroup) {
        assert mConnection != null;
        return mConnection.updateGroupImportance(group, importanceInGroup);
    }

    @Override
    public void replaceService(Intent bindIntent) {
        assert mConnection != null;
        mBindIntent = bindIntent;
        mConnection.retire();
        @ChildBindingState int bindingState = getBindingState();
        if (bindingState == ChildBindingState.UNBOUND) {
            // If ChildProcessConnection fails to bind(), it replaceService() here and then calls
            // bind() again. So we don't need to bind the connection here.
            return;
        }
        mConnection =
                mConnectionFactory.createConnection(
                        mBindIntent,
                        getBindFlags(bindingState),
                        mConnectionDelegate,
                        mInstanceName);
        boolean success = mConnection.bindServiceConnection();
        if (success) {
            updateBindingState(bindingState);
        }
    }

    @Override
    public void setEffectiveBindingState(@ChildBindingState int effectiveBindingState) {
        assert mConnection != null;

        if (mUnbound) {
            return;
        }
        @ChildBindingState int currentBindingState = getBindingState();
        if (effectiveBindingState == currentBindingState) {
            return;
        }
        if (currentBindingState == ChildBindingState.UNBOUND) {
            return;
        }
        boolean isUpgrading = effectiveBindingState > currentBindingState;
        // If downgrading the binding state, we update the binding state before updating the
        // flags. This prevents the process is recorded as LMK-ed with high priority when the
        // process is LMK-ed as soon as the flag is downgraded.
        if (!isUpgrading) {
            updateBindingState(effectiveBindingState);
        }
        final boolean urgent = isUpgrading && effectiveBindingState >= MIN_URGENT_BINDING_STATE;
        mConnection.rebindService(getBindFlags(effectiveBindingState), urgent);
        if (isUpgrading) {
            updateBindingState(effectiveBindingState);
        }
    }



    @Override
    public @ChildBindingState int getBindingState() {
        synchronized (mBindingStateLock) {
            return mBindingState;
        }
    }

    @Override
    public @ChildBindingState int getBindingStateCurrentOrWhenDied() {
        synchronized (mBindingStateLock) {
            return mBindingStateCurrentOrWhenDied;
        }
    }

    @Override
    public boolean isUnbound() {
        return mUnbound;
    }

    @Override
    public ChildProcessConnectionState getConnectionStateForDebugging() {
        boolean isWaivedBound = false;
        boolean isNotPerceptibleBound = false;
        boolean isVisibleBound = false;
        boolean isStrongBound = false;
        @ChildBindingState int bindingState = getBindingState();
        switch (bindingState) {
            case ChildBindingState.WAIVED:
                isWaivedBound = true;
                break;
            case ChildBindingState.VISIBLE:
                isVisibleBound = true;
                break;
            case ChildBindingState.NOT_PERCEPTIBLE:
                isNotPerceptibleBound = true;
                break;
            case ChildBindingState.STRONG:
                isStrongBound = true;
                break;
            default:
                break;
        }
        return new ChildProcessConnectionState(
                isWaivedBound, isNotPerceptibleBound, isVisibleBound, isStrongBound);
    }

    private int getBindFlags(@ChildBindingState int bindingState) {
        int flags = mDefaultBindFlags;
        switch (bindingState) {
            case ChildBindingState.STRONG:
                flags |= Context.BIND_IMPORTANT;
                break;
            case ChildBindingState.VISIBLE:
                // No flags to set.
                break;
            case ChildBindingState.NOT_PERCEPTIBLE:
                flags |= Context.BIND_NOT_PERCEPTIBLE;
                if (BaseFeatureList.sBackgroundNotPerceptibleBinding.isEnabled()) {
                    flags |= Context.BIND_NOT_FOREGROUND;
                }
                break;
            case ChildBindingState.WAIVED:
                flags |= Context.BIND_WAIVE_PRIORITY;
                break;
            case ChildBindingState.UNBOUND:
            default:
                assert false;
                return 0;
        }
        return flags;
    }

    private void updateBindingState(@ChildBindingState int newBindingState) {
        synchronized (mBindingStateLock) {
            mBindingState = newBindingState;
            if (!mUnbound) {
                mBindingStateCurrentOrWhenDied = mBindingState;
            }
        }
    }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.BaseFeatureList;
import org.chromium.base.ChildBindingState;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import javax.annotation.concurrent.GuardedBy;

/**
 * Manages the bindings of a {@link ChildProcessConnection} using a single service connection and
 * the rebindService() API.
 *
 * <p>This requires the EffectiveBindingState feature to be enabled.
 */
@NullMarked
/* package */ class RebindingChildServiceConnectionController
        implements ChildServiceConnectionController {

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

    public static boolean isEnabled() {
        final AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
        return delegate != null
                && delegate.isUpdateServiceBindingApiAvailable()
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
        mConnection.rebindService(getBindFlags(bindingState));
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
        mConnection.rebindService(getBindFlags(effectiveBindingState));
        if (isUpgrading) {
            updateBindingState(effectiveBindingState);
        }
    }

    @Override
    public void setStrongBinding() {
        throw new UnsupportedOperationException();
    }

    @Override
    public void unsetStrongBinding() {
        throw new UnsupportedOperationException();
    }

    @Override
    public void setVisibleBinding() {
        throw new UnsupportedOperationException();
    }

    @Override
    public void unsetVisibleBinding() {
        throw new UnsupportedOperationException();
    }

    @Override
    public void setNotPerceptibleBinding() {
        throw new UnsupportedOperationException();
    }

    @Override
    public void unsetNotPerceptibleBinding() {
        throw new UnsupportedOperationException();
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

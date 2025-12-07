// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.BaseFeatureList;
import org.chromium.base.ChildBindingState;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import javax.annotation.concurrent.GuardedBy;

/**
 * Manages the bindings of a {@link ChildProcessConnection} using the legacy method of multiple
 * parallel bindings.
 */
@NullMarked
/* package */ class LegacyChildServiceConnectionController
        implements ChildServiceConnectionController {
    private static @Nullable RebindServiceConnection sRebindServiceConnection;

    private final ChildServiceConnectionFactory mConnectionFactory;
    // The service binding flags for the default binding (i.e. visible binding).
    private final int mDefaultBindFlags;
    // Instance named used on Android 10 and above to create separate instances from the same
    // <service> manifest declaration.
    private final @Nullable String mInstanceName;
    // ChildServiceConnectionDelegate for this class which is responsible for posting callbacks to
    // the launcher thread, if needed.
    private final ChildServiceConnectionDelegate mConnectionDelegate;
    // ChildServiceConnectionDelegate for the waived binding which is responsible for posting
    // callbacks to the launcher thread, if needed.
    private final ChildServiceConnectionDelegate mWaivedConnectionDelegate;
    private Intent mBindIntent;

    // Strong binding will make the service priority equal to the priority of the activity.
    private ChildServiceConnection mStrongBinding;

    // Visible binding will make the service priority equal to the priority of a visible process
    // while the app is in the foreground.
    // This is also used as the initial binding before any priorities are set.
    private ChildServiceConnection mVisibleBinding;

    // A not perceptible binding will make the service priority below that of a
    // perceptible process of a backgrounded app.
    private ChildServiceConnection mNotPerceptibleBinding;

    // Low priority binding maintained in the entire lifetime of the connection, i.e. between calls
    // to start() and stop().
    private ChildServiceConnection mWaivedBinding;

    // Set to true once unbind() was called.
    private boolean mUnbound;

    private final Object mBindingStateLock = new Object();

    // Binding state of this connection.
    @GuardedBy("mBindingStateLock")
    private @ChildBindingState int mBindingState;

    // Same as above except it no longer updates after |unbind()|.
    @GuardedBy("mBindingStateLock")
    private @ChildBindingState int mBindingStateCurrentOrWhenDied;

    LegacyChildServiceConnectionController(
            ChildServiceConnectionFactory connectionFactory,
            Intent bindIntent,
            int defaultBindFlags,
            ChildServiceConnectionDelegate connectionDelegate,
            ChildServiceConnectionDelegate waivedConnectionDelegate,
            @Nullable String instanceName) {
        mConnectionFactory = connectionFactory;
        mBindIntent = bindIntent;
        mDefaultBindFlags = defaultBindFlags;
        mConnectionDelegate = connectionDelegate;
        mWaivedConnectionDelegate = waivedConnectionDelegate;
        mInstanceName = instanceName;

        createBindings();
    }

    private void createBindings() {
        mVisibleBinding =
                mConnectionFactory.createConnection(
                        mBindIntent, mDefaultBindFlags, mConnectionDelegate, mInstanceName);

        int flags = mDefaultBindFlags | Context.BIND_NOT_PERCEPTIBLE;
        if (BaseFeatureList.sBackgroundNotPerceptibleBinding.isEnabled()) {
            flags |= Context.BIND_NOT_FOREGROUND;
        }
        mNotPerceptibleBinding =
                mConnectionFactory.createConnection(
                        mBindIntent, flags, mConnectionDelegate, mInstanceName);

        mStrongBinding =
                mConnectionFactory.createConnection(
                        mBindIntent,
                        mDefaultBindFlags | Context.BIND_IMPORTANT,
                        mConnectionDelegate,
                        mInstanceName);
        mWaivedBinding =
                mConnectionFactory.createConnection(
                        mBindIntent,
                        mDefaultBindFlags | Context.BIND_WAIVE_PRIORITY,
                        mWaivedConnectionDelegate,
                        mInstanceName);
    }

    @Override
    public boolean bind(@ChildBindingState int initialBindingState) {
        assert !mUnbound;

        ChildServiceConnection binding;
        switch (initialBindingState) {
            case ChildBindingState.STRONG:
                binding = mStrongBinding;
                break;
            case ChildBindingState.VISIBLE:
                binding = mVisibleBinding;
                break;
            case ChildBindingState.NOT_PERCEPTIBLE:
                binding = mNotPerceptibleBinding;
                break;
            case ChildBindingState.WAIVED:
                binding = mWaivedBinding;
                break;
            case ChildBindingState.UNBOUND:
            default:
                assert false : "Unsupported initial binding state: " + initialBindingState;
                binding = null;
        }
        if (binding == null) {
            return false;
        }
        boolean success = binding.bindServiceConnection();

        if (success) {
            if (binding != mWaivedBinding) {
                boolean result = mWaivedBinding.bindServiceConnection();
                // One binding already succeeded. Waived binding should succeed too.
                assert result;
            }

            updateBindingState();
        }
        return success;
    }

    @Override
    public void unbind() {
        mUnbound = true;
        // Update binding state to ChildBindingState.UNBOUND before unbinding
        // actual bindings below.
        updateBindingState();
        mStrongBinding.unbindServiceConnection(null);
        // We must clear shared waived binding when we unbind a waived binding.
        clearSharedWaivedBinding();
        mWaivedBinding.unbindServiceConnection(null);
        mNotPerceptibleBinding.unbindServiceConnection(null);
        mVisibleBinding.unbindServiceConnection(null);
    }

    @Override
    public void rebind() {
        // This method is for rebinding to update LRU and does not take flags.
        // The rebind with flags is only supported on U+ with RebindingController.
        assert mWaivedBinding.isBound();
        if (BaseFeatureList.sUseSharedRebindServiceConnection.isEnabled()) {
            if (sRebindServiceConnection == null) {
                sRebindServiceConnection =
                        new RebindServiceConnection(
                                BaseFeatureList.sMaxDeferredSharedRebindServiceConnection
                                        .getValue());
            }
            sRebindServiceConnection.rebind(
                    mBindIntent, mDefaultBindFlags | Context.BIND_WAIVE_PRIORITY, mInstanceName);
        } else {
            mWaivedBinding.bindServiceConnection();
        }
    }

    @Override
    public boolean updateGroupImportance(int group, int importanceInGroup) {
        return mWaivedBinding.updateGroupImportance(group, importanceInGroup);
    }

    @Override
    public void replaceService(Intent bindIntent) {
        boolean isStrongBindingBound = mStrongBinding.isBound();
        boolean isVisibleBindingBound = mVisibleBinding.isBound();
        boolean isNotPerceptibleBindingBound = mNotPerceptibleBinding.isBound();
        boolean isWaivedBindingBound = mWaivedBinding.isBound();

        retireBindings();

        mBindIntent = bindIntent;
        createBindings();

        // Expect all bindings to succeed or fail together. So early out as soon as
        // one binding fails.
        if (isStrongBindingBound) {
            if (!mStrongBinding.bindServiceConnection()) {
                return;
            }
        }
        if (isVisibleBindingBound) {
            if (!mVisibleBinding.bindServiceConnection()) {
                return;
            }
        }
        if (isNotPerceptibleBindingBound) {
            if (!mNotPerceptibleBinding.bindServiceConnection()) {
                return;
            }
        }
        if (isWaivedBindingBound) {
            mWaivedBinding.bindServiceConnection();
        }
    }

    private void retireBindings() {
        mStrongBinding.retire();
        mVisibleBinding.retire();
        mNotPerceptibleBinding.retire();
        clearSharedWaivedBinding();
        mWaivedBinding.retire();
    }

    @Override
    public void setEffectiveBindingState(@ChildBindingState int effectiveBindingState) {
        assert effectiveBindingState != ChildBindingState.UNBOUND;
        if (mUnbound) {
            return;
        }

        // Bind the new effective binding state first.
        switch (effectiveBindingState) {
            case ChildBindingState.STRONG:
                if (!mStrongBinding.isBound()) {
                    mStrongBinding.bindServiceConnection();
                }
                updateBindingState();
                break;
            case ChildBindingState.VISIBLE:
                if (!mVisibleBinding.isBound()) {
                    mVisibleBinding.bindServiceConnection();
                }
                updateBindingState();
                break;
            case ChildBindingState.NOT_PERCEPTIBLE:
                if (!mNotPerceptibleBinding.isBound()) {
                    mNotPerceptibleBinding.bindServiceConnection();
                }
                updateBindingState();
                break;
            case ChildBindingState.WAIVED:
                // do nothing
                break;
            case ChildBindingState.UNBOUND:
            default:
                assert false : "Unsupported effective binding state: " + effectiveBindingState;
        }

        // Unbind bindings that are not in the effective binding state. Unbinding is done in the
        // reverse order of binding to make the getBindingState() result consistent.
        if (effectiveBindingState != ChildBindingState.NOT_PERCEPTIBLE) {
            if (mNotPerceptibleBinding != null) {
                mNotPerceptibleBinding.unbindServiceConnection(() -> updateBindingState());
            }
        }
        if (effectiveBindingState != ChildBindingState.VISIBLE) {
            mVisibleBinding.unbindServiceConnection(() -> updateBindingState());
        }
        if (effectiveBindingState != ChildBindingState.STRONG) {
            mStrongBinding.unbindServiceConnection(() -> updateBindingState());
        }
    }

    @Override
    public void setStrongBinding() {
        mStrongBinding.bindServiceConnection();
        updateBindingState();
    }

    @Override
    public void unsetStrongBinding() {
        mStrongBinding.unbindServiceConnection(() -> updateBindingState());
    }

    @Override
    public void setVisibleBinding() {
        mVisibleBinding.bindServiceConnection();
        updateBindingState();
    }

    @Override
    public void unsetVisibleBinding() {
        mVisibleBinding.unbindServiceConnection(() -> updateBindingState());
    }

    @Override
    public void setNotPerceptibleBinding() {
        mNotPerceptibleBinding.bindServiceConnection();
        updateBindingState();
    }

    @Override
    public void unsetNotPerceptibleBinding() {
        mNotPerceptibleBinding.unbindServiceConnection(() -> updateBindingState());
    }

    @Override
    public @ChildBindingState int getBindingState() {
        // WARNING: this method can be called from a thread other than the launcher thread.
        // Note that it returns the current waived bound only state and is racy. This not really
        // preventable without changing the caller's API, short of blocking.
        synchronized (mBindingStateLock) {
            return mBindingState;
        }
    }

    @Override
    public @ChildBindingState int getBindingStateCurrentOrWhenDied() {
        // WARNING: this method can be called from a thread other than the launcher thread.
        // Note that it returns the current waived bound only state and is racy. This not really
        // preventable without changing the caller's API, short of blocking.
        synchronized (mBindingStateLock) {
            return mBindingStateCurrentOrWhenDied;
        }
    }

    private void clearSharedWaivedBinding() {
        if (sRebindServiceConnection != null) {
            sRebindServiceConnection.unbind();
        }
    }

    @Override
    public boolean isUnbound() {
        return mUnbound;
    }

    @Override
    public ChildProcessConnectionState getConnectionStateForDebugging() {
        return new ChildProcessConnectionState(
                mWaivedBinding.isBound(),
                mNotPerceptibleBinding.isBound(),
                mVisibleBinding.isBound(),
                mStrongBinding.isBound());
    }

    // Should be called any binding is bound or unbound.
    private void updateBindingState() {
        int newBindingState;
        if (mUnbound) {
            newBindingState = ChildBindingState.UNBOUND;
        } else if (mStrongBinding.isBound()) {
            newBindingState = ChildBindingState.STRONG;
        } else if (mVisibleBinding.isBound()) {
            newBindingState = ChildBindingState.VISIBLE;
        } else if (mNotPerceptibleBinding.isBound()) {
            newBindingState = ChildBindingState.NOT_PERCEPTIBLE;
        } else {
            assert mWaivedBinding.isBound();
            newBindingState = ChildBindingState.WAIVED;
        }

        synchronized (mBindingStateLock) {
            mBindingState = newBindingState;
            if (!mUnbound) {
                mBindingStateCurrentOrWhenDied = mBindingState;
            }
        }
    }
}

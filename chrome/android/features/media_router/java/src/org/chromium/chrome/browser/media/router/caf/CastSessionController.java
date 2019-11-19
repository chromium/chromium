// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.cast.ApplicationMetadata;
import com.google.android.gms.cast.Cast;
import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.framework.CastSession;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Wrapper for {@link CastSession} for Casting. */
public class CastSessionController extends BaseSessionController {
    private static final String TAG = "CafSessionCtrl";

    private List<String> mNamespaces = new ArrayList<String>();
    private CastListener mCastListener;
    private CafNotificationController mNotificationController;

    public CastSessionController(CafBaseMediaRouteProvider provider) {
        super(provider);
        mCastListener = new CastListener();
        mNotificationController = new CafNotificationController(this);
    }

    public List<String> getNamespaces() {
        return mNamespaces;
    }

    /**
     * Init nested fields for testing. The reason is that nested classes are bound to the original
     * instance instead of the spyed instance.
     */
    void initNestedFieldsForTesting() {
        mCastListener = new CastListener();
    }

    @Override
    public void attachToCastSession(CastSession session) {
        super.attachToCastSession(session);
        getSession().addCastListener(mCastListener);
        updateNamespaces();
    }

    @Override
    public void detachFromCastSession() {
        if (getSession() == null) return;

        mNamespaces.clear();
        getSession().removeCastListener(mCastListener);
        super.detachFromCastSession();
    }

    @Override
    public void onSessionEnded() {
        getMessageHandler().onSessionEnded();
        super.onSessionEnded();
    }

    @Override
    public BaseNotificationController getNotificationController() {
        return mNotificationController;
    }

    private class CastListener extends Cast.Listener {
        @Override
        public void onApplicationStatusChanged() {
            CastSessionController.this.onApplicationStatusChanged();
        }

        @Override
        public void onApplicationMetadataChanged(ApplicationMetadata metadata) {
            CastSessionController.this.onApplicationStatusChanged();
        }

        @Override
        public void onVolumeChanged() {
            CastSessionController.this.onApplicationStatusChanged();
            getMessageHandler().onVolumeChanged();
        }
    }

    private void onApplicationStatusChanged() {
        updateNamespaces();

        getMessageHandler().broadcastClientMessage(
                "update_session", getMessageHandler().buildSessionMessage());
    }

    @VisibleForTesting
    void updateNamespaces() {
        if (!isConnected()) return;

        if (getSession().getApplicationMetadata() == null
                || getSession().getApplicationMetadata().getSupportedNamespaces() == null) {
            return;
        }

        Set<String> namespacesToAdd =
                new HashSet<>(getSession().getApplicationMetadata().getSupportedNamespaces());
        Set<String> namespacesToRemove = new HashSet<String>(mNamespaces);

        namespacesToRemove.removeAll(namespacesToAdd);
        namespacesToAdd.removeAll(mNamespaces);

        for (String namespace : namespacesToRemove) unregisterNamespace(namespace);
        for (String namespace : namespacesToAdd) registerNamespace(namespace);
    }

    private void registerNamespace(String namespace) {
        assert !mNamespaces.contains(namespace);

        if (!isConnected()) return;

        try {
            getSession().setMessageReceivedCallbacks(namespace, this::onMessageReceived);
            mNamespaces.add(namespace);
        } catch (Exception e) {
            Log.e(TAG, "Failed to register namespace listener for %s", namespace, e);
        }
    }

    private void unregisterNamespace(String namespace) {
        assert mNamespaces.contains(namespace);

        if (!isConnected()) return;

        try {
            getSession().removeMessageReceivedCallbacks(namespace);
            mNamespaces.remove(namespace);
        } catch (Exception e) {
            Log.e(TAG, "Failed to remove the namespace listener for %s", namespace, e);
        }
    }

    @Override
    protected void onMessageReceived(CastDevice castDevice, String namespace, String message) {
        super.onMessageReceived(castDevice, namespace, message);
        getMessageHandler().onMessageReceived(namespace, message);
    }

    @NonNull
    private CafMessageHandler getMessageHandler() {
        return ((CafMediaRouteProvider) getProvider()).getMessageHandler();
    }
}

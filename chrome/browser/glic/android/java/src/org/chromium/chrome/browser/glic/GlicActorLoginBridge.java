// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/** Bridge between Java and native C++ for Actor Login Permissions. */
@NullMarked
public class GlicActorLoginBridge {
    private long mNativeBridge;

    public GlicActorLoginBridge(Profile profile) {
        mNativeBridge = GlicActorLoginBridgeJni.get().init(this, profile);
    }

    public void destroy() {
        if (mNativeBridge != 0) {
            GlicActorLoginBridgeJni.get().destroy(mNativeBridge);
            mNativeBridge = 0;
        }
    }

    public void getAllPermissions(Callback<List<ActorLoginPermission>> callback) {
        if (mNativeBridge == 0) return;
        GlicActorLoginBridgeJni.get().getAllPermissions(mNativeBridge, callback);
    }

    public void revokePermission(String signonRealm, String username, Callback<Boolean> callback) {
        if (mNativeBridge == 0) return;
        GlicActorLoginBridgeJni.get()
                .revokePermission(mNativeBridge, signonRealm, username, callback);
    }

    @CalledByNative
    private static List<ActorLoginPermission> createPermissionList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addPermissionToList(
            List<ActorLoginPermission> list, ActorLoginPermission permission) {
        list.add(permission);
    }

    @NativeMethods
    interface Natives {
        long init(GlicActorLoginBridge caller, Profile profile);

        void destroy(long nativeGlicActorLoginBridge);

        void getAllPermissions(
                long nativeGlicActorLoginBridge, Callback<List<ActorLoginPermission>> callback);

        void revokePermission(
                long nativeGlicActorLoginBridge,
                String signonRealm,
                String username,
                Callback<Boolean> callback);
    }
}

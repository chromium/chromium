// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.actor_login;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/** Bridge between Java and native C++ for Actor Login Permissions. */
@NullMarked
public class GlicActorLoginBridge {
    @JniType("::glic::GlicActorLoginBridge")
    interface NativeGlicActorLoginBridge extends JniTypeToken {}

    private final JniUniquePtr<NativeGlicActorLoginBridge> mNativeBridge;

    public GlicActorLoginBridge(Profile profile) {
        mNativeBridge = GlicActorLoginBridgeJni.get().init(profile);
    }

    public void destroy() {
        mNativeBridge.destroy();
    }

    public void getAllPermissions(Callback<List<ActorLoginPermission>> callback) {
        GlicActorLoginBridgeJni.get().getAllPermissions(mNativeBridge, callback);
    }

    public void revokePermission(String signonRealm, String username, Callback<Boolean> callback) {
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
        JniUniquePtr<NativeGlicActorLoginBridge> init(Profile profile);

        void getAllPermissions(
                JniPtr<NativeGlicActorLoginBridge> self,
                Callback<List<ActorLoginPermission>> callback);

        void revokePermission(
                JniPtr<NativeGlicActorLoginBridge> self,
                @JniType("std::string") String signonRealm,
                @JniType("std::string") String username,
                Callback<Boolean> callback);
    }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ANDROID_GLIC_ACTOR_LOGIN_BRIDGE_H_
#define CHROME_BROWSER_GLIC_ANDROID_GLIC_ACTOR_LOGIN_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/ui/actor_login_permission.h"

class Profile;

namespace actor_login {
class ActorLoginPermissionsManager;
}

namespace glic {

class GlicActorLoginBridge {
 public:
  GlicActorLoginBridge(JNIEnv* env,
                       const base::android::JavaRef<jobject>& obj,
                       Profile* profile);
  ~GlicActorLoginBridge();

  GlicActorLoginBridge(const GlicActorLoginBridge&) = delete;
  GlicActorLoginBridge& operator=(const GlicActorLoginBridge&) = delete;

  void Destroy(JNIEnv* env);

  void GetAllPermissions(JNIEnv* env,
                         const base::android::JavaRef<jobject>& jcallback);

  void RevokePermission(JNIEnv* env,
                        const base::android::JavaRef<jstring>& j_signon_realm,
                        const base::android::JavaRef<jstring>& j_username,
                        const base::android::JavaRef<jobject>& jcallback);

 private:
  void OnGetAllPermissionsComplete(
      base::android::ScopedJavaGlobalRef<jobject> jcallback,
      base::flat_set<password_manager::ActorLoginPermission> permissions);

  void OnRevokePermissionComplete(
      base::android::ScopedJavaGlobalRef<jobject> jcallback,
      bool success);

  raw_ptr<Profile> profile_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  std::unique_ptr<actor_login::ActorLoginPermissionsManager> manager_;
  base::WeakPtrFactory<GlicActorLoginBridge> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ANDROID_GLIC_ACTOR_LOGIN_BRIDGE_H_

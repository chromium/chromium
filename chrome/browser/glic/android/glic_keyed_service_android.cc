// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_keyed_service.h"

#include "base/logging.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/glic/android/jni_headers/GlicKeyedService_jni.h"

namespace glic {

static void JNI_GlicKeyedService_ToggleUI(JNIEnv* env,
                                          long browser_window_ptr,
                                          Profile* profile,
                                          int32_t source) {
  CHECK(profile);
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile, /*create=*/true);
  if (!service) {
    LOG(ERROR) << "GlicKeyedService not available for this profile.";
    return;
  }

  BrowserWindowInterface* bwi =
      reinterpret_cast<BrowserWindowInterface*>(browser_window_ptr);
  CHECK(bwi);
  service->ToggleUI(bwi, false, static_cast<mojom::InvocationSource>(source));
}

}  // namespace glic

DEFINE_JNI(GlicKeyedService)

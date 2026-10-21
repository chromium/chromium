// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/extension_control_handler_android.h"

#include "base/android/jni_string.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/search_engines/android/jni_headers/ExtensionControlHandler_jni.h"

static jni_zero::JniUniquePtr<ExtensionControlHandler>
JNI_ExtensionControlHandler_Init(JNIEnv* env, Profile* profile) {
  return jni_zero::MakeUnique<ExtensionControlHandler>(profile);
}

ExtensionControlHandler::ExtensionControlHandler(Profile* profile)
    : profile_(profile) {}

ExtensionControlHandler::~ExtensionControlHandler() = default;

void ExtensionControlHandler::DisableExtension(JNIEnv* env,
                                               std::string extension_id) {
  auto* extension_registrar = extensions::ExtensionRegistrar::Get(profile_);
  extension_registrar->DisableExtension(
      extension_id, {extensions::disable_reason::DISABLE_USER_ACTION});
}

DEFINE_JNI(ExtensionControlHandler)

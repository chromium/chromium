// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/messages/jni_headers/MessagesResourceMapperInitializer_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/messages/android/message_dispatcher_bridge.h"

// Sets up a callback for MessageDispatcherBridge ResourceIdMapper. This is done
// in chrome/browser/android because ResourceMapper is not available in
// components.
void JNI_MessagesResourceMapperInitializer_Init(JNIEnv* env) {
  messages::MessageDispatcherBridge::Get()->SetResourceIdMapper(
      base::BindRepeating(&ResourceMapper::MapToJavaDrawableId));
}

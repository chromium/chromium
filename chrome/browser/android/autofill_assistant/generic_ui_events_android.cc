// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/generic_ui_events_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantViewEvents_jni.h"
#include "chrome/browser/android/autofill_assistant/view_handler_android.h"

namespace autofill_assistant {
namespace android_events {

namespace {

void SetOnClickListener(JNIEnv* env,
                        base::android::ScopedJavaGlobalRef<jobject> jview,
                        base::android::ScopedJavaGlobalRef<jobject> jdelegate,
                        const OnViewClickedEventProto& proto) {
  Java_AssistantViewEvents_setOnClickListener(
      env, jview,
      base::android::ConvertUTF8ToJavaString(env, proto.view_identifier()),
      jdelegate);
}

}  // namespace

bool CreateJavaListenersFromProto(
    JNIEnv* env,
    ViewHandlerAndroid* view_handler,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate,
    const InteractionsProto& proto) {
  for (const auto& interaction_proto : proto.interactions()) {
    for (const auto& event_proto : interaction_proto.trigger_event()) {
      switch (event_proto.kind_case()) {
        case EventProto::kOnViewClicked: {
          auto jview = view_handler->GetView(
              event_proto.on_view_clicked().view_identifier());
          if (!jview.has_value()) {
            VLOG(1) << "Invalid click event, no view with id='"
                    << event_proto.on_view_clicked().view_identifier()
                    << "' found";
            return false;
          }
          SetOnClickListener(env, *jview, jdelegate,
                             event_proto.on_view_clicked());
          break;
        }
        case EventProto::kOnValueChanged:
        case EventProto::kOnUserActionCalled:
        case EventProto::kOnTextLinkClicked:
        case EventProto::kOnPopupDismissed:
        case EventProto::kOnViewContainerCleared:
          // Skip events that do not require registering java-side listeners.
          break;
        case EventProto::KIND_NOT_SET:
          VLOG(1)
              << "Error creating java listener for trigger event: kind not set";
          return false;
      }
    }
  }
  return true;
}

}  // namespace android_events
}  // namespace autofill_assistant

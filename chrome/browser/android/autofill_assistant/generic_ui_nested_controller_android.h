// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_GENERIC_UI_NESTED_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_GENERIC_UI_NESTED_CONTROLLER_ANDROID_H_

#include <map>
#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
class InteractionHandlerAndroid;
class BasicInteractions;
class EventHandler;
class RadioButtonController;
class UserModel;
class ViewHandlerAndroid;

class GenericUiNestedControllerAndroid {
 public:
  // Attempts to creata a new instance. May fail if the proto is invalid.
  // Arguments must outlive this instance. Ownership of the arguments is not
  // changed.
  static std::unique_ptr<GenericUiNestedControllerAndroid> CreateFromProto(
      const GenericUserInterfaceProto& proto,
      base::android::ScopedJavaGlobalRef<jobject> jcontext,
      base::android::ScopedJavaGlobalRef<jobject> jdelegate,
      EventHandler* event_handler,
      UserModel* user_model,
      BasicInteractions* basic_interactions,
      RadioButtonController* radio_button_controller);

  base::android::ScopedJavaGlobalRef<jobject> GetRootView() const {
    return jroot_view_;
  }

  GenericUiNestedControllerAndroid(
      base::android::ScopedJavaGlobalRef<jobject> jroot_view,
      std::unique_ptr<ViewHandlerAndroid> view_handler,
      std::unique_ptr<InteractionHandlerAndroid> interaction_handler,
      RadioButtonController* radio_button_controller,
      const std::vector<std::pair<std::string, std::string>>& radio_buttons);
  ~GenericUiNestedControllerAndroid();
  GenericUiNestedControllerAndroid(const GenericUiNestedControllerAndroid&) =
      delete;
  GenericUiNestedControllerAndroid& operator=(
      GenericUiNestedControllerAndroid&) = delete;

 private:
  base::android::ScopedJavaGlobalRef<jobject> jroot_view_;
  std::unique_ptr<ViewHandlerAndroid> view_handler_;
  std::unique_ptr<InteractionHandlerAndroid> interaction_handler_;
  RadioButtonController* radio_button_controller_ = nullptr;
  std::vector<std::pair<std::string, std::string>> radio_buttons_;
};

}  //  namespace autofill_assistant

#endif  //  CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_GENERIC_UI_NESTED_CONTROLLER_ANDROID_H_

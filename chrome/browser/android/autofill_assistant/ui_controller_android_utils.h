// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_UTILS_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_UTILS_H_

#include <map>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/bottom_sheet_state.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/view_layout.pb.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace ui_controller_android_utils {

// Returns a 32-bit Integer representing |color_string| in Java, or null if
// |color_string| is invalid.
// TODO(806868): Get rid of this overload and always use GetJavaColor(proto).
base::android::ScopedJavaLocalRef<jobject> GetJavaColor(
    JNIEnv* env,
    const std::string& color_string);

// Returns a 32-bit Integer representing |proto| in Java, or null if
// |proto| is invalid.
base::android::ScopedJavaLocalRef<jobject> GetJavaColor(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const ColorProto& proto);

// Returns the pixelsize of |proto| in |jcontext|, or |nullopt| if |proto| is
// invalid.
base::Optional<int> GetPixelSize(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const ClientDimensionProto& proto);

// Returns the pixelsize of |proto| in |jcontext|, or |default_value| if |proto|
// is invalid.
int GetPixelSizeOrDefault(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const ClientDimensionProto& proto,
    int default_value);

// Returns an instance of an |AssistantDrawable| or nullptr if it could not
// be created.
base::android::ScopedJavaLocalRef<jobject> CreateJavaDrawable(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const DrawableProto& proto,
    const UserModel* user_model = nullptr);

// Returns the java equivalent of |proto|.
base::android::ScopedJavaLocalRef<jobject> ToJavaValue(JNIEnv* env,
                                                       const ValueProto& proto);

// Returns the native equivalent of |jvalue|. Returns an empty ValueProto if
// |jvalue| is null.
ValueProto ToNativeValue(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jvalue);

// Returns an instance of |AssistantInfoPopup| for |proto|.
base::android::ScopedJavaLocalRef<jobject> CreateJavaInfoPopup(
    JNIEnv* env,
    const InfoPopupProto& proto);

// Shows an instance of |AssistantInfoPopup| on the screen.
void ShowJavaInfoPopup(JNIEnv* env,
                       base::android::ScopedJavaLocalRef<jobject> jinfo_popup,
                       base::android::ScopedJavaLocalRef<jobject> jcontext);

// Converts a java string to native. Returns an empty string if input is null.
std::string SafeConvertJavaStringToNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jstring);

// Creates a BottomSheetState from the Android SheetState enum defined in
// components/browser_ui/bottomsheet/BottomSheetController.java.
BottomSheetState ToNativeBottomSheetState(int state);

// Converts a BottomSheetState to the Android SheetState enum.
int ToJavaBottomSheetState(BottomSheetState state);

}  // namespace ui_controller_android_utils
}  //  namespace autofill_assistant

#endif  //  CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_UTILS_H_

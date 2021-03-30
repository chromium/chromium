// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/ui_controller_android_utils.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/notreached.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantChip_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantColor_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantDateTime_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantDialogButton_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantDimension_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantDrawable_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantInfoPopup_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantValue_jni.h"
#include "components/autofill_assistant/browser/generic_ui_java_generated_enums.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {
namespace ui_controller_android_utils {
namespace {

DrawableIcon MapDrawableIcon(DrawableProto::Icon icon) {
  switch (icon) {
    case DrawableProto::DRAWABLE_ICON_UNDEFINED:
      return DrawableIcon::DRAWABLE_ICON_UNDEFINED;
    case DrawableProto::PROGRESSBAR_DEFAULT_INITIAL_STEP:
      return DrawableIcon::PROGRESSBAR_DEFAULT_INITIAL_STEP;
    case DrawableProto::PROGRESSBAR_DEFAULT_DATA_COLLECTION:
      return DrawableIcon::PROGRESSBAR_DEFAULT_DATA_COLLECTION;
    case DrawableProto::PROGRESSBAR_DEFAULT_PAYMENT:
      return DrawableIcon::PROGRESSBAR_DEFAULT_PAYMENT;
    case DrawableProto::PROGRESSBAR_DEFAULT_FINAL_STEP:
      return DrawableIcon::PROGRESSBAR_DEFAULT_FINAL_STEP;
    case DrawableProto::SITTING_PERSON:
      return DrawableIcon::SITTING_PERSON;
    case DrawableProto::TICKET_STUB:
      return DrawableIcon::TICKET_STUB;
    case DrawableProto::SHOPPING_BASKET:
      return DrawableIcon::SHOPPING_BASKET;
    case DrawableProto::FAST_FOOD:
      return DrawableIcon::FAST_FOOD;
    case DrawableProto::LOCAL_DINING:
      return DrawableIcon::LOCAL_DINING;
    case DrawableProto::COGWHEEL:
      return DrawableIcon::COGWHEEL;
    case DrawableProto::KEY:
      return DrawableIcon::KEY;
    case DrawableProto::CAR:
      return DrawableIcon::CAR;
    case DrawableProto::GROCERY:
      return DrawableIcon::GROCERY;
    case DrawableProto::VISIBILITY_ON:
      return DrawableIcon::VISIBILITY_ON;
    case DrawableProto::VISIBILITY_OFF:
      return DrawableIcon::VISIBILITY_OFF;
  }
}

}  // namespace

base::android::ScopedJavaLocalRef<jobject> GetJavaColor(
    JNIEnv* env,
    const std::string& color_string) {
  if (!Java_AssistantColor_isValidColorString(
          env, base::android::ConvertUTF8ToJavaString(env, color_string))) {
    if (!color_string.empty()) {
      VLOG(1) << "Encountered invalid color string: " << color_string;
    }
    return nullptr;
  }

  return Java_AssistantColor_createFromString(
      env, base::android::ConvertUTF8ToJavaString(env, color_string));
}

base::android::ScopedJavaLocalRef<jobject> GetJavaColor(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const ColorProto& proto) {
  switch (proto.color_case()) {
    case ColorProto::kResourceIdentifier:
      if (!Java_AssistantColor_isValidResourceIdentifier(
              env, jcontext,
              base::android::ConvertUTF8ToJavaString(
                  env, proto.resource_identifier()))) {
        VLOG(1) << "Encountered invalid color resource identifier: "
                << proto.resource_identifier();
        return nullptr;
      }
      return Java_AssistantColor_createFromResource(
          env, jcontext,
          base::android::ConvertUTF8ToJavaString(env,
                                                 proto.resource_identifier()));
    case ColorProto::kParseableColor:
      return GetJavaColor(env, proto.parseable_color());
    case ColorProto::COLOR_NOT_SET:
      return nullptr;
  }
}

base::Optional<int> GetPixelSize(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const ClientDimensionProto& proto) {
  switch (proto.size_case()) {
    case ClientDimensionProto::kDp:
      return Java_AssistantDimension_getPixelSizeDp(env, jcontext, proto.dp());
    case ClientDimensionProto::kWidthFactor:
      return Java_AssistantDimension_getPixelSizeWidthFactor(
          env, jcontext, proto.width_factor());
    case ClientDimensionProto::kHeightFactor:
      return Java_AssistantDimension_getPixelSizeHeightFactor(
          env, jcontext, proto.height_factor());
    case ClientDimensionProto::kSizeInPixel:
      return proto.size_in_pixel();
    case ClientDimensionProto::SIZE_NOT_SET:
      return base::nullopt;
  }
}

int GetPixelSizeOrDefault(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const ClientDimensionProto& proto,
    int default_value) {
  auto size = GetPixelSize(env, jcontext, proto);
  if (size) {
    return *size;
  }
  return default_value;
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaDrawable(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const DrawableProto& proto,
    const UserModel* user_model) {
  switch (proto.drawable_case()) {
    case DrawableProto::kResourceIdentifier:
      if (!Java_AssistantDrawable_isValidDrawableResource(
              env, jcontext,
              base::android::ConvertUTF8ToJavaString(
                  env, proto.resource_identifier()))) {
        VLOG(1) << "Encountered invalid drawable resource identifier: "
                << proto.resource_identifier();
        return nullptr;
      }
      return Java_AssistantDrawable_createFromResource(
          env, base::android::ConvertUTF8ToJavaString(
                   env, proto.resource_identifier()));
    case DrawableProto::kBitmap: {
      int width_pixels = ui_controller_android_utils::GetPixelSizeOrDefault(
          env, jcontext, proto.bitmap().width(), 0);
      int height_pixels = ui_controller_android_utils::GetPixelSizeOrDefault(
          env, jcontext, proto.bitmap().height(), 0);
      return Java_AssistantDrawable_createFromUrl(
          env,
          base::android::ConvertUTF8ToJavaString(env, proto.bitmap().url()),
          width_pixels, height_pixels);
    }
    case DrawableProto::kShape: {
      switch (proto.shape().shape_case()) {
        case ShapeDrawableProto::kRectangle: {
          auto jbackground_color = ui_controller_android_utils::GetJavaColor(
              env, jcontext, proto.shape().background_color());
          auto jstroke_color = ui_controller_android_utils::GetJavaColor(
              env, jcontext, proto.shape().stroke_color());
          int stroke_width_pixels =
              ui_controller_android_utils::GetPixelSizeOrDefault(
                  env, jcontext, proto.shape().stroke_width(), 0);
          int corner_radius_pixels =
              ui_controller_android_utils::GetPixelSizeOrDefault(
                  env, jcontext, proto.shape().rectangle().corner_radius(), 0);
          return Java_AssistantDrawable_createRectangleShape(
              env, jbackground_color, jstroke_color, stroke_width_pixels,
              corner_radius_pixels);
          break;
        }
        case ShapeDrawableProto::SHAPE_NOT_SET:
          return nullptr;
      }
    }
    case DrawableProto::kIcon: {
      return Java_AssistantDrawable_createFromIcon(
          env, static_cast<int>(MapDrawableIcon(proto.icon())));
    }
    case DrawableProto::kBase64: {
      return Java_AssistantDrawable_createFromBase64(
          env, base::android::ToJavaByteArray(env, proto.base64()));
    }
    case DrawableProto::kFavicon: {
      if (!user_model) {
        VLOG(1) << "User model missing while trying to create a favicon.";
        return nullptr;
      }
      int diameter_size_in_pixel =
          ui_controller_android_utils::GetPixelSizeOrDefault(
              env, jcontext, proto.favicon().diameter_size(), 0);
      std::string url = proto.favicon().has_website_url()
                            ? proto.favicon().website_url()
                            : user_model->GetCurrentURL().spec();
      return Java_AssistantDrawable_createFromFavicon(
          env, base::android::ConvertUTF8ToJavaString(env, url),
          diameter_size_in_pixel, proto.favicon().force_monogram());
    }
    case DrawableProto::DRAWABLE_NOT_SET:
      return nullptr;
  }
}

base::android::ScopedJavaLocalRef<jobject> ToJavaValue(
    JNIEnv* env,
    const ValueProto& proto) {
  switch (proto.kind_case()) {
    case ValueProto::kStrings: {
      std::vector<std::string> strings;
      strings.reserve(proto.strings().values_size());
      for (const auto& string : proto.strings().values()) {
        strings.push_back(string);
      }
      return Java_AssistantValue_createForStrings(
          env, base::android::ToJavaArrayOfStrings(env, strings));
    }
    case ValueProto::kBooleans: {
      auto booleans = std::make_unique<bool[]>(proto.booleans().values_size());
      for (int i = 0; i < proto.booleans().values_size(); ++i) {
        booleans[i] = proto.booleans().values()[i];
      }
      return Java_AssistantValue_createForBooleans(
          env, base::android::ToJavaBooleanArray(
                   env, booleans.get(), proto.booleans().values_size()));
    }
    case ValueProto::kInts: {
      auto ints = std::make_unique<int[]>(proto.ints().values_size());
      for (int i = 0; i < proto.ints().values_size(); ++i) {
        ints[i] = proto.ints().values()[i];
      }
      return Java_AssistantValue_createForIntegers(
          env, base::android::ToJavaIntArray(env, ints.get(),
                                             proto.ints().values_size()));
    }
    case ValueProto::kCreditCards:
    case ValueProto::kProfiles:
    case ValueProto::kLoginOptions:
    case ValueProto::kCreditCardResponse:
    case ValueProto::kServerPayload:
    case ValueProto::kUserActions:
      // Unused.
      NOTREACHED();
      return nullptr;
    case ValueProto::kDates: {
      auto jlist = Java_AssistantValue_createDateTimeList(env);
      for (const auto& value : proto.dates().values()) {
        Java_AssistantValue_addDateTimeToList(
            env, jlist,
            Java_AssistantDateTime_Constructor(
                env, static_cast<int>(value.year()), value.month(), value.day(),
                0, 0, 0));
      }
      return Java_AssistantValue_createForDateTimes(env, jlist);
    }
    case ValueProto::KIND_NOT_SET:
      return Java_AssistantValue_create(env);
  }
}

ValueProto ToNativeValue(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jvalue) {
  ValueProto proto;
  if (!jvalue) {
    return proto;
  }
  auto jints = Java_AssistantValue_getIntegers(env, jvalue);
  if (jints) {
    auto* mutable_ints = proto.mutable_ints();
    std::vector<int> ints;
    base::android::JavaIntArrayToIntVector(env, jints, &ints);
    for (int i : ints) {
      mutable_ints->add_values(i);
    }
    return proto;
  }

  auto jbooleans = Java_AssistantValue_getBooleans(env, jvalue);
  if (jbooleans) {
    auto* mutable_booleans = proto.mutable_booleans();
    std::vector<bool> booleans;
    base::android::JavaBooleanArrayToBoolVector(env, jbooleans, &booleans);
    for (auto b : booleans) {
      mutable_booleans->add_values(b);
    }
    return proto;
  }

  auto jstrings = Java_AssistantValue_getStrings(env, jvalue);
  if (jstrings) {
    auto* mutable_strings = proto.mutable_strings();
    std::vector<std::string> strings;
    base::android::AppendJavaStringArrayToStringVector(env, jstrings, &strings);
    for (const auto& string : strings) {
      mutable_strings->add_values(string);
    }
    return proto;
  }

  auto jdatetimes = Java_AssistantValue_getDateTimes(env, jvalue);
  if (jdatetimes) {
    auto* mutable_dates = proto.mutable_dates();
    for (int i = 0; i < Java_AssistantValue_getListSize(env, jdatetimes); ++i) {
      auto jvalue = Java_AssistantValue_getListAt(env, jdatetimes, i);
      DateProto date;
      date.set_year(Java_AssistantDateTime_getYear(env, jvalue));
      date.set_month(Java_AssistantDateTime_getMonth(env, jvalue));
      date.set_day(Java_AssistantDateTime_getDay(env, jvalue));
      *mutable_dates->add_values() = date;
    }
    return proto;
  }

  return proto;
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaDialogButton(
    JNIEnv* env,
    const InfoPopupProto_DialogButton& button_proto) {
  base::android::ScopedJavaLocalRef<jstring> jurl = nullptr;

  switch (button_proto.click_action_case()) {
    case InfoPopupProto::DialogButton::kOpenUrlInCct:
      jurl = base::android::ConvertUTF8ToJavaString(
          env, button_proto.open_url_in_cct().url());
      break;
    case InfoPopupProto::DialogButton::kCloseDialog:
      break;
    case InfoPopupProto::DialogButton::CLICK_ACTION_NOT_SET:
      NOTREACHED();
      break;
  }
  return Java_AssistantDialogButton_Constructor(
      env, base::android::ConvertUTF8ToJavaString(env, button_proto.label()),
      jurl);
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaInfoPopup(
    JNIEnv* env,
    const InfoPopupProto& info_popup_proto) {
  base::android::ScopedJavaLocalRef<jobject> jpositive_button = nullptr;
  base::android::ScopedJavaLocalRef<jobject> jnegative_button = nullptr;
  base::android::ScopedJavaLocalRef<jobject> jneutral_button = nullptr;

  if (info_popup_proto.has_positive_button() ||
      info_popup_proto.has_negative_button() ||
      info_popup_proto.has_neutral_button()) {
    if (info_popup_proto.has_positive_button()) {
      jpositive_button =
          CreateJavaDialogButton(env, info_popup_proto.positive_button());
    }
    if (info_popup_proto.has_negative_button()) {
      jnegative_button =
          CreateJavaDialogButton(env, info_popup_proto.negative_button());
    }
    if (info_popup_proto.has_neutral_button()) {
      jneutral_button =
          CreateJavaDialogButton(env, info_popup_proto.neutral_button());
    }
  } else {
    // If no button is set in the proto, we add a Close button
    jpositive_button = Java_AssistantDialogButton_Constructor(
        env,
        base::android::ConvertUTF8ToJavaString(
            env, l10n_util::GetStringUTF8(IDS_CLOSE)),
        nullptr);
  }

  return Java_AssistantInfoPopup_Constructor(
      env,
      base::android::ConvertUTF8ToJavaString(env, info_popup_proto.title()),
      base::android::ConvertUTF8ToJavaString(env, info_popup_proto.text()),
      jpositive_button, jnegative_button, jneutral_button);
}

void ShowJavaInfoPopup(JNIEnv* env,
                       base::android::ScopedJavaLocalRef<jobject> jinfo_popup,
                       base::android::ScopedJavaLocalRef<jobject> jcontext) {
  Java_AssistantInfoPopup_show(env, jinfo_popup, jcontext);
}

std::string SafeConvertJavaStringToNative(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& jstring) {
  std::string native_string;
  if (jstring) {
    native_string = base::android::ConvertJavaStringToUTF8(env, jstring);
  }
  return native_string;
}

BottomSheetState ToNativeBottomSheetState(int state) {
  switch (state) {
    case 1:
      return BottomSheetState::COLLAPSED;
    case 2:
    case 3:
      return BottomSheetState::EXPANDED;
    default:
      return BottomSheetState::UNDEFINED;
  }
}

int ToJavaBottomSheetState(BottomSheetState state) {
  switch (state) {
    case BottomSheetState::COLLAPSED:
      return 1;
    case BottomSheetState::UNDEFINED:
      // The current assumption is that Autobot always starts with the bottom
      // sheet expanded.
    case BottomSheetState::EXPANDED:
      return 2;
    default:
      return -1;
  }
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaAssistantChip(
    JNIEnv* env,
    const ChipProto& chip) {
  switch (chip.type()) {
    default:  // Other chip types are not supported.
      return nullptr;

    case HIGHLIGHTED_ACTION:
    case DONE_ACTION:
      return Java_AssistantChip_createHighlightedAssistantChip(
          env, chip.icon(),
          base::android::ConvertUTF8ToJavaString(env, chip.text()),
          /* disabled = */ false, chip.sticky(), /* visible = */ true,
          chip.has_content_description()
              ? base::android::ConvertUTF8ToJavaString(
                    env, chip.content_description())
              : nullptr);

    case NORMAL_ACTION:
    case CANCEL_ACTION:
    case CLOSE_ACTION:
    case FEEDBACK_ACTION:
      return Java_AssistantChip_createHairlineAssistantChip(
          env, chip.icon(),
          base::android::ConvertUTF8ToJavaString(env, chip.text()),
          /* disabled = */ false, chip.sticky(), /* visible = */ true,
          chip.has_content_description()
              ? base::android::ConvertUTF8ToJavaString(
                    env, chip.content_description())
              : nullptr);
  }
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaAssistantChipList(
    JNIEnv* env,
    const std::vector<ChipProto>& chips) {
  auto jlist = Java_AssistantChip_createChipList(env);
  for (const auto& chip : chips) {
    auto jchip = CreateJavaAssistantChip(env, chip);
    if (!jchip) {
      return nullptr;
    }
    Java_AssistantChip_addChipToList(env, jlist, jchip);
  }
  return jlist;
}

std::map<std::string, std::string> CreateStringMapFromJava(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& names,
    const base::android::JavaRef<jobjectArray>& values) {
  std::vector<std::string> names_vector;
  base::android::AppendJavaStringArrayToStringVector(env, names, &names_vector);
  std::vector<std::string> values_vector;
  base::android::AppendJavaStringArrayToStringVector(env, values,
                                                     &values_vector);
  std::map<std::string, std::string> result;
  DCHECK_EQ(names_vector.size(), values_vector.size());
  for (size_t i = 0; i < names_vector.size(); ++i) {
    result.insert(std::make_pair(names_vector[i], values_vector[i]));
  }
  return result;
}

std::unique_ptr<TriggerContext> CreateTriggerContext(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& jexperiment_ids,
    const base::android::JavaRef<jobjectArray>& jparameter_names,
    const base::android::JavaRef<jobjectArray>& jparameter_values,
    jboolean is_cct,
    jboolean onboarding_shown,
    jboolean is_direct_action,
    const base::android::JavaRef<jstring>& jinitial_url) {
  return std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(
          CreateStringMapFromJava(env, jparameter_names, jparameter_values)),
      SafeConvertJavaStringToNative(env, jexperiment_ids), is_cct,
      onboarding_shown, is_direct_action,
      SafeConvertJavaStringToNative(env, jinitial_url));
}

}  // namespace ui_controller_android_utils
}  // namespace autofill_assistant

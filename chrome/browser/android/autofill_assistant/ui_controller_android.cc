// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"

#include <map>
#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantCollectUserDataModel_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantDetailsModel_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantDetails_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantFormInput_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantFormModel_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantGenericUiModel_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantHeaderModel_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantInfoBoxModel_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantInfoBox_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantModel_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantOverlayModel_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AutofillAssistantUiController_jni.h"
#include "chrome/browser/android/autofill_assistant/generic_ui_root_controller_android.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android_utils.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/bottom_sheet_state.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/event_handler.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace autofill_assistant {

namespace {

static const char* const kCancelChipIdentifier = "CANCEL_CHIP_ID";

std::vector<float> ToFloatVector(const std::vector<RectF>& areas) {
  std::vector<float> flattened;
  for (const auto& rect : areas) {
    flattened.emplace_back(rect.left);
    flattened.emplace_back(rect.top);
    flattened.emplace_back(rect.right);
    flattened.emplace_back(rect.bottom);
  }
  return flattened;
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaDateTime(
    JNIEnv* env,
    const DateTimeProto& proto) {
  return Java_AssistantCollectUserDataModel_createAssistantDateTime(
      env, (int)proto.date().year(), proto.date().month(), proto.date().day(),
      proto.time().hour(), proto.time().minute(), proto.time().second());
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaDate(
    JNIEnv* env,
    const DateProto& proto) {
  DateTimeProto date_time;
  *date_time.mutable_date() = proto;
  return CreateJavaDateTime(env, date_time);
}

// Creates the Java equivalent to |login_choices|.
base::android::ScopedJavaLocalRef<jobject> CreateJavaLoginChoiceList(
    JNIEnv* env,
    const std::vector<LoginChoice>& login_choices) {
  auto jlist = Java_AssistantCollectUserDataModel_createLoginChoiceList(env);
  for (const auto& login_choice : login_choices) {
    base::android::ScopedJavaLocalRef<jobject> jinfo_popup = nullptr;
    if (login_choice.info_popup.has_value()) {
      jinfo_popup = ui_controller_android_utils::CreateJavaInfoPopup(
          env, *login_choice.info_popup);
    }
    base::android::ScopedJavaLocalRef<jstring> jsublabel_accessibility_hint =
        nullptr;
    if (login_choice.sublabel_accessibility_hint.has_value()) {
      jsublabel_accessibility_hint = base::android::ConvertUTF8ToJavaString(
          env, login_choice.sublabel_accessibility_hint.value());
    }
    Java_AssistantCollectUserDataModel_addLoginChoice(
        env, jlist,
        base::android::ConvertUTF8ToJavaString(env, login_choice.identifier),
        base::android::ConvertUTF8ToJavaString(env, login_choice.label),
        base::android::ConvertUTF8ToJavaString(env, login_choice.sublabel),
        jsublabel_accessibility_hint, login_choice.preselect_priority,
        jinfo_popup);
  }
  return jlist;
}

// Creates the java equivalent to the text inputs specified in |section|.
base::android::ScopedJavaLocalRef<jobject> CreateJavaTextInputsForSection(
    JNIEnv* env,
    const TextInputSectionProto& section) {
  auto jinput_list =
      Java_AssistantCollectUserDataModel_createTextInputList(env);
  for (const auto& input : section.input_fields()) {
    TextInputType type;
    switch (input.input_type()) {
      case TextInputProto::INPUT_TEXT:
        type = TextInputType::INPUT_TEXT;
        break;
      case TextInputProto::INPUT_ALPHANUMERIC:
        type = TextInputType::INPUT_ALPHANUMERIC;
        break;
      case TextInputProto::UNDEFINED:
        NOTREACHED();
        continue;
    }
    Java_AssistantCollectUserDataModel_appendTextInput(
        env, jinput_list, type,
        base::android::ConvertUTF8ToJavaString(env, input.hint()),
        base::android::ConvertUTF8ToJavaString(env, input.value()),
        base::android::ConvertUTF8ToJavaString(env, input.client_memory_key()));
  }
  return jinput_list;
}

// Creates the java equivalent to |sections|.
base::android::ScopedJavaLocalRef<jobject> CreateJavaAdditionalSections(
    JNIEnv* env,
    const std::vector<UserFormSectionProto>& sections) {
  auto jsection_list =
      Java_AssistantCollectUserDataModel_createAdditionalSectionsList(env);
  for (const auto& section : sections) {
    switch (section.section_case()) {
      case UserFormSectionProto::kStaticTextSection:
        Java_AssistantCollectUserDataModel_appendStaticTextSection(
            env, jsection_list,
            base::android::ConvertUTF8ToJavaString(env, section.title()),
            base::android::ConvertUTF8ToJavaString(
                env, section.static_text_section().text()));
        break;
      case UserFormSectionProto::kTextInputSection: {
        Java_AssistantCollectUserDataModel_appendTextInputSection(
            env, jsection_list,
            base::android::ConvertUTF8ToJavaString(env, section.title()),
            CreateJavaTextInputsForSection(env, section.text_input_section()));
        break;
      }
      case UserFormSectionProto::kPopupListSection: {
        std::vector<std::string> items;
        std::copy(section.popup_list_section().item_names().begin(),
                  section.popup_list_section().item_names().end(),
                  std::back_inserter(items));
        std::vector<int> initial_selections;
        std::copy(section.popup_list_section().initial_selection().begin(),
                  section.popup_list_section().initial_selection().end(),
                  std::back_inserter(initial_selections));
        Java_AssistantCollectUserDataModel_appendPopupListSection(
            env, jsection_list,
            base::android::ConvertUTF8ToJavaString(env, section.title()),
            base::android::ConvertUTF8ToJavaString(
                env, section.popup_list_section().additional_value_key()),
            base::android::ToJavaArrayOfStrings(env, items),
            base::android::ToJavaIntArray(env, initial_selections),
            section.popup_list_section().allow_multiselect(),
            section.popup_list_section().selection_mandatory(),
            base::android::ConvertUTF8ToJavaString(
                env,
                section.popup_list_section().no_selection_error_message()));
        break;
      }
      case UserFormSectionProto::SECTION_NOT_SET:
        NOTREACHED();
        break;
    }
  }
  return jsection_list;
}

base::Optional<int> GetPreviousFormCounterResult(
    const FormProto::Result* result,
    int input_index,
    int counter_index) {
  if (result == nullptr) {
    return base::nullopt;
  }

  if (input_index >= result->input_results().size()) {
    return base::nullopt;
  }
  auto input_result = result->input_results(input_index);

  if (counter_index >= input_result.counter().values().size()) {
    return base::nullopt;
  }
  return input_result.counter().values(counter_index);
}

base::Optional<bool> GetPreviousFormSelectionResult(
    const FormProto::Result* result,
    int input_index,
    int selection_index) {
  if (result == nullptr) {
    return base::nullopt;
  }

  if (input_index >= result->input_results().size()) {
    return base::nullopt;
  }
  auto input_result = result->input_results(input_index);

  if (selection_index >= input_result.selection().selected().size()) {
    return base::nullopt;
  }
  return input_result.selection().selected(selection_index);
}

bool ShouldAllowSoftKeyboardForState(AutofillAssistantState state) {
  switch (state) {
    case AutofillAssistantState::STARTING:
    case AutofillAssistantState::RUNNING:
      return false;

    case AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT:
    case AutofillAssistantState::PROMPT:
    case AutofillAssistantState::BROWSE:
    case AutofillAssistantState::MODAL_DIALOG:
    case AutofillAssistantState::STOPPED:
    case AutofillAssistantState::TRACKING:
    case AutofillAssistantState::INACTIVE:
      return true;
  }
}

}  // namespace

// static
std::unique_ptr<UiControllerAndroid> UiControllerAndroid::CreateFromWebContents(
    content::WebContents* web_contents,
    const base::android::JavaParamRef<jobject>& joverlay_coordinator) {
  JNIEnv* env = AttachCurrentThread();
  auto jactivity = Java_AutofillAssistantUiController_findAppropriateActivity(
      env, web_contents->GetJavaWebContents());
  if (!jactivity) {
    return nullptr;
  }
  return std::make_unique<UiControllerAndroid>(env, jactivity,
                                               joverlay_coordinator);
}

UiControllerAndroid::UiControllerAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jactivity,
    const base::android::JavaParamRef<jobject>& joverlay_coordinator)
    : overlay_delegate_(this),
      header_delegate_(this),
      collect_user_data_delegate_(this),
      form_delegate_(this),
      generic_ui_delegate_(this),
      bottom_bar_delegate_(this) {
  java_object_ = Java_AutofillAssistantUiController_create(
      env, jactivity,
      /* allowTabSwitching= */
      base::FeatureList::IsEnabled(features::kAutofillAssistantChromeEntry),
      reinterpret_cast<intptr_t>(this), joverlay_coordinator);

  // Register overlay_delegate_ as delegate for the overlay.
  Java_AssistantOverlayModel_setDelegate(env, GetOverlayModel(),
                                         overlay_delegate_.GetJavaObject());

  // Register header_delegate_ as delegate for clicks on header buttons.
  Java_AssistantHeaderModel_setDelegate(env, GetHeaderModel(),
                                        header_delegate_.GetJavaObject());

  // Register collect_user_data_delegate_ as delegate for the collect user data
  // UI.
  Java_AssistantCollectUserDataModel_setDelegate(
      env, GetCollectUserDataModel(),
      collect_user_data_delegate_.GetJavaObject());
  Java_AssistantModel_setBottomBarDelegate(
      env, GetModel(), bottom_bar_delegate_.GetJavaObject());
}

void UiControllerAndroid::Attach(content::WebContents* web_contents,
                                 Client* client,
                                 UiDelegate* ui_delegate) {
  DCHECK(web_contents);
  DCHECK(client);
  DCHECK(ui_delegate);

  client_ = client;

  // Detach from the current ui_delegate, if one was set previously.
  Detach();

  // Attach to the new ui_delegate.
  ui_delegate_ = ui_delegate;
  ui_delegate_->AddObserver(this);

  destroy_timer_.reset();

  JNIEnv* env = AttachCurrentThread();
  auto java_web_contents = web_contents->GetJavaWebContents();
  Java_AutofillAssistantUiController_setWebContents(env, java_object_,
                                                    java_web_contents);
  Java_AssistantCollectUserDataModel_setWebContents(
      env, GetCollectUserDataModel(), java_web_contents);
  OnClientSettingsChanged(ui_delegate_->GetClientSettings());
  Java_AssistantModel_setPeekModeDisabled(env, GetModel(),
                                          ui_delegate->IsRunningLiteScript());

  if (ui_delegate->GetState() != AutofillAssistantState::INACTIVE &&
      ui_delegate->IsTabSelected()) {
    // The UI was created for an existing Controller.
    RestoreUi();
  } else if (ui_delegate->GetState() == AutofillAssistantState::INACTIVE) {
    SetVisible(true);
  }
  // The call to set the web contents will, for some edge cases, trigger a call
  // from the Java side to the onTabSelected method.
  // We want this to happen only after the AttachUI method was fully executed,
  // as it would otherwise find that IsTabSelected() is true when deciding if
  // restoring the UI.
  Java_AssistantModel_setWebContents(env, GetModel(), java_web_contents);
}

void UiControllerAndroid::Detach() {
  if (ui_delegate_) {
    ui_delegate_->RemoveObserver(this);
  }
  ui_delegate_ = nullptr;
}

UiControllerAndroid::~UiControllerAndroid() {
  Java_AutofillAssistantUiController_clearNativePtr(AttachCurrentThread(),
                                                    java_object_);
  if (ui_delegate_) {
    ui_delegate_->SetUiShown(false);
  }
  Detach();
}

base::android::ScopedJavaLocalRef<jobject> UiControllerAndroid::GetModel() {
  return Java_AutofillAssistantUiController_getModel(AttachCurrentThread(),
                                                     java_object_);
}

// Header related methods.

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetHeaderModel() {
  return Java_AssistantModel_getHeaderModel(AttachCurrentThread(), GetModel());
}

void UiControllerAndroid::OnStateChanged(AutofillAssistantState new_state) {
  if (!Java_AssistantModel_getVisible(AttachCurrentThread(), GetModel())) {
    // Leave the UI alone as long as it's invisible. Missed state changes will
    // be recovered by SetVisible(true).
    return;
  }
  SetupForState();
}

void UiControllerAndroid::SetupForState() {
  DCHECK(ui_delegate_ != nullptr);

  UpdateActions(ui_delegate_->GetUserActions());
  AutofillAssistantState state = ui_delegate_->GetState();
  AllowShowingSoftKeyboard(ShouldAllowSoftKeyboardForState(state));
  bool should_prompt_action_expand_sheet =
      ui_delegate_->ShouldPromptActionExpandSheet();
  switch (state) {
    case AutofillAssistantState::STARTING:
      SetOverlayState(OverlayState::FULL);
      SetSpinPoodle(true);
      return;

    case AutofillAssistantState::RUNNING:
      SetOverlayState(OverlayState::FULL);
      SetSpinPoodle(true);
      return;

    case AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT:
      SetOverlayState(OverlayState::HIDDEN);
      SetSpinPoodle(false);

      if (should_prompt_action_expand_sheet && ui_delegate_->IsTabSelected())
        ShowContentAndExpandBottomSheet();
      return;

    case AutofillAssistantState::PROMPT:
      SetOverlayState(OverlayState::PARTIAL);
      SetSpinPoodle(false);

      if (should_prompt_action_expand_sheet && ui_delegate_->IsTabSelected())
        ShowContentAndExpandBottomSheet();
      return;

    case AutofillAssistantState::BROWSE:
      SetOverlayState(OverlayState::HIDDEN);
      SetSpinPoodle(false);
      return;

    case AutofillAssistantState::MODAL_DIALOG:
      SetOverlayState(OverlayState::FULL);
      SetSpinPoodle(true);
      return;

    case AutofillAssistantState::STOPPED:
      SetOverlayState(OverlayState::HIDDEN);
      SetSpinPoodle(false);

      // Make sure the user sees the error message.
      if (ui_delegate_->IsTabSelected())
        ShowContentAndExpandBottomSheet();
      ResetGenericUiControllers();
      return;

    case AutofillAssistantState::TRACKING:
      SetOverlayState(OverlayState::HIDDEN);
      SetSpinPoodle(false);

      Java_AssistantModel_setVisible(AttachCurrentThread(), GetModel(), false);
      DestroySelf();
      return;

    case AutofillAssistantState::INACTIVE:
      // Wait for the state to change.
      return;
  }
  NOTREACHED() << "Unknown state: " << static_cast<int>(state);
}

void UiControllerAndroid::OnStatusMessageChanged(const std::string& message) {
  JNIEnv* env = AttachCurrentThread();
  Java_AssistantHeaderModel_setStatusMessage(
      env, GetHeaderModel(),
      base::android::ConvertUTF8ToJavaString(env, message));
}

void UiControllerAndroid::OnBubbleMessageChanged(const std::string& message) {
  if (!message.empty()) {
    JNIEnv* env = AttachCurrentThread();
    Java_AssistantHeaderModel_setBubbleMessage(
        env, GetHeaderModel(),
        base::android::ConvertUTF8ToJavaString(env, message));
  }
}

void UiControllerAndroid::OnProgressChanged(int progress) {
  Java_AssistantHeaderModel_setProgress(AttachCurrentThread(), GetHeaderModel(),
                                        progress);
}

void UiControllerAndroid::OnProgressActiveStepChanged(int active_step) {
  Java_AssistantHeaderModel_setProgressActiveStep(
      AttachCurrentThread(), GetHeaderModel(), active_step);
}

void UiControllerAndroid::OnProgressVisibilityChanged(bool visible) {
  Java_AssistantHeaderModel_setProgressVisible(AttachCurrentThread(),
                                               GetHeaderModel(), visible);
}

void UiControllerAndroid::OnProgressBarErrorStateChanged(bool error) {
  Java_AssistantHeaderModel_setProgressBarErrorState(AttachCurrentThread(),
                                                     GetHeaderModel(), error);
}

void UiControllerAndroid::OnStepProgressBarConfigurationChanged(
    const ShowProgressBarProto::StepProgressBarConfiguration& configuration) {
  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetHeaderModel();
  Java_AssistantHeaderModel_setUseStepProgressBar(
      env, jmodel, configuration.use_step_progress_bar());
  if (!configuration.annotated_step_icons().empty()) {
    auto jcontext =
        Java_AutofillAssistantUiController_getContext(env, java_object_);
    auto jlist = Java_AssistantHeaderModel_createIconList(env);
    for (const auto& icon : configuration.annotated_step_icons()) {
      Java_AssistantHeaderModel_addStepProgressBarIcon(
          env, jlist,
          ui_controller_android_utils::CreateJavaDrawable(env, jcontext,
                                                          icon.icon()));
    }
    Java_AssistantHeaderModel_setStepProgressBarIcons(env, jmodel, jlist);
  }
}

void UiControllerAndroid::OnViewportModeChanged(ViewportMode mode) {
  Java_AutofillAssistantUiController_setViewportMode(AttachCurrentThread(),
                                                     java_object_, mode);
}

void UiControllerAndroid::OnPeekModeChanged(
    ConfigureBottomSheetProto::PeekMode peek_mode) {
  Java_AutofillAssistantUiController_setPeekMode(AttachCurrentThread(),
                                                 java_object_, peek_mode);
}

void UiControllerAndroid::OnExpandBottomSheet() {
  Java_AutofillAssistantUiController_expandBottomSheet(AttachCurrentThread(),
                                                       java_object_);
}

void UiControllerAndroid::OnCollapseBottomSheet() {
  Java_AutofillAssistantUiController_collapseBottomSheet(AttachCurrentThread(),
                                                         java_object_);
}

void UiControllerAndroid::OnOverlayColorsChanged(
    const UiDelegate::OverlayColors& colors) {
  JNIEnv* env = AttachCurrentThread();
  auto overlay_model = GetOverlayModel();
  Java_AssistantOverlayModel_setBackgroundColor(
      env, overlay_model,
      ui_controller_android_utils::GetJavaColor(env, colors.background));
  Java_AssistantOverlayModel_setHighlightBorderColor(
      env, overlay_model,
      ui_controller_android_utils::GetJavaColor(env, colors.highlight_border));
}

void UiControllerAndroid::AllowShowingSoftKeyboard(bool enabled) {
  Java_AssistantModel_setAllowSoftKeyboard(AttachCurrentThread(), GetModel(),
                                           enabled);
}

void UiControllerAndroid::ShowContentAndExpandBottomSheet() {
  Java_AutofillAssistantUiController_showContentAndExpandBottomSheet(
      AttachCurrentThread(), java_object_);
}

void UiControllerAndroid::SetSpinPoodle(bool enabled) {
  Java_AssistantHeaderModel_setSpinPoodle(AttachCurrentThread(),
                                          GetHeaderModel(), enabled);
}

void UiControllerAndroid::OnFeedbackButtonClicked() {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_showFeedback(
      env, java_object_,
      base::android::ConvertUTF8ToJavaString(env,
                                             ui_delegate_->GetDebugContext()));
}

void UiControllerAndroid::OnViewEvent(const EventHandler::EventKey& key) {
  ui_delegate_->DispatchEvent(key);
}

void UiControllerAndroid::OnValueChanged(const std::string& identifier,
                                         const ValueProto& value) {
  ui_delegate_->GetUserModel()->SetValue(identifier, value);
}

void UiControllerAndroid::Shutdown(Metrics::DropOutReason reason) {
  client_->Shutdown(reason);
}

void UiControllerAndroid::ShowSnackbar(base::TimeDelta delay,
                                       const std::string& message,
                                       base::OnceCallback<void()> action) {
  if (delay.is_zero()) {
    std::move(action).Run();
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetModel();
  if (!Java_AssistantModel_getVisible(env, jmodel)) {
    // If the UI is not visible, execute the action immediately.
    std::move(action).Run();
    return;
  }
  SetVisible(false);
  snackbar_action_ = std::move(action);
  Java_AutofillAssistantUiController_showSnackbar(
      env, java_object_, static_cast<jint>(delay.InMilliseconds()),
      base::android::ConvertUTF8ToJavaString(env, message));
}

void UiControllerAndroid::SnackbarResult(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean undo) {
  base::OnceCallback<void()> action = std::move(snackbar_action_);
  if (!action) {
    NOTREACHED();
    return;
  }
  if (undo) {
    SetVisible(true);
    return;
  }
  std::move(action).Run();
}

void UiControllerAndroid::DestroySelf() {
  if (ui_delegate_)
    ui_delegate_->ShutdownIfNecessary();

  client_->DestroyUI();
}

void UiControllerAndroid::SetVisible(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean visible) {
  SetVisible(visible);
}

void UiControllerAndroid::SetVisible(bool visible) {
  Java_AssistantModel_setVisible(AttachCurrentThread(), GetModel(), visible);
  if (visible) {
    // Recover possibly state changes missed by OnStateChanged()
    SetupForState();
  } else {
    SetOverlayState(OverlayState::HIDDEN);
  }
  ui_delegate_->SetUiShown(visible);
}

void UiControllerAndroid::RestoreUi() {
  if (ui_delegate_ == nullptr)
    return;

  OnStatusMessageChanged(ui_delegate_->GetStatusMessage());
  OnBubbleMessageChanged(ui_delegate_->GetBubbleMessage());
  auto step_progress_bar_configuration =
      ui_delegate_->GetStepProgressBarConfiguration();
  if (step_progress_bar_configuration.has_value()) {
    OnStepProgressBarConfigurationChanged(*step_progress_bar_configuration);
    if (step_progress_bar_configuration->use_step_progress_bar()) {
      auto active_step = ui_delegate_->GetProgressActiveStep();
      if (active_step.has_value()) {
        OnProgressActiveStepChanged(*active_step);
      }
      OnProgressBarErrorStateChanged(ui_delegate_->GetProgressBarErrorState());
    }
  } else {
    OnStepProgressBarConfigurationChanged(
        ShowProgressBarProto::StepProgressBarConfiguration());
    OnProgressChanged(ui_delegate_->GetProgress());
  }
  OnProgressVisibilityChanged(ui_delegate_->GetProgressVisible());
  OnInfoBoxChanged(ui_delegate_->GetInfoBox());
  OnDetailsChanged(ui_delegate_->GetDetails());
  OnUserActionsChanged(ui_delegate_->GetUserActions());
  OnCollectUserDataOptionsChanged(ui_delegate_->GetCollectUserDataOptions());
  OnUserDataChanged(ui_delegate_->GetUserData(), UserData::FieldChange::ALL);
  OnGenericUserInterfaceChanged(ui_delegate_->GetGenericUiProto());

  std::vector<RectF> area;
  ui_delegate_->GetTouchableArea(&area);
  std::vector<RectF> restricted_area;
  ui_delegate_->GetRestrictedArea(&restricted_area);
  RectF visual_viewport;
  ui_delegate_->GetVisualViewport(&visual_viewport);
  OnTouchableAreaChanged(visual_viewport, area, restricted_area);
  OnViewportModeChanged(ui_delegate_->GetViewportMode());
  OnPeekModeChanged(ui_delegate_->GetPeekMode());
  OnFormChanged(ui_delegate_->GetForm(), ui_delegate_->GetFormResult());
  UiDelegate::OverlayColors colors;
  ui_delegate_->GetOverlayColors(&colors);
  OnOverlayColorsChanged(colors);
  SetVisible(true);
  Java_AutofillAssistantUiController_restoreBottomSheetState(
      AttachCurrentThread(), java_object_,
      ui_controller_android_utils::ToJavaBottomSheetState(
          ui_delegate_->GetBottomSheetState()));
}

void UiControllerAndroid::OnTabSwitched(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint state,
    jboolean activity_changed) {
  if (ui_delegate_ == nullptr) {
    return;
  }

  // TODO(b/167947210) Allow lite scripts to transition from CCT to regular
  // scripts.
  if (activity_changed && ui_delegate_->IsRunningLiteScript()) {
    // Destroying UI here because Shutdown does not do so in all cases.
    DestroySelf();
    Shutdown(Metrics::DropOutReason::CUSTOM_TAB_CLOSED);
    return;
  }

  ui_delegate_->SetBottomSheetState(
      ui_controller_android_utils::ToNativeBottomSheetState(state));
  ui_delegate_->SetTabSelected(false);
}

void UiControllerAndroid::OnTabSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  if (ui_delegate_ == nullptr) {
    return;
  }
  if (!ui_delegate_->IsTabSelected()) {
    RestoreUi();
    ui_delegate_->SetTabSelected(true);
  }
}

// Actions carousels related methods.

void UiControllerAndroid::UpdateActions(
    const std::vector<UserAction>& user_actions) {
  DCHECK(ui_delegate_);

  JNIEnv* env = AttachCurrentThread();

  bool has_close_or_cancel = false;
  auto jchips = Java_AutofillAssistantUiController_createChipList(env);
  auto jsticky_chips = Java_AutofillAssistantUiController_createChipList(env);
  int user_action_count = static_cast<int>(user_actions.size());
  for (int i = 0; i < user_action_count; i++) {
    const auto& action = user_actions[i];
    const Chip& chip = action.chip();
    base::android::ScopedJavaLocalRef<jobject> jchip;
    switch (chip.type) {
      default:  // Ignore actions with other chip types or with no chips.
        break;

      case HIGHLIGHTED_ACTION:
        // Here and below, we set the identifier to the empty string so that we
        // can hide all the chips except for the cancel chip when the keyboard
        // is showing.
        // TODO(b/149543425): Find a better way to do this.
        jchip =
            Java_AutofillAssistantUiController_createHighlightedActionButton(
                env, java_object_, chip.icon,
                base::android::ConvertUTF8ToJavaString(env, chip.text), i,
                !action.enabled(), chip.sticky,
                base::android::ConvertUTF8ToJavaString(env, ""));
        break;

      case NORMAL_ACTION:
        jchip = Java_AutofillAssistantUiController_createActionButton(
            env, java_object_, chip.icon,
            base::android::ConvertUTF8ToJavaString(env, chip.text), i,
            !action.enabled(), chip.sticky,
            base::android::ConvertUTF8ToJavaString(env, ""));
        break;

      case CANCEL_ACTION:
        // A Cancel button sneaks in an UNDO snackbar before executing the
        // action, while a close button behaves like a normal button.
        jchip = Java_AutofillAssistantUiController_createCancelButton(
            env, java_object_, chip.icon,
            base::android::ConvertUTF8ToJavaString(env, chip.text), i,
            !action.enabled(), chip.sticky,
            base::android::ConvertUTF8ToJavaString(env, kCancelChipIdentifier));
        has_close_or_cancel = true;
        break;

      case CLOSE_ACTION:
        jchip = Java_AutofillAssistantUiController_createActionButton(
            env, java_object_, chip.icon,
            base::android::ConvertUTF8ToJavaString(env, chip.text), i,
            !action.enabled(), chip.sticky,
            base::android::ConvertUTF8ToJavaString(env, ""));
        has_close_or_cancel = true;
        break;

      case DONE_ACTION:
        jchip =
            Java_AutofillAssistantUiController_createHighlightedActionButton(
                env, java_object_, chip.icon,
                base::android::ConvertUTF8ToJavaString(env, chip.text), i,
                !action.enabled(), chip.sticky,
                base::android::ConvertUTF8ToJavaString(env, ""));
        has_close_or_cancel = true;
        break;
    }
    if (jchip) {
      Java_AutofillAssistantUiController_appendChipToList(env, jchips, jchip);
      if (chip.sticky) {
        Java_AutofillAssistantUiController_appendChipToList(env, jsticky_chips,
                                                            jchip);
      }
    }
  }

  if (!has_close_or_cancel) {
    base::android::ScopedJavaLocalRef<jobject> jcancel_chip;
    if (ui_delegate_->GetState() == AutofillAssistantState::STOPPED) {
      jcancel_chip = Java_AutofillAssistantUiController_createCloseButton(
          env, java_object_, ICON_CLEAR,
          base::android::ConvertUTF8ToJavaString(env, ""),
          /* disabled= */ false, /* sticky= */ true,
          base::android::ConvertUTF8ToJavaString(env, ""));
    } else if (ui_delegate_->GetState() != AutofillAssistantState::INACTIVE) {
      jcancel_chip = Java_AutofillAssistantUiController_createCancelButton(
          env, java_object_, ICON_CLEAR,
          base::android::ConvertUTF8ToJavaString(env, ""), -1,
          /* disabled= */ false, /* sticky= */ true,
          base::android::ConvertUTF8ToJavaString(env, kCancelChipIdentifier));
    }
    if (jcancel_chip) {
      Java_AutofillAssistantUiController_appendChipToList(env, jchips,
                                                          jcancel_chip);
      Java_AutofillAssistantUiController_appendChipToList(env, jsticky_chips,
                                                          jcancel_chip);
    }
  }

  Java_AutofillAssistantUiController_setActions(env, java_object_, jchips);
  Java_AssistantHeaderModel_setChips(AttachCurrentThread(), GetHeaderModel(),
                                     jsticky_chips);
}

void UiControllerAndroid::OnUserActionsChanged(
    const std::vector<UserAction>& actions) {
  UpdateActions(actions);
}

void UiControllerAndroid::OnUserActionSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint index) {
  if (ui_delegate_)
    ui_delegate_->PerformUserAction(index);
}

void UiControllerAndroid::OnCancelButtonClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint index) {
  // If the keyboard is currently shown, clicking the cancel button should
  // hide the keyboard rather than close autofill assistant, because the cancel
  // chip will be displayed right above the keyboard.
  if (Java_AutofillAssistantUiController_isKeyboardShown(env, java_object_)) {
    Java_AutofillAssistantUiController_hideKeyboard(env, java_object_);
    return;
  }

  CloseOrCancel(index, TriggerContext::CreateEmpty(),
                Metrics::DropOutReason::SHEET_CLOSED);
}

void UiControllerAndroid::OnCloseButtonClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  DestroySelf();
}

void UiControllerAndroid::OnKeyboardVisibilityChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean visible) {
  // Hide all chips except cancel while the keyboard is shown, to prevent users
  // from accidentally tapping chips while using the keyboard.
  // TODO(b/149543425): Find a better way to do this.
  Java_AutofillAssistantUiController_setAllChipsVisibleExcept(
      env, java_object_,
      base::android::ConvertUTF8ToJavaString(env, kCancelChipIdentifier),
      !visible);
}

bool UiControllerAndroid::OnBackButtonClicked() {
  // If the keyboard is currently shown, clicking the back button should
  // hide the keyboard rather than close autofill assistant.
  if (Java_AutofillAssistantUiController_isKeyboardShown(AttachCurrentThread(),
                                                         java_object_)) {
    Java_AutofillAssistantUiController_hideKeyboard(AttachCurrentThread(),
                                                    java_object_);
    return true;
  }

  // For BROWSE state the back button should react in its default way.
  if (ui_delegate_ != nullptr &&
      (ui_delegate_->GetState() == AutofillAssistantState::BROWSE)) {
    return false;
  }

  if (ui_delegate_ == nullptr ||
      ui_delegate_->GetState() == AutofillAssistantState::STOPPED ||
      ui_delegate_->IsRunningLiteScript()) {
    if (client_->GetWebContents() != nullptr &&
        client_->GetWebContents()->GetController().CanGoBack()) {
      client_->GetWebContents()->GetController().GoBack();
    }

    // Lite scripts should not shut down here. The navigation will be handled
    // by the lite script coordinator.
    if (!ui_delegate_ || !ui_delegate_->IsRunningLiteScript()) {
      // Destroying UI here because Shutdown does not do so in all cases.
      DestroySelf();
      Shutdown(Metrics::DropOutReason::BACK_BUTTON_CLICKED);
    }

    return true;
  }

  // ui_delegate_ must never be nullptr here!
  auto back_button_settings =
      ui_delegate_->GetClientSettings().back_button_settings;
  if (back_button_settings.has_value()) {
    ui_delegate_->OnStop(back_button_settings->message(),
                         back_button_settings->undo_label());
  } else {
    CloseOrCancel(-1, TriggerContext::CreateEmpty(),
                  Metrics::DropOutReason::BACK_BUTTON_CLICKED);
  }
  return true;
}

void UiControllerAndroid::OnBottomSheetClosedWithSwipe() {
  if (ui_delegate_->IsTabSelected() && ui_delegate_->IsRunningLiteScript()) {
    // Destroying UI here because Shutdown does not do so in all cases.
    DestroySelf();
    Shutdown(Metrics::DropOutReason::SHEET_CLOSED);
  }
}

void UiControllerAndroid::CloseOrCancel(
    int action_index,
    std::unique_ptr<TriggerContext> trigger_context,
    Metrics::DropOutReason dropout_reason) {
  // Close immediately.
  if (!ui_delegate_ ||
      ui_delegate_->GetState() == AutofillAssistantState::STOPPED) {
    DestroySelf();
    return;
  }

  // Close, with an action.
  const std::vector<UserAction>& user_actions = ui_delegate_->GetUserActions();
  if (action_index >= 0 &&
      static_cast<size_t>(action_index) < user_actions.size() &&
      user_actions[action_index].chip().type == CLOSE_ACTION &&
      ui_delegate_->PerformUserActionWithContext(action_index,
                                                 std::move(trigger_context))) {
    return;
  }

  // Cancel, with a snackbar to allow UNDO.
  ShowSnackbar(ui_delegate_->GetClientSettings().cancel_delay,
               l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_STOPPED),
               base::BindOnce(&UiControllerAndroid::OnCancel,
                              weak_ptr_factory_.GetWeakPtr(), action_index,
                              std::move(trigger_context), dropout_reason));
}

void UiControllerAndroid::OnCancel(
    int action_index,
    std::unique_ptr<TriggerContext> trigger_context,
    Metrics::DropOutReason dropout_reason) {
  if (action_index == -1 || !ui_delegate_ ||
      !ui_delegate_->PerformUserActionWithContext(action_index,
                                                  std::move(trigger_context))) {
    Shutdown(dropout_reason);
  }
}

// Overlay related methods.
base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetOverlayModel() {
  return Java_AssistantModel_getOverlayModel(AttachCurrentThread(), GetModel());
}

void UiControllerAndroid::SetOverlayState(OverlayState state) {
  desired_overlay_state_ = state;

  // Ensure that we don't set partial state if the touchable area is empty. This
  // is important because a partial overlay will be hidden if TalkBack is
  // enabled, and we want to completely prevent TalkBack from accessing the web
  // page if there is no touchable area.
  if (state == OverlayState::PARTIAL) {
    std::vector<RectF> area;
    ui_delegate_->GetTouchableArea(&area);
    if (area.empty()) {
      state = OverlayState::FULL;
    }
  }
  overlay_state_ = state;

  if (ui_delegate_ && ui_delegate_->ShouldShowOverlay()) {
    ApplyOverlayState(state);
  }
}

void UiControllerAndroid::ApplyOverlayState(OverlayState state) {
  Java_AssistantOverlayModel_setState(AttachCurrentThread(), GetOverlayModel(),
                                      state);
  Java_AssistantModel_setAllowTalkbackOnWebsite(
      AttachCurrentThread(), GetModel(), state != OverlayState::FULL);
}

void UiControllerAndroid::OnShouldShowOverlayChanged(bool should_show) {
  if (should_show) {
    ApplyOverlayState(overlay_state_);
  } else {
    ApplyOverlayState(OverlayState::HIDDEN);
  }
}

void UiControllerAndroid::OnTouchableAreaChanged(
    const RectF& visual_viewport,
    const std::vector<RectF>& touchable_areas,
    const std::vector<RectF>& restricted_areas) {
  if (!touchable_areas.empty() &&
      desired_overlay_state_ == OverlayState::PARTIAL) {
    SetOverlayState(OverlayState::PARTIAL);
  }

  JNIEnv* env = AttachCurrentThread();
  Java_AssistantOverlayModel_setVisualViewport(
      env, GetOverlayModel(), visual_viewport.left, visual_viewport.top,
      visual_viewport.right, visual_viewport.bottom);

  Java_AssistantOverlayModel_setTouchableArea(
      env, GetOverlayModel(),
      base::android::ToJavaFloatArray(env, ToFloatVector(touchable_areas)));

  Java_AssistantOverlayModel_setRestrictedArea(
      AttachCurrentThread(), GetOverlayModel(),
      base::android::ToJavaFloatArray(env, ToFloatVector(restricted_areas)));
}

void UiControllerAndroid::OnUnexpectedTaps() {
  if (!ui_delegate_) {
    Shutdown(Metrics::DropOutReason::OVERLAY_STOP);
    return;
  }

  ShowSnackbar(ui_delegate_->GetClientSettings().tap_shutdown_delay,
               l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_MAYBE_GIVE_UP),
               base::BindOnce(&UiControllerAndroid::Shutdown,
                              weak_ptr_factory_.GetWeakPtr(),
                              Metrics::DropOutReason::OVERLAY_STOP));
}

void UiControllerAndroid::OnUserInteractionInsideTouchableArea() {
  if (ui_delegate_)
    ui_delegate_->OnUserInteractionInsideTouchableArea();
}

// Other methods.
void UiControllerAndroid::CloseCustomTab() {
  Java_AutofillAssistantUiController_scheduleCloseCustomTab(
      AttachCurrentThread(), java_object_);
}

// Collect user data related methods.

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetCollectUserDataModel() {
  return Java_AssistantModel_getCollectUserDataModel(AttachCurrentThread(),
                                                     GetModel());
}

void UiControllerAndroid::OnShippingAddressChanged(
    std::unique_ptr<autofill::AutofillProfile> address) {
  ui_delegate_->SetShippingAddress(std::move(address));
}

void UiControllerAndroid::OnContactInfoChanged(
    std::unique_ptr<autofill::AutofillProfile> profile) {
  ui_delegate_->SetContactInfo(std::move(profile));
}

void UiControllerAndroid::OnCreditCardChanged(
    std::unique_ptr<autofill::CreditCard> card,
    std::unique_ptr<autofill::AutofillProfile> billing_profile) {
  ui_delegate_->SetCreditCard(std::move(card), std::move(billing_profile));
}

void UiControllerAndroid::OnTermsAndConditionsChanged(
    TermsAndConditionsState state) {
  ui_delegate_->SetTermsAndConditions(state);
}

void UiControllerAndroid::OnLoginChoiceChanged(std::string identifier) {
  ui_delegate_->SetLoginOption(identifier);
}

void UiControllerAndroid::OnTextLinkClicked(int link) {
  ui_delegate_->OnTextLinkClicked(link);
}

void UiControllerAndroid::OnFormActionLinkClicked(int link) {
  ui_delegate_->OnFormActionLinkClicked(link);
}

void UiControllerAndroid::OnDateTimeRangeStartDateChanged(int year,
                                                          int month,
                                                          int day) {
  auto date = base::make_optional<DateProto>();
  date->set_year(year);
  date->set_month(month);
  date->set_day(day);
  ui_delegate_->SetDateTimeRangeStartDate(date);
}

void UiControllerAndroid::OnDateTimeRangeStartDateCleared() {
  ui_delegate_->SetDateTimeRangeStartDate(base::nullopt);
}

void UiControllerAndroid::OnDateTimeRangeStartTimeSlotChanged(int index) {
  ui_delegate_->SetDateTimeRangeStartTimeSlot(base::make_optional<int>(index));
}

void UiControllerAndroid::OnDateTimeRangeStartTimeSlotCleared() {
  ui_delegate_->SetDateTimeRangeStartTimeSlot(base::nullopt);
}

void UiControllerAndroid::OnDateTimeRangeEndDateChanged(int year,
                                                        int month,
                                                        int day) {
  auto date = base::make_optional<DateProto>();
  date->set_year(year);
  date->set_month(month);
  date->set_day(day);
  ui_delegate_->SetDateTimeRangeEndDate(date);
}

void UiControllerAndroid::OnDateTimeRangeEndDateCleared() {
  ui_delegate_->SetDateTimeRangeEndDate(base::nullopt);
}

void UiControllerAndroid::OnDateTimeRangeEndTimeSlotChanged(int index) {
  ui_delegate_->SetDateTimeRangeEndTimeSlot(base::make_optional<int>(index));
}

void UiControllerAndroid::OnDateTimeRangeEndTimeSlotCleared() {
  ui_delegate_->SetDateTimeRangeEndTimeSlot(base::nullopt);
}

void UiControllerAndroid::OnKeyValueChanged(const std::string& key,
                                            const ValueProto& value) {
  ui_delegate_->SetAdditionalValue(key, value);
}

void UiControllerAndroid::OnTextFocusLost() {
  // We set a delay to avoid having the keyboard flickering when the focus goes
  // from one text field to another
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UiControllerAndroid::HideKeyboardIfFocusNotOnText,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(50));
}

bool UiControllerAndroid::IsContactComplete(
    autofill::AutofillProfile* contact) {
  auto* options = ui_delegate_->GetCollectUserDataOptions();
  if (options == nullptr) {
    return false;
  }
  return IsCompleteContact(contact, *options);
}

bool UiControllerAndroid::IsShippingAddressComplete(
    autofill::AutofillProfile* address) {
  auto* options = ui_delegate_->GetCollectUserDataOptions();
  if (options == nullptr) {
    return false;
  }
  return IsCompleteShippingAddress(address, *options);
}

bool UiControllerAndroid::IsPaymentInstrumentComplete(
    autofill::CreditCard* card,
    autofill::AutofillProfile* address) {
  auto* options = ui_delegate_->GetCollectUserDataOptions();
  if (options == nullptr) {
    return false;
  }
  return IsCompleteCreditCard(card, address, *options);
}

void UiControllerAndroid::HideKeyboardIfFocusNotOnText() {
  Java_AutofillAssistantUiController_hideKeyboardIfFocusNotOnText(
      AttachCurrentThread(), java_object_);
}

void UiControllerAndroid::OnCollectUserDataOptionsChanged(
    const CollectUserDataOptions* collect_user_data_options) {
  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetCollectUserDataModel();
  if (!collect_user_data_options) {
    ResetGenericUiControllers();
    Java_AssistantCollectUserDataModel_setVisible(env, jmodel, false);
    return;
  }

  Java_AssistantCollectUserDataModel_setRequestName(
      env, jmodel, collect_user_data_options->request_payer_name);
  Java_AssistantCollectUserDataModel_setRequestEmail(
      env, jmodel, collect_user_data_options->request_payer_email);
  Java_AssistantCollectUserDataModel_setRequestPhone(
      env, jmodel, collect_user_data_options->request_payer_phone);
  std::vector<int> contact_summary_fields;
  for (const auto& field : collect_user_data_options->contact_summary_fields) {
    contact_summary_fields.emplace_back((int)field);
  }
  Java_AssistantCollectUserDataModel_setContactSummaryDescriptionOptions(
      env, jmodel,
      Java_AssistantCollectUserDataModel_createContactDescriptionOptions(
          env, base::android::ToJavaIntArray(env, contact_summary_fields),
          collect_user_data_options->contact_summary_max_lines));
  std::vector<int> contact_full_fields;
  for (const auto& field : collect_user_data_options->contact_full_fields) {
    contact_full_fields.emplace_back((int)field);
  }
  Java_AssistantCollectUserDataModel_setContactFullDescriptionOptions(
      env, jmodel,
      Java_AssistantCollectUserDataModel_createContactDescriptionOptions(
          env, base::android::ToJavaIntArray(env, contact_full_fields),
          collect_user_data_options->contact_full_max_lines));
  Java_AssistantCollectUserDataModel_setRequestShippingAddress(
      env, jmodel, collect_user_data_options->request_shipping);
  Java_AssistantCollectUserDataModel_setRequestPayment(
      env, jmodel, collect_user_data_options->request_payment_method);
  Java_AssistantCollectUserDataModel_setRequestLoginChoice(
      env, jmodel, collect_user_data_options->request_login_choice);
  Java_AssistantCollectUserDataModel_setLoginSectionTitle(
      env, jmodel,
      base::android::ConvertUTF8ToJavaString(
          env, collect_user_data_options->login_section_title));
  Java_AssistantCollectUserDataModel_setContactSectionTitle(
      env, jmodel,
      base::android::ConvertUTF8ToJavaString(
          env, collect_user_data_options->contact_details_section_title));
  Java_AssistantCollectUserDataModel_setShippingSectionTitle(
      env, jmodel,
      base::android::ConvertUTF8ToJavaString(
          env, collect_user_data_options->shipping_address_section_title));
  Java_AssistantCollectUserDataModel_setAcceptTermsAndConditionsText(
      env, jmodel,
      base::android::ConvertUTF8ToJavaString(
          env, collect_user_data_options->accept_terms_and_conditions_text));
  Java_AssistantCollectUserDataModel_setShowTermsAsCheckbox(
      env, jmodel, collect_user_data_options->show_terms_as_checkbox);
  Java_AssistantCollectUserDataModel_setRequireBillingPostalCode(
      env, jmodel, collect_user_data_options->require_billing_postal_code);
  Java_AssistantCollectUserDataModel_setBillingPostalCodeMissingText(
      env, jmodel,
      base::android::ConvertUTF8ToJavaString(
          env, collect_user_data_options->billing_postal_code_missing_text));
  Java_AssistantCollectUserDataModel_setCreditCardExpiredText(
      env, jmodel,
      base::android::ConvertUTF8ToJavaString(
          env, collect_user_data_options->credit_card_expired_text));
  Java_AssistantCollectUserDataModel_setSupportedBasicCardNetworks(
      env, jmodel,
      base::android::ToJavaArrayOfStrings(
          env, collect_user_data_options->supported_basic_card_networks));
  if (collect_user_data_options->request_login_choice) {
    auto jlist = CreateJavaLoginChoiceList(
        env, collect_user_data_options->login_choices);
    Java_AssistantCollectUserDataModel_setLoginChoices(env, jmodel, jlist);
  }
  Java_AssistantCollectUserDataModel_setRequestDateRange(
      env, jmodel, collect_user_data_options->request_date_time_range);
  if (collect_user_data_options->request_date_time_range) {
    auto jmin_date = CreateJavaDate(
        env, collect_user_data_options->date_time_range.min_date());
    auto jmax_date = CreateJavaDate(
        env, collect_user_data_options->date_time_range.max_date());
    std::vector<std::string> time_slots;
    for (const auto& slot :
         collect_user_data_options->date_time_range.time_slots()) {
      time_slots.emplace_back(slot.label());
    }
    auto jtime_slots = base::android::ToJavaArrayOfStrings(env, time_slots);
    Java_AssistantCollectUserDataModel_setDateTimeRangeStartOptions(
        env, jmodel, jmin_date, jmax_date, jtime_slots);
    Java_AssistantCollectUserDataModel_setDateTimeRangeEndOptions(
        env, jmodel, jmin_date, jmax_date, jtime_slots);
    Java_AssistantCollectUserDataModel_setDateTimeRangeStartDateLabel(
        env, jmodel,
        base::android::ConvertUTF8ToJavaString(
            env,
            collect_user_data_options->date_time_range.start_date_label()));
    Java_AssistantCollectUserDataModel_setDateTimeRangeStartTimeLabel(
        env, jmodel,
        base::android::ConvertUTF8ToJavaString(
            env,
            collect_user_data_options->date_time_range.start_time_label()));
    Java_AssistantCollectUserDataModel_setDateTimeRangeEndDateLabel(
        env, jmodel,
        base::android::ConvertUTF8ToJavaString(
            env, collect_user_data_options->date_time_range.end_date_label()));
    Java_AssistantCollectUserDataModel_setDateTimeRangeEndTimeLabel(
        env, jmodel,
        base::android::ConvertUTF8ToJavaString(
            env, collect_user_data_options->date_time_range.end_time_label()));
    Java_AssistantCollectUserDataModel_setDateTimeRangeDateNotSetErrorMessage(
        env, jmodel,
        base::android::ConvertUTF8ToJavaString(
            env,
            collect_user_data_options->date_time_range.date_not_set_error()));
    Java_AssistantCollectUserDataModel_setDateTimeRangeTimeNotSetErrorMessage(
        env, jmodel,
        base::android::ConvertUTF8ToJavaString(
            env,
            collect_user_data_options->date_time_range.time_not_set_error()));
  }
  Java_AssistantCollectUserDataModel_setTermsRequireReviewText(
      env, jmodel,
      base::android::ConvertUTF8ToJavaString(
          env, collect_user_data_options->terms_require_review_text));
  Java_AssistantCollectUserDataModel_setInfoSectionText(
      env, jmodel,
      base::android::ConvertUTF8ToJavaString(
          env, collect_user_data_options->info_section_text),
      collect_user_data_options->info_section_text_center);
  Java_AssistantCollectUserDataModel_setPrivacyNoticeText(
      env, jmodel,
      base::android::ConvertUTF8ToJavaString(
          env, collect_user_data_options->privacy_notice_text));

  Java_AssistantCollectUserDataModel_setPrependedSections(
      env, jmodel,
      CreateJavaAdditionalSections(
          env, collect_user_data_options->additional_prepended_sections));
  Java_AssistantCollectUserDataModel_setAppendedSections(
      env, jmodel,
      CreateJavaAdditionalSections(
          env, collect_user_data_options->additional_appended_sections));

  if (collect_user_data_options->generic_user_interface_prepended.has_value()) {
    collect_user_data_prepended_generic_ui_controller_ =
        CreateGenericUiControllerForProto(
            *collect_user_data_options->generic_user_interface_prepended);
    Java_AssistantCollectUserDataModel_setGenericUserInterfacePrepended(
        env, jmodel,
        collect_user_data_prepended_generic_ui_controller_ != nullptr
            ? collect_user_data_prepended_generic_ui_controller_->GetRootView()
            : nullptr);
  }
  if (collect_user_data_options->generic_user_interface_appended.has_value()) {
    collect_user_data_appended_generic_ui_controller_ =
        CreateGenericUiControllerForProto(
            *collect_user_data_options->generic_user_interface_appended);
    Java_AssistantCollectUserDataModel_setGenericUserInterfaceAppended(
        env, jmodel,
        collect_user_data_appended_generic_ui_controller_ != nullptr
            ? collect_user_data_appended_generic_ui_controller_->GetRootView()
            : nullptr);
  }

  Java_AssistantCollectUserDataModel_setVisible(env, jmodel, true);
}

void UiControllerAndroid::OnUserDataChanged(
    const UserData* state,
    UserData::FieldChange field_change) {
  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetCollectUserDataModel();
  if (!state) {
    return;
  }
  DCHECK(ui_delegate_ != nullptr);
  DCHECK(client_->GetWebContents() != nullptr);
  const CollectUserDataOptions* collect_user_data_options =
      ui_delegate_->GetCollectUserDataOptions();
  if (collect_user_data_options == nullptr) {
    // If there are no options, there currently is no active
    // CollectUserDataAction, the UI is not shown and does not need to be
    // updated.
    return;
  }

  auto jcontext =
      Java_AutofillAssistantUiController_getContext(env, java_object_);
  auto web_contents = client_->GetWebContents()->GetJavaWebContents();

  if (field_change == UserData::FieldChange::ALL ||
      field_change == UserData::FieldChange::TERMS_AND_CONDITIONS) {
    Java_AssistantCollectUserDataModel_setTermsStatus(
        env, jmodel, state->terms_and_conditions_);
  }

  if (field_change == UserData::FieldChange::ALL ||
      field_change == UserData::FieldChange::AVAILABLE_PROFILES) {
    // Contact profiles.
    auto jcontactlist =
        Java_AssistantCollectUserDataModel_createAutofillContactList(env);
    auto contact_indices = SortContactsByCompleteness(
        *collect_user_data_options, state->available_profiles_);
    for (int index : contact_indices) {
      auto jcontact = Java_AssistantCollectUserDataModel_createAutofillContact(
          env, jcontext,
          autofill::PersonalDataManagerAndroid::CreateJavaProfileFromNative(
              env, *state->available_profiles_[index]),
          collect_user_data_options->request_payer_name,
          collect_user_data_options->request_payer_phone,
          collect_user_data_options->request_payer_email);
      if (jcontact) {
        Java_AssistantCollectUserDataModel_addAutofillContact(env, jcontactlist,
                                                              jcontact);
      }
    }
    Java_AssistantCollectUserDataModel_setAvailableContacts(env, jmodel,
                                                            jcontactlist);

    // Ignore changes to FieldChange::CONTACT_PROFILE, this is already coming
    // from the view.
    const autofill::AutofillProfile* contact_profile = state->selected_address(
        collect_user_data_options->contact_details_name);
    Java_AssistantCollectUserDataModel_setSelectedContactDetails(
        env, jmodel,
        contact_profile == nullptr
            ? nullptr
            : Java_AssistantCollectUserDataModel_createAutofillContact(
                  env, jcontext,
                  autofill::PersonalDataManagerAndroid::
                      CreateJavaProfileFromNative(env, *contact_profile),
                  collect_user_data_options->request_payer_name,
                  collect_user_data_options->request_payer_phone,
                  collect_user_data_options->request_payer_email));

    // Billing addresses profiles.
    auto jbillinglist =
        Java_AssistantCollectUserDataModel_createAutofillAddressList(env);
    for (const auto& profile : state->available_profiles_) {
      auto jaddress = Java_AssistantCollectUserDataModel_createAutofillAddress(
          env, jcontext,
          autofill::PersonalDataManagerAndroid::CreateJavaProfileFromNative(
              env, *profile));
      if (jaddress) {
        Java_AssistantCollectUserDataModel_addAutofillAddress(env, jbillinglist,
                                                              jaddress);
      }
    }
    Java_AssistantCollectUserDataModel_setAvailableBillingAddresses(
        env, jmodel, jbillinglist);

    // Address profiles.
    auto jshippinglist =
        Java_AssistantCollectUserDataModel_createAutofillAddressList(env);
    auto address_indices = SortAddressesByCompleteness(
        *collect_user_data_options, state->available_profiles_);
    for (int index : address_indices) {
      auto jaddress = Java_AssistantCollectUserDataModel_createAutofillAddress(
          env, jcontext,
          autofill::PersonalDataManagerAndroid::CreateJavaProfileFromNative(
              env, *state->available_profiles_[index]));
      if (jaddress) {
        Java_AssistantCollectUserDataModel_addAutofillAddress(
            env, jshippinglist, jaddress);
      }
    }
    Java_AssistantCollectUserDataModel_setAvailableShippingAddresses(
        env, jmodel, jshippinglist);

    // Ignore changes to FieldChange::SHIPPING_ADDRESS, this is already coming
    // from the view.
    const autofill::AutofillProfile* shipping_address = state->selected_address(
        collect_user_data_options->shipping_address_name);
    Java_AssistantCollectUserDataModel_setSelectedShippingAddress(
        env, jmodel,
        shipping_address == nullptr
            ? nullptr
            : Java_AssistantCollectUserDataModel_createAutofillAddress(
                  env, jcontext,
                  autofill::PersonalDataManagerAndroid::
                      CreateJavaProfileFromNative(env, *shipping_address)));
  }

  if (field_change == UserData::FieldChange::ALL ||
      field_change == UserData::FieldChange::AVAILABLE_PAYMENT_INSTRUMENTS) {
    auto jlist =
        Java_AssistantCollectUserDataModel_createAutofillPaymentInstrumentList(
            env);
    auto sorted_payment_instrument_indices =
        SortPaymentInstrumentsByCompleteness(
            *collect_user_data_options, state->available_payment_instruments_);
    for (int index : sorted_payment_instrument_indices) {
      const auto& instrument = state->available_payment_instruments_[index];
      Java_AssistantCollectUserDataModel_addAutofillPaymentInstrument(
          env, jlist, web_contents,
          instrument->card == nullptr
              ? nullptr
              : autofill::PersonalDataManagerAndroid::
                    CreateJavaCreditCardFromNative(env, *(instrument->card)),
          instrument->billing_address == nullptr
              ? nullptr
              : autofill::PersonalDataManagerAndroid::
                    CreateJavaProfileFromNative(
                        env, *(instrument->billing_address)));
    }
    Java_AssistantCollectUserDataModel_setAvailablePaymentInstruments(
        env, jmodel, jlist);

    // Ignore changes to FieldChange::CARD, this is already coming from the
    // view.
    autofill::CreditCard* card = state->selected_card_.get();
    const autofill::AutofillProfile* billing_address = state->selected_address(
        collect_user_data_options->billing_address_name);
    Java_AssistantCollectUserDataModel_setSelectedPaymentInstrument(
        env, jmodel, web_contents,
        card == nullptr ? nullptr
                        : autofill::PersonalDataManagerAndroid::
                              CreateJavaCreditCardFromNative(env, *card),
        billing_address == nullptr
            ? nullptr
            : autofill::PersonalDataManagerAndroid::CreateJavaProfileFromNative(
                  env, *billing_address));
  }

  if (field_change == UserData::FieldChange::ALL ||
      field_change == UserData::FieldChange::DATE_TIME_RANGE_START) {
    if (state->date_time_range_start_date_.has_value()) {
      Java_AssistantCollectUserDataModel_setDateTimeRangeStartDate(
          env, jmodel,
          CreateJavaDate(env, *state->date_time_range_start_date_));
    } else {
      Java_AssistantCollectUserDataModel_clearDateTimeRangeStartDate(env,
                                                                     jmodel);
    }

    if (state->date_time_range_start_timeslot_.has_value()) {
      Java_AssistantCollectUserDataModel_setDateTimeRangeStartTimeSlot(
          env, jmodel, *state->date_time_range_start_timeslot_);
    } else {
      Java_AssistantCollectUserDataModel_clearDateTimeRangeStartTimeSlot(
          env, jmodel);
    }
  }

  if (field_change == UserData::FieldChange::ALL ||
      field_change == UserData::FieldChange::DATE_TIME_RANGE_END) {
    if (state->date_time_range_end_date_.has_value()) {
      Java_AssistantCollectUserDataModel_setDateTimeRangeEndDate(
          env, jmodel, CreateJavaDate(env, *state->date_time_range_end_date_));
    } else {
      Java_AssistantCollectUserDataModel_clearDateTimeRangeEndDate(env, jmodel);
    }

    if (state->date_time_range_end_timeslot_.has_value()) {
      Java_AssistantCollectUserDataModel_setDateTimeRangeEndTimeSlot(
          env, jmodel, *state->date_time_range_end_timeslot_);
    } else {
      Java_AssistantCollectUserDataModel_clearDateTimeRangeEndTimeSlot(env,
                                                                       jmodel);
    }
  }

  // TODO(crbug.com/806868): Add |setSelectedLogin|.
}

// FormProto related methods.
base::android::ScopedJavaLocalRef<jobject> UiControllerAndroid::GetFormModel() {
  return Java_AssistantModel_getFormModel(AttachCurrentThread(), GetModel());
}

void UiControllerAndroid::OnFormChanged(const FormProto* form,
                                        const FormProto::Result* result) {
  JNIEnv* env = AttachCurrentThread();

  if (!form) {
    Java_AssistantFormModel_clearInputs(env, GetFormModel());
    return;
  }

  auto jinput_list = Java_AssistantFormModel_createInputList(env);
  for (int i = 0; i < form->inputs_size(); i++) {
    const FormInputProto input = form->inputs(i);

    switch (input.input_type_case()) {
      case FormInputProto::InputTypeCase::kCounter: {
        CounterInputProto counter_input = input.counter();

        auto jcounters = Java_AssistantFormInput_createCounterList(env);
        for (int j = 0; j < counter_input.counters_size(); ++j) {
          const CounterInputProto::Counter& counter = counter_input.counters(j);

          std::vector<int> allowed_values;
          for (int value : counter.allowed_values()) {
            allowed_values.push_back(value);
          }

          auto result_value = GetPreviousFormCounterResult(result, i, j);
          Java_AssistantFormInput_addCounter(
              env, jcounters,
              Java_AssistantFormInput_createCounter(
                  env,
                  base::android::ConvertUTF8ToJavaString(env, counter.label()),
                  base::android::ConvertUTF8ToJavaString(
                      env, counter.description_line_1()),
                  base::android::ConvertUTF8ToJavaString(
                      env, counter.description_line_2()),
                  result_value.has_value() ? result_value.value()
                                           : counter.initial_value(),
                  counter.min_value(), counter.max_value(),
                  base::android::ToJavaIntArray(env, allowed_values)));
        }

        Java_AssistantFormModel_addInput(
            env, jinput_list,
            Java_AssistantFormInput_createCounterInput(
                env, i,
                base::android::ConvertUTF8ToJavaString(env,
                                                       counter_input.label()),
                base::android::ConvertUTF8ToJavaString(
                    env, counter_input.expand_text()),
                base::android::ConvertUTF8ToJavaString(
                    env, counter_input.minimize_text()),
                jcounters, counter_input.minimized_count(),
                counter_input.min_counters_sum(),
                counter_input.max_counters_sum(),
                form_delegate_.GetJavaObject()));
        break;
      }
      case FormInputProto::InputTypeCase::kSelection: {
        SelectionInputProto selection_input = input.selection();

        auto jchoices = Java_AssistantFormInput_createChoiceList(env);
        for (int j = 0; j < selection_input.choices_size(); ++j) {
          const SelectionInputProto::Choice& choice =
              selection_input.choices(j);

          auto result_value = GetPreviousFormSelectionResult(result, i, j);
          Java_AssistantFormInput_addChoice(
              env, jchoices,
              Java_AssistantFormInput_createChoice(
                  env,
                  base::android::ConvertUTF8ToJavaString(env, choice.label()),
                  base::android::ConvertUTF8ToJavaString(
                      env, choice.description_line_1()),
                  base::android::ConvertUTF8ToJavaString(
                      env, choice.description_line_2()),
                  result_value.has_value() ? result_value.value()
                                           : choice.selected()));
        }

        Java_AssistantFormModel_addInput(
            env, jinput_list,
            Java_AssistantFormInput_createSelectionInput(
                env, i,
                base::android::ConvertUTF8ToJavaString(env,
                                                       selection_input.label()),
                jchoices, selection_input.allow_multiple(),
                form_delegate_.GetJavaObject()));
        break;
      }
      case FormInputProto::InputTypeCase::INPUT_TYPE_NOT_SET:
        NOTREACHED();
        break;
        // Intentionally no default case to make compilation fail if a new value
        // was added to the enum but not to this list.
    }
  }
  Java_AssistantFormModel_setInputs(env, GetFormModel(), jinput_list);

  if (form->has_info_label()) {
    Java_AssistantFormModel_setInfoLabel(
        env, GetFormModel(),
        base::android::ConvertUTF8ToJavaString(env, form->info_label()));
  } else {
    Java_AssistantFormModel_clearInfoLabel(env, GetFormModel());
  }

  if (form->has_info_popup()) {
    Java_AssistantFormModel_setInfoPopup(
        env, GetFormModel(),
        ui_controller_android_utils::CreateJavaInfoPopup(env,
                                                         form->info_popup()));
  } else {
    Java_AssistantFormModel_clearInfoPopup(env, GetFormModel());
  }
}

void UiControllerAndroid::OnClientSettingsChanged(
    const ClientSettings& settings) {
  JNIEnv* env = AttachCurrentThread();
  Java_AssistantOverlayModel_setTapTracking(
      env, GetOverlayModel(), settings.tap_count,
      settings.tap_tracking_duration.InMilliseconds());

  if (settings.overlay_image.has_value()) {
    auto jcontext =
        Java_AutofillAssistantUiController_getContext(env, java_object_);
    const auto& image = *(settings.overlay_image);
    int image_size = ui_controller_android_utils::GetPixelSizeOrDefault(
        env, jcontext, image.image_size(), 0);
    int top_margin = ui_controller_android_utils::GetPixelSizeOrDefault(
        env, jcontext, image.image_top_margin(), 0);
    int bottom_margin = ui_controller_android_utils::GetPixelSizeOrDefault(
        env, jcontext, image.image_bottom_margin(), 0);
    int text_size = ui_controller_android_utils::GetPixelSizeOrDefault(
        env, jcontext, image.text_size(), 0);

    Java_AssistantOverlayModel_setOverlayImage(
        env, GetOverlayModel(),
        base::android::ConvertUTF8ToJavaString(env, image.image_url()),
        image_size, top_margin, bottom_margin,
        base::android::ConvertUTF8ToJavaString(env, image.text()),
        ui_controller_android_utils::GetJavaColor(env, image.text_color()),
        text_size);
  } else {
    Java_AssistantOverlayModel_clearOverlayImage(env, GetOverlayModel());
  }
  if (settings.integration_test_settings.has_value()) {
    Java_AssistantHeaderModel_setDisableAnimations(
        env, GetHeaderModel(),
        settings.integration_test_settings->disable_header_animations());
    Java_AutofillAssistantUiController_setDisableChipChangeAnimations(
        env, java_object_,
        settings.integration_test_settings
            ->disable_carousel_change_animations());
  }
  Java_AssistantModel_setTalkbackSheetSizeFraction(
      env, GetModel(), settings.talkback_sheet_size_fraction);
}

void UiControllerAndroid::OnGenericUserInterfaceChanged(
    const GenericUserInterfaceProto* generic_ui) {
  // Try to inflate user interface from proto.
  if (generic_ui != nullptr) {
    generic_ui_controller_ = CreateGenericUiControllerForProto(*generic_ui);
    ClientStatus status(generic_ui_controller_ ? ACTION_APPLIED
                                               : INVALID_ACTION);
    ui_delegate_->GetBasicInteractions()->NotifyViewInflationFinished(status);
  } else {
    generic_ui_controller_.reset();
  }

  // Set or clear generic UI.
  Java_AssistantGenericUiModel_setView(
      AttachCurrentThread(), GetGenericUiModel(),
      generic_ui_controller_ != nullptr ? generic_ui_controller_->GetRootView()
                                        : nullptr);
}

void UiControllerAndroid::OnCounterChanged(int input_index,
                                           int counter_index,
                                           int value) {
  ui_delegate_->SetCounterValue(input_index, counter_index, value);
}

void UiControllerAndroid::OnChoiceSelectionChanged(int input_index,
                                                   int choice_index,
                                                   bool selected) {
  ui_delegate_->SetChoiceSelected(input_index, choice_index, selected);
}

// Details related method.

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetDetailsModel() {
  return Java_AssistantModel_getDetailsModel(AttachCurrentThread(), GetModel());
}

void UiControllerAndroid::OnDetailsChanged(const Details* details) {
  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetDetailsModel();
  if (!details) {
    Java_AssistantDetailsModel_clearDetails(env, jmodel);
    return;
  }
  auto opt_image_accessibility_hint = details->imageAccessibilityHint();
  base::android::ScopedJavaLocalRef<jstring> jimage_accessibility_hint =
      nullptr;
  if (opt_image_accessibility_hint.has_value()) {
    jimage_accessibility_hint = base::android::ConvertUTF8ToJavaString(
        env, opt_image_accessibility_hint.value());
  }
  auto jdetails = Java_AssistantDetails_create(
      env, base::android::ConvertUTF8ToJavaString(env, details->title()),
      details->titleMaxLines(),
      base::android::ConvertUTF8ToJavaString(env, details->imageUrl()),
      jimage_accessibility_hint, details->imageAllowClickthrough(),
      base::android::ConvertUTF8ToJavaString(env, details->imageDescription()),
      base::android::ConvertUTF8ToJavaString(env, details->imagePositiveText()),
      base::android::ConvertUTF8ToJavaString(env, details->imageNegativeText()),
      base::android::ConvertUTF8ToJavaString(env,
                                             details->imageClickthroughUrl()),
      details->showImagePlaceholder(),
      base::android::ConvertUTF8ToJavaString(env, details->totalPriceLabel()),
      base::android::ConvertUTF8ToJavaString(env, details->totalPrice()),
      base::android::ConvertUTF8ToJavaString(env, details->descriptionLine1()),
      base::android::ConvertUTF8ToJavaString(env, details->descriptionLine2()),
      base::android::ConvertUTF8ToJavaString(env, details->descriptionLine3()),
      base::android::ConvertUTF8ToJavaString(env, details->priceAttribution()),
      details->userApprovalRequired(), details->highlightTitle(),
      details->highlightLine1(), details->highlightLine2(),
      details->highlightLine3(), details->animatePlaceholders());
  Java_AssistantDetailsModel_setDetails(env, jmodel, jdetails);
}

// InfoBox related method.

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetInfoBoxModel() {
  return Java_AssistantModel_getInfoBoxModel(AttachCurrentThread(), GetModel());
}

void UiControllerAndroid::OnInfoBoxChanged(const InfoBox* info_box) {
  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetInfoBoxModel();
  if (!info_box) {
    Java_AssistantInfoBoxModel_clearInfoBox(env, jmodel);
    return;
  }

  const InfoBoxProto& proto = info_box->proto().info_box();
  auto jinfo_box = Java_AssistantInfoBox_create(
      env, base::android::ConvertUTF8ToJavaString(env, proto.image_path()),
      base::android::ConvertUTF8ToJavaString(env, proto.explanation()));
  Java_AssistantInfoBoxModel_setInfoBox(env, jmodel, jinfo_box);
}

void UiControllerAndroid::Stop(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               int jreason) {
  client_->Shutdown(static_cast<Metrics::DropOutReason>(jreason));
}

void UiControllerAndroid::OnFatalError(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& jmessage,
    int jreason) {
  if (!ui_delegate_)
    return;
  ui_delegate_->OnFatalError(
      base::android::ConvertJavaStringToUTF8(env, jmessage),
      static_cast<Metrics::DropOutReason>(jreason));
}

void UiControllerAndroid::ResetGenericUiControllers() {
  JNIEnv* env = AttachCurrentThread();
  collect_user_data_prepended_generic_ui_controller_.reset();
  collect_user_data_appended_generic_ui_controller_.reset();
  generic_ui_controller_.reset();
  auto jcollectuserdatamodel = GetCollectUserDataModel();
  Java_AssistantCollectUserDataModel_setGenericUserInterfacePrepended(
      env, jcollectuserdatamodel, nullptr);
  Java_AssistantCollectUserDataModel_setGenericUserInterfaceAppended(
      env, jcollectuserdatamodel, nullptr);
  Java_AssistantGenericUiModel_setView(env, GetGenericUiModel(), nullptr);
}

std::unique_ptr<GenericUiRootControllerAndroid>
UiControllerAndroid::CreateGenericUiControllerForProto(
    const GenericUserInterfaceProto& proto) {
  JNIEnv* env = AttachCurrentThread();
  auto jcontext =
      Java_AutofillAssistantUiController_getContext(env, java_object_);
  return GenericUiRootControllerAndroid::CreateFromProto(
      proto, base::android::ScopedJavaGlobalRef<jobject>(jcontext),
      generic_ui_delegate_.GetJavaObject(), ui_delegate_->GetEventHandler(),
      ui_delegate_->GetUserModel(), ui_delegate_->GetBasicInteractions());
}

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetGenericUiModel() {
  return Java_AssistantModel_getGenericUiModel(AttachCurrentThread(),
                                               GetModel());
}

}  // namespace autofill_assistant

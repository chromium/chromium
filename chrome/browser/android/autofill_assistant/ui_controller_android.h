// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/autofill_assistant/assistant_bottom_bar_delegate.h"
#include "chrome/browser/android/autofill_assistant/assistant_collect_user_data_delegate.h"
#include "chrome/browser/android/autofill_assistant/assistant_form_delegate.h"
#include "chrome/browser/android/autofill_assistant/assistant_generic_ui_delegate.h"
#include "chrome/browser/android/autofill_assistant/assistant_header_delegate.h"
#include "chrome/browser/android/autofill_assistant/assistant_header_model.h"
#include "chrome/browser/android/autofill_assistant/assistant_overlay_delegate.h"
#include "components/autofill_assistant/browser/chip.h"
#include "components/autofill_assistant/browser/controller_observer.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/info_box.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/overlay_state.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/user_action.h"

namespace autofill_assistant {
struct ClientSettings;
class GenericUiRootControllerAndroid;
class ClientAndroid;

// Starts and owns the UI elements required to display AA.
//
// This class and its UI elements are tied to a ChromeActivity. A
// UiControllerAndroid can be attached and detached from an AA controller, which
// is tied to a BrowserContent.
//
// TODO(crbug.com/806868): This class should be renamed to
// AssistantMediator(Android) and listen for state changes to forward those
// changes to the UI model.
class UiControllerAndroid : public ControllerObserver {
 public:
  static std::unique_ptr<UiControllerAndroid> CreateFromWebContents(
      content::WebContents* web_contents,
      const base::android::JavaParamRef<jobject>& joverlay_coordinator);

  // pointers to |web_contents|, |client| must remain valid for the lifetime of
  // this instance.
  //
  // Pointer to |ui_delegate| must remain valid for the lifetime of this
  // instance or until WillShutdown is called.
  UiControllerAndroid(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jactivity,
      const base::android::JavaParamRef<jobject>& joverlay_coordinator);
  ~UiControllerAndroid() override;

  // Attaches the UI to the given client, its web contents and delegate.
  //
  // |web_contents|, |client| and |ui_delegate| must remain valid for the
  // lifetime of this instance or until Attach() is called again, with different
  // pointers.
  void Attach(content::WebContents* web_contents,
              ClientAndroid* client,
              UiDelegate* ui_delegate);

  // Detaches the UI from |ui_delegate_|. It will stop receiving notifications
  // from the delegate until it is attached again.
  void Detach();

  // Returns true if the UI is attached to a delegate.
  bool IsAttached() { return ui_delegate_ != nullptr; }

  // Returns whether the UI is currently attached to the given delegate or not.
  bool IsAttachedTo(UiDelegate* ui_delegate) {
    return ui_delegate_ == ui_delegate;
  }

  // Have the UI react as if a close or cancel button was pressed.
  //
  // If action_index != -1, execute that action as close/cancel. Otherwise
  // execute the default close or cancel action.
  void CloseOrCancel(int action_index,
                     std::unique_ptr<TriggerContext> trigger_context,
                     Metrics::DropOutReason dropout_reason);
  // Returns the size of the window.
  base::Optional<std::pair<int, int>> GetWindowSize() const;
  // Returns the screen's orientation.
  ClientContextProto::ScreenOrientation GetScreenOrientation() const;

  // Overrides ControllerObserver:
  void OnStateChanged(AutofillAssistantState new_state) override;
  void OnStatusMessageChanged(const std::string& message) override;
  void OnBubbleMessageChanged(const std::string& message) override;
  void CloseCustomTab() override;
  void OnUserActionsChanged(const std::vector<UserAction>& actions) override;
  void OnCollectUserDataOptionsChanged(
      const CollectUserDataOptions* collect_user_data_options) override;
  void OnUserDataChanged(const UserData* state,
                         UserData::FieldChange field_change) override;
  void OnDetailsChanged(const std::vector<Details>& details) override;
  void OnInfoBoxChanged(const InfoBox* info_box) override;
  void OnProgressChanged(int progress) override;
  void OnProgressActiveStepChanged(int active_step) override;
  void OnProgressVisibilityChanged(bool visible) override;
  void OnProgressBarErrorStateChanged(bool error) override;
  void OnStepProgressBarConfigurationChanged(
      const ShowProgressBarProto::StepProgressBarConfiguration& configuration)
      override;
  void OnTouchableAreaChanged(
      const RectF& visual_viewport,
      const std::vector<RectF>& touchable_areas,
      const std::vector<RectF>& restricted_areas) override;
  void OnViewportModeChanged(ViewportMode mode) override;
  void OnPeekModeChanged(
      ConfigureBottomSheetProto::PeekMode peek_mode) override;
  void OnExpandBottomSheet() override;
  void OnCollapseBottomSheet() override;
  void OnOverlayColorsChanged(const UiDelegate::OverlayColors& colors) override;
  void OnFormChanged(const FormProto* form,
                     const FormProto::Result* result) override;
  void OnClientSettingsChanged(const ClientSettings& settings) override;
  void OnGenericUserInterfaceChanged(
      const GenericUserInterfaceProto* generic_ui) override;
  void OnShouldShowOverlayChanged(bool should_show) override;

  // Called by AssistantOverlayDelegate:
  void OnUnexpectedTaps();
  void OnUserInteractionInsideTouchableArea();

  // Called by AssistantHeaderDelegate:
  void OnHeaderFeedbackButtonClicked();

  // Called by AssistantGenericUiDelegate:
  void OnViewEvent(const EventHandler::EventKey& key);
  void OnValueChanged(const std::string& identifier, const ValueProto& value);

  // Called by AssistantCollectUserDataDelegate:
  void OnShippingAddressChanged(
      std::unique_ptr<autofill::AutofillProfile> address);
  void OnContactInfoChanged(std::unique_ptr<autofill::AutofillProfile> profile);
  void OnCreditCardChanged(
      std::unique_ptr<autofill::CreditCard> card,
      std::unique_ptr<autofill::AutofillProfile> billing_profile);
  void OnTermsAndConditionsChanged(TermsAndConditionsState state);
  void OnLoginChoiceChanged(std::string identifier);
  void OnTextLinkClicked(int link);
  void OnFormActionLinkClicked(int link);
  void OnDateTimeRangeStartDateChanged(int year, int month, int day);
  void OnDateTimeRangeStartDateCleared();
  void OnDateTimeRangeStartTimeSlotChanged(int index);
  void OnDateTimeRangeStartTimeSlotCleared();
  void OnDateTimeRangeEndDateChanged(int year, int month, int day);
  void OnDateTimeRangeEndDateCleared();
  void OnDateTimeRangeEndTimeSlotChanged(int index);
  void OnDateTimeRangeEndTimeSlotCleared();
  void OnKeyValueChanged(const std::string& key, const ValueProto& value);
  void OnTextFocusLost();
  bool IsContactComplete(autofill::AutofillProfile* contact);
  bool IsShippingAddressComplete(autofill::AutofillProfile* address);
  bool IsPaymentInstrumentComplete(autofill::CreditCard* card,
                                   autofill::AutofillProfile* address);

  // Called by AssistantFormDelegate:
  void OnCounterChanged(int input_index, int counter_index, int value);
  void OnChoiceSelectionChanged(int input_index,
                                int choice_index,
                                bool selected);

  // Called by AssistantBottomBarNativeDelegate:
  bool OnBackButtonClicked();
  void OnBottomSheetClosedWithSwipe();

  // Called by Java.
  void SnackbarResult(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jboolean undo);
  void Stop(JNIEnv* env,
            const base::android::JavaParamRef<jobject>& obj,
            int reason);
  void OnFatalError(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& message,
                    int reason);
  base::android::ScopedJavaLocalRef<jstring> GetPrimaryAccountName(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void OnUserActionSelected(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& jcaller,
                            jint index);
  void OnCancelButtonClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint actionIndex);
  void OnCloseButtonClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void OnFeedbackButtonClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint actionIndex);
  void OnKeyboardVisibilityChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean visible);
  void SetVisible(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jcaller,
                  jboolean visible);
  void OnTabSwitched(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller,
                     jint state,
                     jboolean activity_changed);
  void OnTabSelected(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller);

 private:
  // A pointer to the client. nullptr until Attach() is called.
  ClientAndroid* client_ = nullptr;

  // A pointer to the ui_delegate. nullptr until Attach() is called.
  UiDelegate* ui_delegate_ = nullptr;
  AssistantOverlayDelegate overlay_delegate_;
  AssistantHeaderDelegate header_delegate_;
  AssistantCollectUserDataDelegate collect_user_data_delegate_;
  AssistantFormDelegate form_delegate_;
  AssistantGenericUiDelegate generic_ui_delegate_;
  AssistantBottomBarDelegate bottom_bar_delegate_;

  // What to do if undo is not pressed on the current snackbar.
  base::OnceCallback<void()> snackbar_action_;

  base::android::ScopedJavaLocalRef<jobject> GetModel();
  base::android::ScopedJavaLocalRef<jobject> GetOverlayModel();
  base::android::ScopedJavaLocalRef<jobject> GetHeaderModel();
  base::android::ScopedJavaLocalRef<jobject> GetDetailsModel();
  base::android::ScopedJavaLocalRef<jobject> GetInfoBoxModel();
  base::android::ScopedJavaLocalRef<jobject> GetCollectUserDataModel();
  base::android::ScopedJavaLocalRef<jobject> GetFormModel();
  base::android::ScopedJavaLocalRef<jobject> GetGenericUiModel();

  // The UiDelegate has the last say on whether we should show the overlay.
  // This saves the AutofillAssistantState-determined OverlayState and then
  // applies it the actual UI only if the UiDelegate's ShouldShowOverlay is
  // true.
  void SetOverlayState(OverlayState state);
  // Applies the specified OverlayState to the UI.
  void ApplyOverlayState(OverlayState state);
  void AllowShowingSoftKeyboard(bool enabled);
  void ShowContentAndExpandBottomSheet();
  void SetSpinPoodle(bool enabled);
  std::string GetDebugContext();
  void DestroySelf();
  void Shutdown(Metrics::DropOutReason reason);
  void UpdateActions(const std::vector<UserAction>& GetUserActions);
  void HideKeyboardIfFocusNotOnText();

  void ResetGenericUiControllers();
  std::unique_ptr<GenericUiRootControllerAndroid>
  CreateGenericUiControllerForProto(const GenericUserInterfaceProto& proto);

  // Hide the UI, show a snackbar with an undo button, and execute the given
  // action after a short delay unless the user taps the undo button.
  void ShowSnackbar(base::TimeDelta delay,
                    const std::string& message,
                    base::OnceCallback<void()> action);

  void OnCancel(int action_index,
                std::unique_ptr<TriggerContext> context,
                Metrics::DropOutReason dropout_reason);

  // Updates the state of the UI to reflect the UIDelegate's state.
  void SetupForState();

  // Makes the whole of AA invisible or visible again.
  void SetVisible(bool visible);

  // Restore the UI for the current UIDelegate.
  void RestoreUi();

  // Java-side AutofillAssistantUiController object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // Native controllers for generic UI.
  std::unique_ptr<GenericUiRootControllerAndroid>
      collect_user_data_prepended_generic_ui_controller_;
  std::unique_ptr<GenericUiRootControllerAndroid>
      collect_user_data_appended_generic_ui_controller_;
  std::unique_ptr<GenericUiRootControllerAndroid> generic_ui_controller_;

  OverlayState desired_overlay_state_ = OverlayState::FULL;
  OverlayState overlay_state_ = OverlayState::FULL;

  std::unique_ptr<AssistantHeaderModel> header_model_;

  base::WeakPtrFactory<UiControllerAndroid> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UiControllerAndroid);
};

}  // namespace autofill_assistant.
#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_

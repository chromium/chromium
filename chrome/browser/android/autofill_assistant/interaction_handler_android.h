// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_INTERACTION_HANDLER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_INTERACTION_HANDLER_ANDROID_H_

#include <map>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/autofill_assistant/browser/event_handler.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
class BasicInteractions;
class GenericUiNestedControllerAndroid;
class UserModel;
class ViewHandlerAndroid;
class RadioButtonController;

// Receives incoming events and runs the corresponding set of callbacks.
//
// - It is NOT safe to register new interactions while listening to events!
// - This class is NOT thread-safe!
// - The lifetimes of instances should be tied to the existence of a particular
// UI.
class InteractionHandlerAndroid : public EventHandler::Observer {
 public:
  using InteractionCallback = base::RepeatingCallback<void()>;

  // Constructor. All dependencies must outlive this instance.
  InteractionHandlerAndroid(
      EventHandler* event_handler,
      UserModel* user_model,
      BasicInteractions* basic_interactions,
      ViewHandlerAndroid* view_handler,
      RadioButtonController* radio_button_controller,
      base::android::ScopedJavaGlobalRef<jobject> jcontext,
      base::android::ScopedJavaGlobalRef<jobject> jdelegate);
  ~InteractionHandlerAndroid() override;

  base::WeakPtr<InteractionHandlerAndroid> GetWeakPtr();

  void StartListening();
  void StopListening();

  // Access to the user model that this interaction handler is bound to.
  UserModel* GetUserModel() const;

  // Access to the basic interactions that this interaction handler is bound to.
  BasicInteractions* GetBasicInteractions() const;

  // Creates interaction callbacks as specified by |proto|. Returns false if
  // |proto| is invalid.
  bool AddInteractionsFromProto(const InteractionProto& proto);

  // Adds a single interaction. This can be used to add internal interactions
  // which are not exposed in the proto interface.
  void AddInteraction(const EventHandler::EventKey& key,
                      const InteractionCallback& callback);

  // Overrides autofill_assistant::EventHandler::Observer.
  void OnEvent(const EventHandler::EventKey& key) override;

  // Runs all callbacks triggered by model value changes. This is useful to
  // properly initialize a UI after inflation, since all UI state should be
  // bound to the model.
  void RunValueChangedCallbacks();

  // Creates a callback from |proto|.
  base::Optional<InteractionCallback> CreateInteractionCallbackFromProto(
      const CallbackProto& proto);

 private:
  // Deletes the nested ui controller associated with |identifier|.
  void DeleteNestedUi(const std::string& identifier);

  // Attempts to inflate |proto|. If successful, the new controller is added
  // to the list of managed nested controllers. Note that *this keeps ownership
  // of created nested UIs!
  const GenericUiNestedControllerAndroid* CreateNestedUi(
      const GenericUserInterfaceProto& proto,
      const std::string& identifier);

  void CreateAndAttachNestedGenericUi(const CreateNestedGenericUiProto& proto);
  void CreateAndShowGenericPopup(const ShowGenericUiPopupProto& proto);

  // Maps event keys to the corresponding list of callbacks to execute.
  std::map<EventHandler::EventKey, std::vector<InteractionCallback>>
      interactions_;

  EventHandler* event_handler_ = nullptr;
  UserModel* user_model_ = nullptr;
  BasicInteractions* basic_interactions_ = nullptr;
  ViewHandlerAndroid* view_handler_ = nullptr;
  RadioButtonController* radio_button_controller_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> jcontext_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> jdelegate_ = nullptr;
  bool is_listening_ = false;

  // TODO(b/154811503): move nested_ui_controllers_ to
  // generic_ui_controller_android.
  // Maps nested-ui identifiers to their instances.
  std::map<std::string, std::unique_ptr<GenericUiNestedControllerAndroid>>
      nested_ui_controllers_;

  base::WeakPtrFactory<InteractionHandlerAndroid> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(InteractionHandlerAndroid);
};

}  //  namespace autofill_assistant

#endif  //  CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_INTERACTION_HANDLER_ANDROID_H_

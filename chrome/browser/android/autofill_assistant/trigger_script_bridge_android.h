// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_TRIGGER_SCRIPT_BRIDGE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_TRIGGER_SCRIPT_BRIDGE_ANDROID_H_

#include <map>
#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"
#include "url/gurl.h"

namespace autofill_assistant {

// Facilitates communication between trigger script UI and native coordinator.
class TriggerScriptBridgeAndroid : public TriggerScriptCoordinator::Observer {
 public:
  TriggerScriptBridgeAndroid();
  ~TriggerScriptBridgeAndroid() override;
  TriggerScriptBridgeAndroid(const TriggerScriptBridgeAndroid&) = delete;
  TriggerScriptBridgeAndroid& operator=(const TriggerScriptBridgeAndroid&) =
      delete;

  // Attempts to start a trigger script on |initial_url|. Will communicate with
  // |jdelegate| to show/hide UI as necessary.
  void StartTriggerScript(content::WebContents* web_contents,
                          const base::android::JavaParamRef<jobject>& jdelegate,
                          const GURL& initial_url,
                          std::unique_ptr<TriggerContext> trigger_context,
                          jlong jservice_request_sender);

  // Stops and destroys the current trigger script, if any. Also disconnects the
  // java-side delegate.
  void StopTriggerScript();

  // Called by the UI to execute a specific trigger script action.
  void OnTriggerScriptAction(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint action);

  // Called by the UI when the bottom sheet has been swipe-dismissed.
  void OnBottomSheetClosedWithSwipe(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  // Called by the UI when the back button was pressed. Returns whether the
  // event was handled or not.
  bool OnBackButtonPressed(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jcaller);

  // Called by the UI when the tab's interactability has changed.
  void OnTabInteractabilityChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean jinteractable);

  // Access to the last shown trigger script.
  base::Optional<TriggerScriptUIProto> GetLastShownTriggerScript() const;

  // Clears the last shown trigger script.
  void ClearLastShownTriggerScript();

  // Called by the UI when the keyboard was shown or hidden.
  void OnKeyboardVisibilityChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean jvisible);

  // Called by the UI when a user interacted with the onboarding UI or when
  // onboarding is already accepted.
  void OnOnboardingFinished(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& jcaller,
                            jboolean jonboarding_shown,
                            jint jresult);

 private:
  // From TriggerScriptCoordinator::Observer:
  void OnTriggerScriptShown(const TriggerScriptUIProto& proto) override;
  void OnTriggerScriptHidden() override;
  void OnTriggerScriptFinished(Metrics::LiteScriptFinishedState state) override;
  void OnVisibilityChanged(bool visible) override;
  void OnOnboardingRequested(bool is_dialog_onboarding_enabled) override;

  // The login manager for fetching login credentials.
  // TODO(arbesser) move this to the owner of trigger_script_bridge_android.
  std::unique_ptr<WebsiteLoginManager> website_login_manager_;

  // Reference to the Java counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  bool disable_header_animations_for_testing_ = false;
  base::Optional<TriggerScriptUIProto> last_shown_trigger_script_;
  std::unique_ptr<TriggerScriptCoordinator> trigger_script_coordinator_;
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_TRIGGER_SCRIPT_BRIDGE_ANDROID_H_

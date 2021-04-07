// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_CLIENT_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_CLIENT_ANDROID_H_

#include <map>
#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/android/autofill_assistant/trigger_script_bridge_android.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/service/access_token_fetcher.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill_assistant {

// Creates a Autofill Assistant client associated with a WebContents.
//
// To obtain an instance of this class from the C++ side, call
// ClientAndroid::FromWebContents(web_contents). To make sure an instance
// exists, call ClientAndroid::CreateForWebContents first.
//
// From the Java side, call AutofillAssistantClient.fromWebContents.
//
// This class is accessible from the Java side through AutofillAssistantClient.
class ClientAndroid : public Client,
                      public AccessTokenFetcher,
                      public content::WebContentsUserData<ClientAndroid> {
 public:
  ~ClientAndroid() override;

  base::WeakPtr<ClientAndroid> GetWeakPtr();

  // Returns the corresponding Java AutofillAssistantClient.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  bool Start(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& jcaller,
             const base::android::JavaParamRef<jstring>& jinitial_url,
             const base::android::JavaParamRef<jstring>& jexperiment_ids,
             const base::android::JavaParamRef<jobjectArray>& parameter_names,
             const base::android::JavaParamRef<jobjectArray>& parameter_values,
             jboolean jis_cct,
             const base::android::JavaParamRef<jobject>& joverlay_coordinator,
             jboolean jonboarding_shown,
             jlong jservice);
  void OnJavaDestroyUI(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& jcaller);
  void TransferUITo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jother_web_contents);

  void StartTriggerScript(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jdelegate,
      const base::android::JavaParamRef<jstring>& jinitial_url,
      const base::android::JavaParamRef<jstring>& jexperiment_ids,
      const base::android::JavaParamRef<jobjectArray>& jparameter_names,
      const base::android::JavaParamRef<jobjectArray>& jparameter_values,
      jlong jservice_request_sender);

  base::android::ScopedJavaLocalRef<jstring> GetPrimaryAccountName(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void OnAccessToken(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller,
                     jboolean success,
                     const base::android::JavaParamRef<jstring>& access_token);

  void FetchWebsiteActions(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jexperiment_ids,
      const base::android::JavaParamRef<jobjectArray>& jargument_names,
      const base::android::JavaParamRef<jobjectArray>& jargument_values,
      const base::android::JavaParamRef<jobject>& jcallback);

  bool HasRunFirstCheck(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller) const;

  base::android::ScopedJavaLocalRef<jobjectArray> GetDirectActions(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  bool PerformDirectAction(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jaction_id,
      const base::android::JavaParamRef<jstring>& jexperiment_ids,
      const base::android::JavaParamRef<jobjectArray>& jargument_names,
      const base::android::JavaParamRef<jobjectArray>& jargument_values,
      const base::android::JavaParamRef<jobject>& joverlay_coordinator);

  // Overrides Client
  void AttachUI() override;
  void DestroyUI() override;
  version_info::Channel GetChannel() const override;
  std::string GetEmailAddressForAccessTokenAccount() const override;
  std::string GetChromeSignedInEmailAddress() const override;
  base::Optional<std::pair<int, int>> GetWindowSize() const override;
  ClientContextProto::ScreenOrientation GetScreenOrientation() const override;
  AccessTokenFetcher* GetAccessTokenFetcher() override;
  autofill::PersonalDataManager* GetPersonalDataManager() const override;
  WebsiteLoginManager* GetWebsiteLoginManager() const override;
  std::string GetLocale() const override;
  std::string GetCountryCode() const override;
  DeviceContext GetDeviceContext() const override;
  bool IsAccessibilityEnabled() const override;
  content::WebContents* GetWebContents() const override;
  void Shutdown(Metrics::DropOutReason reason) override;
  void RecordDropOut(Metrics::DropOutReason reason) override;
  bool HasHadUI() const override;

  // Overrides AccessTokenFetcher
  void FetchAccessToken(
      base::OnceCallback<void(bool, const std::string&)>) override;
  void InvalidateAccessToken(const std::string& access_token) override;

 private:
  friend class content::WebContentsUserData<ClientAndroid>;

  explicit ClientAndroid(content::WebContents* web_contents);

  void CreateController(std::unique_ptr<Service> service);
  void DestroyController();
  void AttachUI(
      const base::android::JavaParamRef<jobject>& joverlay_coordinator);
  bool NeedsUI();
  void OnFetchWebsiteActions(const base::android::JavaRef<jobject>& jcallback);
  void SafeDestroyControllerAndUI(Metrics::DropOutReason reason);

  base::android::ScopedJavaLocalRef<jobjectArray>
  GetDirectActionsAsJavaArrayOfStrings(JNIEnv* env) const;

  base::android::ScopedJavaLocalRef<jobject>
  ToJavaAutofillAssistantDirectAction(JNIEnv* env,
                                      const DirectAction& direct_action) const;

  // Returns the index of a direct action with that name, to pass to
  // UiDelegate::PerformUserAction() or -1 if not found.
  int FindDirectAction(const std::string& action_name);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  content::WebContents* web_contents_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  std::unique_ptr<Controller> controller_;
  mutable std::unique_ptr<WebsiteLoginManager> website_login_manager_;

  // True if Start() was called. This turns on the tracking of dropouts.
  bool started_ = false;

  // Intent parameter used for tracking dropouts per intent.
  // TODO(b/182164683) Do not store intent paramenter in |ClientAndroid|.
  std::string intent_;

  // True if the UI was ever attached.
  bool has_had_ui_ = false;

  std::unique_ptr<UiControllerAndroid> ui_controller_android_;

  // Bridge that allows Java to start trigger scripts.
  TriggerScriptBridgeAndroid trigger_script_bridge_;

  base::OnceCallback<void(bool, const std::string&)>
      fetch_access_token_callback_;

  base::WeakPtrFactory<ClientAndroid> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClientAndroid);
};

}  // namespace autofill_assistant.
#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_CLIENT_ANDROID_H_

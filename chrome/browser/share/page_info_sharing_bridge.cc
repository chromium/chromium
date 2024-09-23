// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/page_info_sharing_bridge.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/translate/content/browser/content_record_page_language.h"
#include "components/translate/core/browser/language_state.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/share/jni_headers/PageInfoSharingBridge_jni.h"

using base::android::JavaParamRef;

jboolean JNI_PageInfoSharingBridge_DoesProfileSupportPageInfo(
    JNIEnv* env,
    Profile* profile) {
  if (profile->IsOffTheRecord()) {
    profile = profile->GetOriginalProfile();
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return sharing::DoesProfileSupportPageInfo(identity_manager);
}

jboolean JNI_PageInfoSharingBridge_DoesTabSupportPageInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab_android) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab_android);
  if (!tab) {
    return false;
  }
  content::WebContents* web_contents = tab->web_contents();
  return sharing::DoesWebContentsSupportPageInfo(web_contents);
}

namespace sharing {

bool DoesProfileSupportPageInfo(signin::IdentityManager* identity_manager) {
  const auto account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (account_id.empty()) {
    return false;
  }
  const auto& account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(account_id);
  return account_info.capabilities.can_use_model_execution_features() !=
         signin::Tribool::kFalse;
}

bool DoesWebContentsSupportPageInfo(content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  if (!client) {
    return false;
  }
  const std::string& language_code =
      client->GetLanguageState().source_language();
  return language_code == "en";
}

}  // namespace sharing

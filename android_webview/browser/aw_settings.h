// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_SETTINGS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_SETTINGS_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/web_contents_observer.h"

namespace blink {
namespace web_pref {
struct WebPreferences;
}
}  // namespace blink

namespace android_webview {

class AwContentsOriginMatcher;
class AwRenderViewHostExt;

// Lifetime: WebView
class AwSettings : public content::WebContentsObserver {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview.settings
  enum ForceDarkMode {
    FORCE_DARK_OFF = 0,
    FORCE_DARK_AUTO = 1,
    FORCE_DARK_ON = 2,
  };

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview.settings
  enum ForceDarkBehavior {
    FORCE_DARK_ONLY = 0,
    MEDIA_QUERY_ONLY = 1,
    PREFER_MEDIA_QUERY_OVER_FORCE_DARK = 2,
  };

  enum RequestedWithHeaderMode {
    NO_HEADER = 0,
    APP_PACKAGE_NAME = 1,
    CONSTANT_WEBVIEW = 2,
  };

  enum MixedContentMode {
    MIXED_CONTENT_ALWAYS_ALLOW = 0,
    MIXED_CONTENT_NEVER_ALLOW = 1,
    MIXED_CONTENT_COMPATIBILITY_MODE = 2,
    COUNT,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview.settings
  enum AttributionBehavior {
    DISABLED = 0,
    APP_SOURCE_AND_WEB_TRIGGER = 1,
    WEB_SOURCE_AND_WEB_TRIGGER = 2,
    APP_SOURCE_AND_APP_TRIGGER = 3,
    kMaxValue = APP_SOURCE_AND_APP_TRIGGER,
  };

  static AwSettings* FromWebContents(content::WebContents* web_contents);
  static bool GetAllowSniffingFileUrls();

  // Static accessor to get the currently configured default value based
  // on feature flags and trial config
  static RequestedWithHeaderMode GetDefaultRequestedWithHeaderMode();

  AwSettings(JNIEnv* env, jobject obj, content::WebContents* web_contents);
  ~AwSettings() override;

  bool GetAllowFileAccessFromFileURLs();
  bool GetJavaScriptEnabled();
  bool GetJavaScriptCanOpenWindowsAutomatically();
  bool GetAllowThirdPartyCookies();
  MixedContentMode GetMixedContentMode();
  AttributionBehavior GetAttributionBehavior();

  // Called from Java. Methods with "Locked" suffix require that the settings
  // access lock is held during their execution.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void PopulateWebPreferencesLocked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong web_prefs);
  void ResetScrollAndScaleState(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void UpdateEverythingLocked(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);
  void UpdateInitialPageScaleLocked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void UpdateWillSuppressErrorStateLocked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void UpdateUserAgentLocked(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);
  void UpdateWebkitPreferencesLocked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void UpdateRendererPreferencesLocked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void UpdateJavaScriptPolicyLocked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void UpdateCookiePolicyLocked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void UpdateOffscreenPreRasterLocked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void UpdateAllowFileAccessLocked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void UpdateMixedContentModeLocked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void UpdateAttributionBehaviorLocked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void PopulateWebPreferences(blink::web_pref::WebPreferences* web_prefs);
  bool GetAllowFileAccess();
  bool IsForceDarkApplied(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);
  bool PrefersDarkFromTheme(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);

  void SetEnterpriseAuthenticationAppLinkPolicyEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean enabled);
  bool GetEnterpriseAuthenticationAppLinkPolicyEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  inline bool enterprise_authentication_app_link_policy_enabled() {
    return enterprise_authentication_app_link_policy_enabled_;
  }

  base::android::ScopedJavaLocalRef<jobjectArray>
  UpdateXRequestedWithAllowListOriginMatcher(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& rules);
  scoped_refptr<AwContentsOriginMatcher> xrw_allowlist_matcher();

 private:
  AwRenderViewHostExt* GetAwRenderViewHostExt();
  void UpdateEverything();

  // WebContentsObserver overrides:
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void WebContentsDestroyed() override;

  bool renderer_prefs_initialized_{false};
  bool javascript_enabled_{false};
  bool javascript_can_open_windows_automatically_{false};
  bool allow_third_party_cookies_{false};
  bool allow_file_access_{false};
  bool allow_file_access_from_file_urls_{false};
  // TODO(b/222053757,ayushsha): Change this policy to be by
  // default false from next Android version(Maybe Android U).
  bool enterprise_authentication_app_link_policy_enabled_{true};
  MixedContentMode mixed_content_mode_;
  AttributionBehavior attribution_behavior_;

  scoped_refptr<AwContentsOriginMatcher> xrw_allowlist_matcher_;

  JavaObjectWeakGlobalRef aw_settings_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_SETTINGS_H_

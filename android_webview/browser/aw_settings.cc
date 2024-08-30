// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_settings.h"

#include <memory>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_client_hints_controller_delegate.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_contents_origin_matcher.h"
#include "android_webview/browser/aw_dark_mode.h"
#include "android_webview/browser/aw_user_agent_metadata.h"
#include "android_webview/browser/metrics/aw_metrics_service_accessor.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"
#include "android_webview/common/aw_content_client.h"
#include "android_webview/common/aw_features.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/supports_user_data.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/viz/common/features.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/renderer_preferences_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwSettings_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using blink::web_pref::WebPreferences;

namespace android_webview {

namespace {

// Metrics on the count of difference cases when we populate the user-agent
// metadata. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class UserAgentMetadataAvailableType {
  kSystemDefault = 0,
  kSystemDefaultLowEntropyOnly = 1,
  kUserOverrides = 2,
  kMaxValue = kUserOverrides,
};

void LogUserAgentMetadataAvailableType(UserAgentMetadataAvailableType type) {
  base::UmaHistogramEnumeration(
      "Android.WebView.UserAgentClientHintsMetadata.AvailableType", type);
}

void PopulateFixedWebPreferences(WebPreferences* web_prefs) {
  web_prefs->shrinks_standalone_images_to_fit = false;
  web_prefs->should_clear_document_background = false;
  web_prefs->viewport_meta_enabled = true;
  web_prefs->picture_in_picture_enabled = false;
  web_prefs->disable_accelerated_small_canvases = true;
  // WebView has historically not adjusted font scale for text autosizing.
  web_prefs->device_scale_adjustment = 1.0;
}

const void* const kAwSettingsUserDataKey = &kAwSettingsUserDataKey;

}  // namespace

class AwSettingsUserData : public base::SupportsUserData::Data {
 public:
  explicit AwSettingsUserData(AwSettings* ptr) : settings_(ptr) {}

  static AwSettings* GetSettings(content::WebContents* web_contents) {
    if (!web_contents)
      return NULL;
    AwSettingsUserData* data = static_cast<AwSettingsUserData*>(
        web_contents->GetUserData(kAwSettingsUserDataKey));
    return data ? data->settings_.get() : NULL;
  }

 private:
  raw_ptr<AwSettings> settings_;
};

AwSettings::AwSettings(JNIEnv* env,
                       const jni_zero::JavaRef<jobject>& obj,
                       content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      xrw_allowlist_matcher_(base::MakeRefCounted<AwContentsOriginMatcher>()),
      aw_settings_(env, obj) {
  web_contents->SetUserData(kAwSettingsUserDataKey,
                            std::make_unique<AwSettingsUserData>(this));
}

AwSettings::~AwSettings() {
  if (web_contents()) {
    web_contents()->SetUserData(kAwSettingsUserDataKey, NULL);
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> scoped_obj = aw_settings_.get(env);
  if (!scoped_obj)
    return;
  Java_AwSettings_nativeAwSettingsGone(env, scoped_obj,
                                       reinterpret_cast<intptr_t>(this));
}

bool AwSettings::GetJavaScriptCanOpenWindowsAutomatically() {
  return javascript_can_open_windows_automatically_;
}

bool AwSettings::GetAllowThirdPartyCookies() {
  return allow_third_party_cookies_;
}

bool AwSettings::GetJavaScriptEnabled() {
  return javascript_enabled_;
}

AwSettings::MixedContentMode AwSettings::GetMixedContentMode() {
  return mixed_content_mode_;
}

AwSettings::AttributionBehavior AwSettings::GetAttributionBehavior() {
  return attribution_behavior_;
}

bool AwSettings::IsPrerender2Allowed() {
  return (speculative_loading_allowed_flags_ & PRERENDER_ENABLED);
}

bool AwSettings::IsBackForwardCacheEnabled() {
  return bfcache_enabled_in_java_settings_;
}

void AwSettings::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

AwSettings* AwSettings::FromWebContents(content::WebContents* web_contents) {
  return AwSettingsUserData::GetSettings(web_contents);
}

bool AwSettings::GetAllowSniffingFileUrls() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AwSettings_getAllowSniffingFileUrls(env);
}

AwSettings::RequestedWithHeaderMode
AwSettings::GetDefaultRequestedWithHeaderMode() {
  // If the control feature is not enabled, the default is the old behavior,
  // which is to send the app package name.
  if (!base::FeatureList::IsEnabled(
          features::kWebViewXRequestedWithHeaderControl))
    return AwSettings::RequestedWithHeaderMode::APP_PACKAGE_NAME;

  int configuredValue = features::kWebViewXRequestedWithHeaderMode.Get();
  switch (configuredValue) {
    case AwSettings::RequestedWithHeaderMode::CONSTANT_WEBVIEW:
      return AwSettings::RequestedWithHeaderMode::CONSTANT_WEBVIEW;
    case AwSettings::RequestedWithHeaderMode::NO_HEADER:
      return AwSettings::RequestedWithHeaderMode::NO_HEADER;
    default:
      // If the field trial config is broken for some reason, use the
      // package name.
      return AwSettings::RequestedWithHeaderMode::APP_PACKAGE_NAME;
  }
}

AwRenderViewHostExt* AwSettings::GetAwRenderViewHostExt() {
  if (!web_contents())
    return NULL;
  AwContents* contents = AwContents::FromWebContents(web_contents());
  if (!contents)
    return NULL;
  return contents->render_view_host_ext();
}

void AwSettings::ResetScrollAndScaleState(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  AwRenderViewHostExt* rvhe = GetAwRenderViewHostExt();
  if (!rvhe)
    return;
  rvhe->ResetScrollAndScaleState();
}

void AwSettings::UpdateEverything() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  ScopedJavaLocalRef<jobject> scoped_obj = aw_settings_.get(env);
  if (!scoped_obj)
    return;
  // Grab the lock and call UpdateEverythingLocked.
  Java_AwSettings_updateEverything(env, scoped_obj);
}

void AwSettings::UpdateEverythingLocked(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  base::AutoReset<bool> auto_reset(&in_update_everything_locked_, true);
  UpdateInitialPageScaleLocked(env, obj);
  UpdateWebkitPreferencesLocked(env, obj);
  UpdateUserAgentLocked(env, obj);
  ResetScrollAndScaleState(env, obj);
  UpdateRendererPreferencesLocked(env, obj);
  UpdateOffscreenPreRasterLocked(env, obj);
  UpdateWillSuppressErrorStateLocked(env, obj);
  UpdateCookiePolicyLocked(env, obj);
  UpdateJavaScriptPolicyLocked(env, obj);
  UpdateAllowFileAccessLocked(env, obj);
  UpdateMixedContentModeLocked(env, obj);
  UpdateAttributionBehaviorLocked(env, obj);
  UpdateSpeculativeLoadingAllowedLocked(env, obj);
  UpdateBackForwardCacheEnabledLocked(env, obj);
  UpdateGeolocationEnabledLocked(env, obj);
}

void AwSettings::UpdateUserAgentLocked(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;

  ScopedJavaLocalRef<jstring> str =
      Java_AwSettings_getUserAgentLocked(env, obj);
  bool ua_overidden = !!str;
  bool ua_metadata_overridden =
      Java_AwSettings_getHasUserAgentMetadataOverridesLocked(env, obj);

  if (ua_overidden) {
    std::string ua_string_override = ConvertJavaStringToUTF8(str);
    std::string ua_default = GetUserAgent();
    blink::UserAgentOverride override_ua_with_metadata;
    override_ua_with_metadata.ua_string_override = ua_string_override;

    // If kUACHOverrideBlank is enabled, set user-agent metadata with the
    // default blank value.
    if (!ua_string_override.empty() &&
        base::FeatureList::IsEnabled(blink::features::kUACHOverrideBlank)) {
      override_ua_with_metadata.ua_metadata_override =
          blink::UserAgentMetadata();
    }

    // Generate user-agent client hints in the following three cases:
    // 1. If user provide the user-agent metadata overrides, we use the
    // override data to populate the user-agent client hints.
    // 2. Otherwise, if override user-agent contains default user-agent, we
    // use system default user-agent metadata to populate the user-agent
    // client hints.
    // 3. Finally, if the above two cases don't match, we only populate system
    // default low-entropy client hints.
    if (ua_metadata_overridden) {
      ScopedJavaLocalRef<jobject> java_ua_metadata =
          Java_AwSettings_getUserAgentMetadataLocked(env, obj);
      override_ua_with_metadata.ua_metadata_override =
          FromJavaAwUserAgentMetadata(env, java_ua_metadata);
      LogUserAgentMetadataAvailableType(
          UserAgentMetadataAvailableType::kUserOverrides);
    } else if (base::Contains(ua_string_override, ua_default)) {
      override_ua_with_metadata.ua_metadata_override =
          AwClientHintsControllerDelegate::GetUserAgentMetadataOverrideBrand();
      LogUserAgentMetadataAvailableType(
          UserAgentMetadataAvailableType::kSystemDefault);
    } else {
      override_ua_with_metadata.ua_metadata_override =
          AwClientHintsControllerDelegate::GetUserAgentMetadataOverrideBrand(
              /*only_low_entropy_ch=*/true);
      LogUserAgentMetadataAvailableType(
          UserAgentMetadataAvailableType::kSystemDefaultLowEntropyOnly);
    }

    // Set overridden user-agent and default client hints metadata if applied.
    web_contents()->SetUserAgentOverride(override_ua_with_metadata, true);
  }

  content::NavigationController& controller = web_contents()->GetController();
  for (int i = 0; i < controller.GetEntryCount(); ++i)
    controller.GetEntryAtIndex(i)->SetIsOverridingUserAgent(ua_overidden);
}

void AwSettings::UpdateWebkitPreferencesLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;
  AwRenderViewHostExt* render_view_host_ext = GetAwRenderViewHostExt();
  if (!render_view_host_ext)
    return;

  web_contents()->OnWebPreferencesChanged();
}

void AwSettings::UpdateInitialPageScaleLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  AwRenderViewHostExt* rvhe = GetAwRenderViewHostExt();
  if (!rvhe)
    return;

  float initial_page_scale_percent =
      Java_AwSettings_getInitialPageScalePercentLocked(env, obj);
  if (initial_page_scale_percent == 0) {
    rvhe->SetInitialPageScale(-1);
  } else {
    float dip_scale =
        static_cast<float>(Java_AwSettings_getDIPScaleLocked(env, obj));
    rvhe->SetInitialPageScale(initial_page_scale_percent / dip_scale / 100.0f);
    initial_page_scale_is_non_default_ = true;
  }
}

void AwSettings::UpdateWillSuppressErrorStateLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  AwRenderViewHostExt* rvhe = GetAwRenderViewHostExt();
  if (!rvhe)
    return;

  bool suppress = Java_AwSettings_getWillSuppressErrorPageLocked(env, obj);
  rvhe->SetWillSuppressErrorPage(suppress);
}

void AwSettings::UpdateRendererPreferencesLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;

  bool update_prefs = false;
  blink::RendererPreferences* prefs = web_contents()->GetMutableRendererPrefs();

  if (!renderer_prefs_initialized_) {
    content::UpdateFontRendererPreferencesFromSystemSettings(prefs);
    renderer_prefs_initialized_ = true;
    update_prefs = true;
  }

  if (prefs->accept_languages.compare(
          AwContentBrowserClient::GetAcceptLangsImpl())) {
    prefs->accept_languages = AwContentBrowserClient::GetAcceptLangsImpl();
    update_prefs = true;
  }

  if (update_prefs)
    web_contents()->SyncRendererPrefs();

  if (update_prefs) {
    // make sure to update accept languages when the network service is enabled
    AwBrowserContext* aw_browser_context =
        AwBrowserContext::FromWebContents(web_contents());
    // AndroidWebview does not use per-site storage partitions.
    content::StoragePartition* storage_partition =
        aw_browser_context->GetDefaultStoragePartition();
    std::string expanded_language_list =
        net::HttpUtil::ExpandLanguageList(prefs->accept_languages);
    storage_partition->GetNetworkContext()->SetAcceptLanguage(
        net::HttpUtil::GenerateAcceptLanguageHeader(expanded_language_list));
  }
}

void AwSettings::UpdateCookiePolicyLocked(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;

  allow_third_party_cookies_ =
      Java_AwSettings_getAcceptThirdPartyCookiesLocked(env, obj);
}

void AwSettings::UpdateJavaScriptPolicyLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;

  javascript_enabled_ = Java_AwSettings_getJavaScriptEnabledLocked(env, obj);
}

void AwSettings::UpdateOffscreenPreRasterLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  AwContents* contents = AwContents::FromWebContents(web_contents());
  if (contents) {
    contents->SetOffscreenPreRaster(
        Java_AwSettings_getOffscreenPreRasterLocked(env, obj));
  }
}

void AwSettings::UpdateAllowFileAccessLocked(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;

  allow_file_access_ = Java_AwSettings_getAllowFileAccess(env, obj);
}

void AwSettings::UpdateMixedContentModeLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;

  mixed_content_mode_ = static_cast<MixedContentMode>(
      Java_AwSettings_getMixedContentMode(env, obj));
}

void AwSettings::UpdateAttributionBehaviorLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!web_contents()) {
    return;
  }

  AttributionBehavior previous = attribution_behavior_;
  attribution_behavior_ = static_cast<AttributionBehavior>(
      Java_AwSettings_getAttributionBehavior(env, obj));

  base::UmaHistogramEnumeration("Conversions.AttributionBehavior",
                                attribution_behavior_);

  // If attribution was previously disabled or has now been disabled, then
  // we need to update attribution support values in the renderer.
  if (previous != attribution_behavior_ &&
      (previous == AwSettings::AttributionBehavior::DISABLED ||
       attribution_behavior_ == AwSettings::AttributionBehavior::DISABLED)) {
    web_contents()->UpdateAttributionSupportRenderer();
  }
}

void AwSettings::UpdateSpeculativeLoadingAllowedLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  SpeculativeLoadingAllowedFlags previous = speculative_loading_allowed_flags_;
  speculative_loading_allowed_flags_ =
      static_cast<SpeculativeLoadingAllowedFlags>(
          Java_AwSettings_getSpeculativeLoadingAllowed(env, obj));

  if (!in_update_everything_locked_) {
    // The setting was explicitly updated, since this is not part of the
    // UpdateEverythingLocked call. Register a synthetic field trial so that
    // even if we do experiments that are not run via Finch, we can still
    // identify the "Prerender enabled" vs "Prerender disabled" groups for UMA
    // and crash comparison. Note that we only register when the setting was
    // explicitly updated, to exclude cases that are not part of any experiment
    // groups at all (e.g. when we're on a version of the WebView embedder that
    // doesn't have the experiment at all).
    static constexpr char kPrerenderTrial[] = "WebViewPrerenderSynthetic";
    AwMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        AwMetricsServiceClient::GetInstance()->GetMetricsService(),
        kPrerenderTrial,
        (speculative_loading_allowed_flags_ & AwSettings::PRERENDER_ENABLED)
            ? "Enabled"
            : "Disabled",
        variations::SyntheticTrialAnnotationMode::kNextLog);
  }

  if (previous == speculative_loading_allowed_flags_) {
    return;
  }

  if (!web_contents()) {
    // No need to cancel preloading entries if the WebContents that host them
    // doesn't exist.
    return;
  }

  // TODO(crbug.com/339561855): Clear navigational prefetches when
  // preloading is disabled.

  if ((previous & AwSettings::PRERENDER_ENABLED) &&
      !(speculative_loading_allowed_flags_ & AwSettings::PRERENDER_ENABLED)) {
    web_contents()->CancelAllPrerendering();
  }
}

void AwSettings::UpdateBackForwardCacheEnabledLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  bool bfcache_enabled_by_feature_flag =
      base::FeatureList::IsEnabled(features::kWebViewBackForwardCache);
  bool previous_enabled =
      bfcache_enabled_in_java_settings_ || bfcache_enabled_by_feature_flag;
  bfcache_enabled_in_java_settings_ =
      Java_AwSettings_getBackForwardCacheEnabled(env, obj);
  bool current_enabled =
      bfcache_enabled_in_java_settings_ || bfcache_enabled_by_feature_flag;

  if (!current_enabled && previous_enabled && web_contents()) {
    AwContents* contents = AwContents::FromWebContents(web_contents());
    contents->FlushBackForwardCache(
        env, static_cast<int>(content::BackForwardCache::NotRestoredReason::
                                  kWebViewSettingsChanged));
  }

  if (!in_update_everything_locked_) {
    // The setting was explicitly updated, since this is not part of the
    // UpdateEverythingLocked call. Register a synthetic field trial so that
    // even if we do experiments that are not run via Finch, we can still
    // identify the "BFCache enabled" vs "BFCache disabled" groups for UMA and
    // crash comparison. Note that we only register when the setting was
    // explicitly updated, to exclude cases that are not part of any experiment
    // groups at all (e.g. when we're on a version of the WebView embedder that
    // doesn't have the experiment at all).
    static constexpr char kBackForwardCacheTrial[] =
        "WebViewBackForwardCacheSynthetic";
    AwMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        AwMetricsServiceClient::GetInstance()->GetMetricsService(),
        kBackForwardCacheTrial,
        bfcache_enabled_in_java_settings_ ? "Enabled" : "Disabled",
        variations::SyntheticTrialAnnotationMode::kNextLog);
  }
}

void AwSettings::UpdateGeolocationEnabledLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!web_contents()) {
    return;
  }

  geolocation_enabled_ = Java_AwSettings_getGeolocationEnabled(env, obj);
}

void AwSettings::RenderViewHostChanged(content::RenderViewHost* old_host,
                                       content::RenderViewHost* new_host) {
  DCHECK_EQ(new_host, web_contents()->GetRenderViewHost());

  UpdateEverything();
}

void AwSettings::WebContentsDestroyed() {
  delete this;
}

void AwSettings::PopulateWebPreferences(WebPreferences* web_prefs) {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  ScopedJavaLocalRef<jobject> scoped_obj = aw_settings_.get(env);
  if (!scoped_obj)
    return;
  // Grab the lock and call PopulateWebPreferencesLocked.
  Java_AwSettings_populateWebPreferences(env, scoped_obj,
                                         reinterpret_cast<jlong>(web_prefs));
}

void AwSettings::PopulateWebPreferencesLocked(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              jlong web_prefs_ptr) {
  AwRenderViewHostExt* render_view_host_ext = GetAwRenderViewHostExt();
  if (!render_view_host_ext)
    return;

  WebPreferences* web_prefs = reinterpret_cast<WebPreferences*>(web_prefs_ptr);
  PopulateFixedWebPreferences(web_prefs);

  web_prefs->text_autosizing_enabled =
      Java_AwSettings_getTextAutosizingEnabledLocked(env, obj);

  int text_size_percent = Java_AwSettings_getTextSizePercentLocked(env, obj);
  if (web_prefs->text_autosizing_enabled) {
    web_prefs->font_scale_factor = text_size_percent / 100.0f;
    web_prefs->force_enable_zoom = text_size_percent >= 130;
    // Use the default zoom factor value when Text Autosizer is turned on.
    render_view_host_ext->SetTextZoomFactor(1);
  } else {
    web_prefs->force_enable_zoom = false;
    render_view_host_ext->SetTextZoomFactor(text_size_percent / 100.0f);
  }

  web_prefs->standard_font_family_map[blink::web_pref::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getStandardFontFamilyLocked(env, obj));

  web_prefs->fixed_font_family_map[blink::web_pref::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getFixedFontFamilyLocked(env, obj));

  web_prefs->sans_serif_font_family_map[blink::web_pref::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getSansSerifFontFamilyLocked(env, obj));

  web_prefs->serif_font_family_map[blink::web_pref::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getSerifFontFamilyLocked(env, obj));

  web_prefs->cursive_font_family_map[blink::web_pref::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getCursiveFontFamilyLocked(env, obj));

  web_prefs->fantasy_font_family_map[blink::web_pref::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getFantasyFontFamilyLocked(env, obj));

  web_prefs->default_encoding = ConvertJavaStringToUTF8(
      Java_AwSettings_getDefaultTextEncodingLocked(env, obj));

  web_prefs->minimum_font_size =
      Java_AwSettings_getMinimumFontSizeLocked(env, obj);

  web_prefs->minimum_logical_font_size =
      Java_AwSettings_getMinimumLogicalFontSizeLocked(env, obj);

  web_prefs->default_font_size =
      Java_AwSettings_getDefaultFontSizeLocked(env, obj);

  web_prefs->default_fixed_font_size =
      Java_AwSettings_getDefaultFixedFontSizeLocked(env, obj);

  // Blink's LoadsImagesAutomatically and ImagesEnabled must be
  // set cris-cross to Android's. See
  // https://code.google.com/p/chromium/issues/detail?id=224317#c26
  web_prefs->loads_images_automatically =
      Java_AwSettings_getImagesEnabledLocked(env, obj);
  web_prefs->images_enabled =
      Java_AwSettings_getLoadsImagesAutomaticallyLocked(env, obj);

  web_prefs->javascript_enabled =
      Java_AwSettings_getJavaScriptEnabledLocked(env, obj);

  web_prefs->allow_universal_access_from_file_urls =
      Java_AwSettings_getAllowUniversalAccessFromFileURLsLocked(env, obj);

  allow_file_access_from_file_urls_ =
      Java_AwSettings_getAllowFileAccessFromFileURLsLocked(env, obj);
  web_prefs->allow_file_access_from_file_urls =
      allow_file_access_from_file_urls_;

  javascript_can_open_windows_automatically_ =
      Java_AwSettings_getJavaScriptCanOpenWindowsAutomaticallyLocked(env, obj);

  web_prefs->supports_multiple_windows =
      Java_AwSettings_getSupportMultipleWindowsLocked(env, obj);

  web_prefs->plugins_enabled = false;

  web_prefs->local_storage_enabled =
      Java_AwSettings_getDomStorageEnabledLocked(env, obj);

  web_prefs->databases_enabled =
      Java_AwSettings_getDatabaseEnabledLocked(env, obj);

  web_prefs->wide_viewport_quirk = true;
  web_prefs->use_wide_viewport =
      Java_AwSettings_getUseWideViewportLocked(env, obj);

  web_prefs->force_zero_layout_height =
      Java_AwSettings_getForceZeroLayoutHeightLocked(env, obj);

  const bool zero_layout_height_disables_viewport_quirk =
      Java_AwSettings_getZeroLayoutHeightDisablesViewportQuirkLocked(env, obj);
  web_prefs->viewport_enabled = !(zero_layout_height_disables_viewport_quirk &&
                                  web_prefs->force_zero_layout_height);

  web_prefs->double_tap_to_zoom_enabled =
      Java_AwSettings_supportsDoubleTapZoomLocked(env, obj);

  web_prefs->initialize_at_minimum_page_scale =
      Java_AwSettings_getLoadWithOverviewModeLocked(env, obj);

  initial_page_scale_is_non_default_ |=
      (web_prefs->initialize_at_minimum_page_scale);

  web_prefs->autoplay_policy =
      Java_AwSettings_getMediaPlaybackRequiresUserGestureLocked(env, obj)
          ? blink::mojom::AutoplayPolicy::kUserGestureRequired
          : blink::mojom::AutoplayPolicy::kNoUserGestureRequired;

  ScopedJavaLocalRef<jstring> url =
      Java_AwSettings_getDefaultVideoPosterURLLocked(env, obj);
  web_prefs->default_video_poster_url =
      url.obj() ? GURL(ConvertJavaStringToUTF8(url)) : GURL();

  bool support_quirks = Java_AwSettings_getSupportLegacyQuirksLocked(env, obj);
  // Please see the corresponding Blink settings for bug references.
  web_prefs->support_deprecated_target_density_dpi = support_quirks;
  web_prefs->viewport_meta_merge_content_quirk = support_quirks;
  web_prefs->viewport_meta_non_user_scalable_quirk = support_quirks;
  web_prefs->viewport_meta_zero_values_quirk = support_quirks;
  web_prefs->clobber_user_agent_initial_scale_quirk = support_quirks;
  web_prefs->ignore_main_frame_overflow_hidden_quirk = support_quirks;
  web_prefs->report_screen_size_in_physical_pixels_quirk = support_quirks;

  web_prefs->reuse_global_for_unowned_main_frame =
      Java_AwSettings_getAllowEmptyDocumentPersistenceLocked(env, obj);

  web_prefs->password_echo_enabled =
      Java_AwSettings_getPasswordEchoEnabledLocked(env, obj);
  web_prefs->spatial_navigation_enabled =
      Java_AwSettings_getSpatialNavigationLocked(env, obj);

  bool enable_supported_hardware_accelerated_features =
      Java_AwSettings_getEnableSupportedHardwareAcceleratedFeaturesLocked(env,
                                                                          obj);
  web_prefs->accelerated_2d_canvas_enabled =
      web_prefs->accelerated_2d_canvas_enabled &&
      enable_supported_hardware_accelerated_features;
  // Always allow webgl. Webview always requires access to the GPU even if
  // it only does software draws. WebGL will not show up in software draw so
  // there is no more brokenness for user. This makes it easier for apps that
  // want to start running webgl content before webview is first attached.

  // If strict mixed content checking is enabled then running should not be
  // allowed.
  DCHECK(!Java_AwSettings_getUseStricMixedContentCheckingLocked(env, obj) ||
         !Java_AwSettings_getAllowRunningInsecureContentLocked(env, obj));
  web_prefs->allow_running_insecure_content =
      Java_AwSettings_getAllowRunningInsecureContentLocked(env, obj);
  web_prefs->strict_mixed_content_checking =
      Java_AwSettings_getUseStricMixedContentCheckingLocked(env, obj);

  web_prefs->fullscreen_supported =
      Java_AwSettings_getFullscreenSupportedLocked(env, obj);
  web_prefs->record_whole_document =
      Java_AwSettings_getRecordFullDocument(env, obj);

  // TODO(jww): This should be removed once sufficient warning has been given of
  // possible API breakage because of disabling insecure use of geolocation.
  web_prefs->allow_geolocation_on_insecure_origins =
      Java_AwSettings_getAllowGeolocationOnInsecureOrigins(env, obj);

  web_prefs->do_not_update_selection_on_mutating_selection_range =
      Java_AwSettings_getDoNotUpdateSelectionOnMutatingSelectionRange(env, obj);

  web_prefs->css_hex_alpha_color_enabled =
      Java_AwSettings_getCSSHexAlphaColorEnabledLocked(env, obj);

  // Keep spellcheck disabled on html elements unless the spellcheck="true"
  // attribute is explicitly specified. This "opt-in" behavior is for backward
  // consistency in apps that use WebView (see crbug.com/652314).
  web_prefs->spellcheck_enabled_by_default = false;

  web_prefs->scroll_top_left_interop_enabled =
      Java_AwSettings_getScrollTopLeftInteropEnabledLocked(env, obj);

  web_prefs->allow_mixed_content_upgrades =
      Java_AwSettings_getAllowMixedContentAutoupgradesLocked(env, obj);

  if (AwDarkMode* aw_dark_mode = AwDarkMode::FromWebContents(web_contents())) {
    aw_dark_mode->PopulateWebPreferences(
        web_prefs, Java_AwSettings_getForceDarkModeLocked(env, obj),
        Java_AwSettings_getForceDarkBehaviorLocked(env, obj),
        Java_AwSettings_isAlgorithmicDarkeningAllowedLocked(env, obj));
  }

  web_prefs->disable_webauthn = true;
  if (Java_AwSettings_getWebauthnSupportLocked(env, obj) != 0) {
    web_prefs->disable_webauthn = false;
  }
}

bool AwSettings::IsForceDarkApplied(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  if (AwDarkMode* aw_dark_mode = AwDarkMode::FromWebContents(web_contents())) {
    return aw_dark_mode->is_force_dark_applied();
  }
  return false;
}

bool AwSettings::PrefersDarkFromTheme(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  if (AwDarkMode* aw_dark_mode = AwDarkMode::FromWebContents(web_contents())) {
    return aw_dark_mode->prefers_dark_from_theme();
  }
  return false;
}

base::android::ScopedJavaLocalRef<jobject> AwSettings::GetJavaObject() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return aw_settings_.get(env);
}

void AwSettings::SetEnterpriseAuthenticationAppLinkPolicyEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  enterprise_authentication_app_link_policy_enabled_ = enabled;
}

bool AwSettings::GetEnterpriseAuthenticationAppLinkPolicyEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return enterprise_authentication_app_link_policy_enabled();
}

bool AwSettings::GetAllowFileAccess() {
  return allow_file_access_;
}

bool AwSettings::GetAllowFileAccessFromFileURLs() {
  return allow_file_access_from_file_urls_;
}

base::android::ScopedJavaLocalRef<jobjectArray>
AwSettings::UpdateXRequestedWithAllowListOriginMatcher(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& jrules) {
  std::vector<std::string> rules;
  base::android::AppendJavaStringArrayToStringVector(env, jrules, &rules);
  std::vector<std::string> bad_rules =
      xrw_allowlist_matcher_->UpdateRuleList(rules);
  return base::android::ToJavaArrayOfStrings(env, bad_rules);
}

scoped_refptr<AwContentsOriginMatcher> AwSettings::xrw_allowlist_matcher() {
  return xrw_allowlist_matcher_;
}

static jlong JNI_AwSettings_Init(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 const JavaParamRef<jobject>& web_contents) {
  content::WebContents* contents =
      content::WebContents::FromJavaWebContents(web_contents);
  AwSettings* settings = new AwSettings(env, obj, contents);
  return reinterpret_cast<intptr_t>(settings);
}

static ScopedJavaLocalRef<jobject> JNI_AwSettings_FromWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  base::android::ScopedJavaLocalRef<jobject> jaw_settings;

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  AwSettings* aw_settings =
      web_contents ? AwSettings::FromWebContents(web_contents) : nullptr;
  if (aw_settings) {
    jaw_settings = aw_settings->GetJavaObject();
  }
  return jaw_settings;
}

static ScopedJavaLocalRef<jstring> JNI_AwSettings_GetDefaultUserAgent(
    JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(env, GetUserAgent());
}

static ScopedJavaLocalRef<jobject> JNI_AwSettings_GetDefaultUserAgentMetadata(
    JNIEnv* env) {
  return ToJavaAwUserAgentMetadata(
      env,
      AwClientHintsControllerDelegate::GetUserAgentMetadataOverrideBrand());
}

}  // namespace android_webview

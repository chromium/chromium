// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/omnibox_prerender.h"

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/omnibox/jni_headers/OmniboxPrerender_jni.h"

using base::android::JavaParamRef;
using predictors::AutocompleteActionPredictor;
using predictors::AutocompleteActionPredictorFactory;

OmniboxPrerender::OmniboxPrerender(JNIEnv* env,
                                   const jni_zero::JavaRef<jobject>& obj)
    : weak_java_omnibox_(env, obj) {}

OmniboxPrerender::~OmniboxPrerender() {
}

static jlong JNI_OmniboxPrerender_Init(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  OmniboxPrerender* omnibox = new OmniboxPrerender(env, obj);
  return reinterpret_cast<intptr_t>(omnibox);
}

void OmniboxPrerender::Clear(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             Profile* profile) {
  DCHECK(profile);
  if (!profile)
    return;
  AutocompleteActionPredictor* action_predictor =
      AutocompleteActionPredictorFactory::GetForProfile(profile);
  action_predictor->UpdateDatabaseFromTransitionalMatches(GURL());
}

void OmniboxPrerender::InitializeForProfile(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj,
                                            Profile* profile) {
  // Initialize the AutocompleteActionPredictor for this profile.
  // It needs to register for notifications as part of its initialization.
  AutocompleteActionPredictorFactory::GetForProfile(profile);
}

void OmniboxPrerender::PrerenderMaybe(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jstring>& j_current_url,
    jlong jsource_match,
    Profile* profile,
    const JavaParamRef<jobject>& j_tab) {
  AutocompleteResult* autocomplete_result =
      reinterpret_cast<AutocompleteResult*>(jsource_match);
  std::u16string url_string =
      base::android::ConvertJavaStringToUTF16(env, j_url);
  std::u16string current_url_string =
      base::android::ConvertJavaStringToUTF16(env, j_current_url);
  content::WebContents* web_contents =
      TabAndroid::GetNativeTab(env, j_tab)->web_contents();
  // TODO(apiccion) Use a delegate for communicating with web_contents.
  // This can happen in OmniboxTests since the results are generated
  // in Java only.
  if (!autocomplete_result)
    return;
  if (!profile)
    return;

  // TODO(crbug.com/40830195): Consider how to co-work with preconnect.
  if (SearchPrefetchService* search_prefetch_service =
          SearchPrefetchServiceFactory::GetForProfile(profile)) {
    search_prefetch_service->OnResultChanged(web_contents,
                                             *autocomplete_result);
  }

  auto* default_match = autocomplete_result->default_match();
  if (!default_match)
    return;

  AutocompleteActionPredictor* action_predictor =
      AutocompleteActionPredictorFactory::GetForProfile(profile);
  if (!action_predictor)
    return;

  action_predictor->
      RegisterTransitionalMatches(url_string, *autocomplete_result);
  AutocompleteActionPredictor::Action recommended_action =
      action_predictor->RecommendAction(url_string, *default_match,
                                        web_contents);

  GURL current_url = GURL(current_url_string);
  // Ask for prerendering if the destination URL is different than the
  // current URL.
  if (default_match->destination_url == current_url)
    return;

  switch (recommended_action) {
    case AutocompleteActionPredictor::ACTION_PRERENDER:
      DoPrerender(*default_match, profile, web_contents);
      break;
    case AutocompleteActionPredictor::ACTION_PRECONNECT:
      DoPreconnect(*default_match, profile);
      break;
    case AutocompleteActionPredictor::ACTION_NONE:
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void OmniboxPrerender::DoPrerender(const AutocompleteMatch& match,
                                   Profile* profile,
                                   content::WebContents* web_contents) {
  DCHECK(profile);
  if (!profile)
    return;
  DCHECK(web_contents);
  if (!web_contents)
    return;

  // AutocompleteActionPredictor does not perform prerendering for search
  // AutocompleteMatches. See `AutocompleteActionPredictor::RecommendAction` for
  // more information.
  // SearchPrefetchService is responsible for handling search
  // AutocompleteMatches and preloading search result pages when needed.
  DCHECK(!AutocompleteMatch::IsSearchType(match.type));
  gfx::Rect container_bounds = web_contents->GetContainerBounds();
  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile)
      ->StartPrerendering(match.destination_url, *web_contents,
                          container_bounds.size());
}

void OmniboxPrerender::DoPreconnect(const AutocompleteMatch& match,
                                    Profile* profile) {
  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile);
  if (loading_predictor) {
    loading_predictor->PrepareForPageLoad(
        /*initiator_origin=*/std::nullopt, match.destination_url,
        predictors::HintOrigin::OMNIBOX,
        predictors::AutocompleteActionPredictor::IsPreconnectable(match));
  }
}

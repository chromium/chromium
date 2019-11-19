// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/omnibox_prerender.h"

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/android/chrome_jni_headers/OmniboxPrerender_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using predictors::AutocompleteActionPredictor;
using predictors::AutocompleteActionPredictorFactory;

OmniboxPrerender::OmniboxPrerender(JNIEnv* env, jobject obj)
    : weak_java_omnibox_(env, obj) {
}

OmniboxPrerender::~OmniboxPrerender() {
}

static jlong JNI_OmniboxPrerender_Init(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  OmniboxPrerender* omnibox = new OmniboxPrerender(env, obj);
  return reinterpret_cast<intptr_t>(omnibox);
}

void OmniboxPrerender::Clear(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             const JavaParamRef<jobject>& j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  DCHECK(profile);
  if (!profile)
    return;
  AutocompleteActionPredictor* action_predictor =
      AutocompleteActionPredictorFactory::GetForProfile(profile);
  action_predictor->ClearTransitionalMatches();
  action_predictor->CancelPrerender();
}

void OmniboxPrerender::InitializeForProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
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
    const JavaParamRef<jobject>& j_profile_android,
    const JavaParamRef<jobject>& j_tab) {
  AutocompleteResult* autocomplete_result =
      reinterpret_cast<AutocompleteResult*>(jsource_match);
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  base::string16 url_string =
      base::android::ConvertJavaStringToUTF16(env, j_url);
  base::string16 current_url_string =
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

  const AutocompleteResult::const_iterator default_match(
      autocomplete_result->default_match());
  if (default_match == autocomplete_result->end())
    return;

  AutocompleteActionPredictor* action_predictor =
      AutocompleteActionPredictorFactory::GetForProfile(profile);
  if (!action_predictor)
    return;

  action_predictor->
      RegisterTransitionalMatches(url_string, *autocomplete_result);
  AutocompleteActionPredictor::Action recommended_action =
      action_predictor->RecommendAction(url_string, *default_match);

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
      NOTREACHED();
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
  gfx::Rect container_bounds = web_contents->GetContainerBounds();
  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile)->
      StartPrerendering(
          match.destination_url,
          web_contents->GetController().GetDefaultSessionStorageNamespace(),
          container_bounds.size());
}

void OmniboxPrerender::DoPreconnect(const AutocompleteMatch& match,
                                    Profile* profile) {
  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile);
  if (loading_predictor) {
    loading_predictor->PrepareForPageLoad(
        match.destination_url, predictors::HintOrigin::OMNIBOX,
        predictors::AutocompleteActionPredictor::IsPreconnectable(match));
  }
}

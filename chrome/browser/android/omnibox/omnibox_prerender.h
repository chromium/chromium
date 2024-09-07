// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_OMNIBOX_PRERENDER_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_OMNIBOX_PRERENDER_H_

#include "base/android/jni_weak_ref.h"

class Profile;
struct AutocompleteMatch;

namespace content {
class WebContents;
}

// This class is responsible for taking the user's omnibox input text,
// AutocompleteResults and navigation actions and then feeding them to the
// AutocompleteActionPredictor. The predictor uses it to update its
// database and returns predictions on what page, if any, to pre-render
// or pre-connect. This class then takes the corresponding action.
class OmniboxPrerender {
 public:
  OmniboxPrerender(JNIEnv* env, const jni_zero::JavaRef<jobject>& obj);

  OmniboxPrerender(const OmniboxPrerender&) = delete;
  OmniboxPrerender& operator=(const OmniboxPrerender&) = delete;

  virtual ~OmniboxPrerender();

  // Clears the transitional matches. This should be called when the user
  // stops typing into the omnibox (e.g. when navigating away, closing the
  // keyboard or changing tabs).
  void Clear(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& obj,
             Profile* profile);

  // Initializes the underlying action predictor for a given profile instance.
  // This should be called as soon as possible as the predictor must register
  // for certain notifications to properly initialize before providing
  // predictions and updated its learning database.
  void InitializeForProfile(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            Profile* profile);

  // Potentailly invokes a pre-render or pre-connect given the url typed into
  // the omnibox and a corresponding autocomplete result. This should be
  // invoked everytime the omnibox changes (e.g. As the user types characters
  // this method should be invoked at least once per character).
  void PrerenderMaybe(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      const base::android::JavaParamRef<jstring>& j_url,
                      const base::android::JavaParamRef<jstring>& j_current_url,
                      jlong jsource_match,
                      Profile* profile,
                      const base::android::JavaParamRef<jobject>& j_tab);

 private:

  // Prerenders a given AutocompleteMatch's url.
  void DoPrerender(const AutocompleteMatch& match,
                   Profile* profile,
                   content::WebContents* web_contents);
  void DoPreconnect(const AutocompleteMatch& match, Profile* profile);
  JavaObjectWeakGlobalRef weak_java_omnibox_;
};

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_OMNIBOX_PRERENDER_H_

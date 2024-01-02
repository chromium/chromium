// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_LOCALE_LOCALE_TEMPLATE_URL_LOADER_H_
#define CHROME_BROWSER_ANDROID_LOCALE_LOCALE_TEMPLATE_URL_LOADER_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/search_engines/template_url.h"

using base::android::JavaParamRef;

class TemplateURLService;

class LocaleTemplateUrlLoader : public ProfileObserver {
 public:
  LocaleTemplateUrlLoader(const std::string& locale,
                          TemplateURLService* service,
                          Profile* profile);
  void Destroy(JNIEnv* env);
  jboolean LoadTemplateUrls(JNIEnv* env);
  void RemoveTemplateUrls(JNIEnv* env);
  void OverrideDefaultSearchProvider(JNIEnv* env);
  void SetGoogleAsDefaultSearch(JNIEnv* env);

  LocaleTemplateUrlLoader(const LocaleTemplateUrlLoader&) = delete;
  LocaleTemplateUrlLoader& operator=(const LocaleTemplateUrlLoader&) = delete;

  ~LocaleTemplateUrlLoader() override;

  // ProfileObserver overrides.
  void OnProfileWillBeDestroyed(Profile* profile) override;

 protected:
  virtual std::vector<std::unique_ptr<TemplateURLData>>
  GetLocalPrepopulatedEngines();
  virtual int GetDesignatedSearchEngineForChina();

 private:
  FRIEND_TEST_ALL_PREFIXES(LocaleTemplateUrlLoaderTest,
                           GetLocalPrepopulatedEngines);
  FRIEND_TEST_ALL_PREFIXES(LocaleTemplateUrlLoaderTest,
                           OnProfileWillBeDestroyed);

  std::string locale_;

  // Tracks all local search engines that were added to TURL service.
  std::vector<int> prepopulate_ids_;

  // Pointer to the TemplateUrlService for the main profile.
  raw_ptr<TemplateURLService> template_url_service_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

#endif  // CHROME_BROWSER_ANDROID_LOCALE_LOCALE_TEMPLATE_URL_LOADER_H_

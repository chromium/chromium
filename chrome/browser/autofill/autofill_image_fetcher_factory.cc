// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_image_fetcher_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>
#endif

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/autofill/android/autofill_image_fetcher_impl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/AutofillImageFetcherFactory_jni.h"
#else
#include "chrome/browser/autofill/ui/autofill_image_fetcher_impl.h"
#endif

namespace autofill {

// static
AutofillImageFetcherBase* AutofillImageFetcherFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AutofillImageFetcherImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AutofillImageFetcherFactory* AutofillImageFetcherFactory::GetInstance() {
  static base::NoDestructor<AutofillImageFetcherFactory> instance;
  return instance.get();
}

AutofillImageFetcherFactory::AutofillImageFetcherFactory()
    : ProfileKeyedServiceFactory(
          "AutofillImageFetcher",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

AutofillImageFetcherFactory::~AutofillImageFetcherFactory() = default;

std::unique_ptr<KeyedService>
AutofillImageFetcherFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AutofillImageFetcherImpl>(
      Profile::FromBrowserContext(context)->GetProfileKey());
}

#if BUILDFLAG(IS_ANDROID)
// Returns the AutofillImageFetcher Java object associated with `profile`.
static base::android::ScopedJavaLocalRef<jobject>
JNI_AutofillImageFetcherFactory_GetForProfile(JNIEnv* env, Profile* profile) {
  CHECK(profile);
  return AutofillImageFetcherFactory::GetForProfile(profile)
      ->GetOrCreateJavaImageFetcher();
}
#endif

}  // namespace autofill

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(AutofillImageFetcherFactory)
#endif

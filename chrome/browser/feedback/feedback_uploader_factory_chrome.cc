// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"

#include "base/memory/singleton.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "content/public/browser/browser_context.h"

namespace feedback {

// static
FeedbackUploaderFactoryChrome* FeedbackUploaderFactoryChrome::GetInstance() {
  return base::Singleton<FeedbackUploaderFactoryChrome>::get();
}

// static
FeedbackUploaderChrome* FeedbackUploaderFactoryChrome::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FeedbackUploaderChrome*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

FeedbackUploaderFactoryChrome::FeedbackUploaderFactoryChrome()
    : FeedbackUploaderFactory("feedback::FeedbackUploaderChrome") {}

FeedbackUploaderFactoryChrome::~FeedbackUploaderFactoryChrome() = default;

content::BrowserContext* FeedbackUploaderFactoryChrome::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      // TODO(crbug.com/40257657): Check if this service is needed in
      // Guest mode.
      .WithGuest(ProfileSelection::kRedirectedToOriginal)
      // TODO(crbug.com/41488885): Check if this service is needed for
      // Ash Internals.
      .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
      .Build()
      .ApplyProfileSelection(Profile::FromBrowserContext(context));
}

bool FeedbackUploaderFactoryChrome::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool FeedbackUploaderFactoryChrome::ServiceIsNULLWhileTesting() const {
  // FeedbackUploaderChrome attempts to access directories that don't exist in
  // unit tests.
  return true;
}

std::unique_ptr<KeyedService>
FeedbackUploaderFactoryChrome::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<FeedbackUploaderChrome>(context);
}

}  // namespace feedback

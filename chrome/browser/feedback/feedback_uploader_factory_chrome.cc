// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"

#include "base/memory/singleton.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/profiles/profile.h"
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
  return Profile::FromBrowserContext(context)->GetOriginalProfile();
}

bool FeedbackUploaderFactoryChrome::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool FeedbackUploaderFactoryChrome::ServiceIsNULLWhileTesting() const {
  // FeedbackUploaderChrome attempts to access directories that don't exist in
  // unit tests.
  return true;
}

KeyedService* FeedbackUploaderFactoryChrome::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FeedbackUploaderChrome(context, task_runner_);
}

}  // namespace feedback

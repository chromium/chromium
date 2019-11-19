// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_FACTORY_CHROME_H_
#define CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_FACTORY_CHROME_H_

#include "base/macros.h"
#include "components/feedback/feedback_uploader_factory.h"

namespace feedback {

class FeedbackUploaderChrome;

// Singleton that owns the FeedbackUploaderChrome.
class FeedbackUploaderFactoryChrome : public FeedbackUploaderFactory {
 public:
  // Returns singleton instance of FeedbackUploaderFactoryChrome.
  static FeedbackUploaderFactoryChrome* GetInstance();

  // Returns the FeedbackUploaderChrome associated with |context|.
  static FeedbackUploaderChrome* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<FeedbackUploaderFactoryChrome>;

  FeedbackUploaderFactoryChrome();
  ~FeedbackUploaderFactoryChrome() override;

  // BrowserContextKeyedServiceFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploaderFactoryChrome);
};

}  // namespace feedback

#endif  // CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_FACTORY_CHROME_H_

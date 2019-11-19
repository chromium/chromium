// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_
#define CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_

#include <memory>

#include "build/build_config.h"
#include "content/public/browser/tracing_delegate.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#else
#include "chrome/browser/ui/browser_list_observer.h"
#endif

class PrefRegistrySimple;

namespace network {
class SharedURLLoaderFactory;
}

class ChromeTracingDelegate : public content::TracingDelegate,
#if defined(OS_ANDROID)
                              public TabModelListObserver
#else
                              public BrowserListObserver
#endif
{
 public:
  ChromeTracingDelegate();
  ~ChromeTracingDelegate() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  std::unique_ptr<content::TraceUploader> GetTraceUploader(
      scoped_refptr<network::SharedURLLoaderFactory> factory) override;

  bool IsAllowedToBeginBackgroundScenario(
      const content::BackgroundTracingConfig& config,
      bool requires_anonymized_data) override;

  bool IsAllowedToEndBackgroundScenario(
      const content::BackgroundTracingConfig& config,
      bool requires_anonymized_data) override;

  bool IsProfileLoaded() override;

  std::unique_ptr<base::DictionaryValue> GenerateMetadataDict() override;

 private:
#if defined(OS_ANDROID)
  // TabModelListObserver implementation.
  void OnTabModelAdded() override;
  void OnTabModelRemoved() override;
#else
  // BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;
#endif

  bool incognito_launched_;
};

#endif  // CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_

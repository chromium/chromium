// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_READ_ALOUD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_READALOUD_READ_ALOUD_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace readaloud {

class ReadAloudService;

// Factory responsible for instantiating ReadAloudService instances per
// BrowserContext (Profile). Gated behind kReadAloudNative on Android and
// behind kReadAnythingReadAloudExperimentalPlaybackUi on desktop, where the
// service currently only handles AI Playback feedback (b/584339399).
class ReadAloudServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ReadAloudService* GetForProfile(Profile* profile);
  // Like GetForProfile() but never creates the service; for teardown paths.
  static ReadAloudService* GetForProfileIfExists(Profile* profile);
  static ReadAloudServiceFactory* GetInstance();

  ReadAloudServiceFactory(const ReadAloudServiceFactory&) = delete;
  ReadAloudServiceFactory& operator=(const ReadAloudServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ReadAloudServiceFactory>;

  ReadAloudServiceFactory();
  ~ReadAloudServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_READ_ALOUD_SERVICE_FACTORY_H_

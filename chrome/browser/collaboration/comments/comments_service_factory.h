// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COLLABORATION_COMMENTS_COMMENTS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_COLLABORATION_COMMENTS_COMMENTS_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace collaboration::comments {
class CommentsService;

// A factory to create a unique CommentsService.
class CommentsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the CommentsService for the profile. An empty service is returned for
  // incognito/guest, or if the data sharing or comments features are disabled.
  static CommentsService* GetForProfile(Profile* profile);

  // Gets the lazy singleton instance of CommentsServiceFactory.
  static CommentsServiceFactory* GetInstance();

  // Disallow copy/assign.
  CommentsServiceFactory(const CommentsServiceFactory&) = delete;
  CommentsServiceFactory& operator=(const CommentsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<CommentsServiceFactory>;

  CommentsServiceFactory();
  ~CommentsServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace collaboration::comments

#endif  // CHROME_BROWSER_COLLABORATION_COMMENTS_COMMENTS_SERVICE_FACTORY_H_

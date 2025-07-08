// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/collaboration/comments/comments_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/collaboration/internal/comments/comments_service_impl.h"
#include "components/collaboration/internal/comments/empty_comments_service.h"
#include "components/collaboration/public/comments/comments_service.h"
#include "components/collaboration/public/features.h"
#include "components/data_sharing/public/features.h"
#include "content/public/browser/browser_context.h"

namespace collaboration::comments {

// static
CommentsServiceFactory* CommentsServiceFactory::GetInstance() {
  static base::NoDestructor<CommentsServiceFactory> instance;
  return instance.get();
}

// static
CommentsService* CommentsServiceFactory::GetForProfile(Profile* profile) {
  CHECK(profile);
  return static_cast<CommentsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

CommentsServiceFactory::CommentsServiceFactory()
    : ProfileKeyedServiceFactory(
          "CommentsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

CommentsServiceFactory::~CommentsServiceFactory() = default;

std::unique_ptr<KeyedService>
CommentsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // This service requires the data sharing and commenting features to be
  // enabled.
  if (!data_sharing::features::IsDataSharingFunctionalityEnabled() ||
      !base::FeatureList::IsEnabled(features::kCollaborationComments)) {
    return std::make_unique<EmptyCommentsService>();
  }
  return std::make_unique<CommentsServiceImpl>();
}

}  // namespace collaboration::comments

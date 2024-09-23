// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/power_bookmarks/power_bookmark_service_factory.h"

#include "base/files/file_path.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

// static
power_bookmarks::PowerBookmarkService*
PowerBookmarkServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<power_bookmarks::PowerBookmarkService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PowerBookmarkServiceFactory* PowerBookmarkServiceFactory::GetInstance() {
  static base::NoDestructor<PowerBookmarkServiceFactory> instance;
  return instance.get();
}

PowerBookmarkServiceFactory::PowerBookmarkServiceFactory()
    : ProfileKeyedServiceFactory(
          "PowerBookmarkService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(BookmarkModelFactory::GetInstance());
}

PowerBookmarkServiceFactory::~PowerBookmarkServiceFactory() = default;

std::unique_ptr<KeyedService>
PowerBookmarkServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<power_bookmarks::PowerBookmarkService>(
      BookmarkModelFactory::GetInstance()->GetForBrowserContext(context),
      context->GetPath().AppendASCII("power_bookmarks"),
      content::GetUIThreadTaskRunner({}),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
}

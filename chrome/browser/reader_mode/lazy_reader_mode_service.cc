// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reader_mode/lazy_reader_mode_service.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reader_mode/reader_mode_service_factory.h"
#include "components/web_distiller/core/distilled_page_prefs.h"
#include "components/web_distiller/core/distiller_page.h"
#include "components/web_distiller/core/web_distiller_service.h"

namespace reader_mode {

namespace {

// Identifies the user data on the profile.
const char kLazyReaderModeServiceKey[] = "kLazyReaderModeServiceKey";

}  // namespace

// static
LazyReaderModeService* LazyReaderModeService::Create(Profile* profile) {
  // We can't use make_unique because the constructor is private.
  LazyReaderModeService* instance = new LazyReaderModeService(profile);
  profile->SetUserData(&kLazyReaderModeServiceKey, base::WrapUnique(instance));
  return instance;
}

LazyReaderModeService::~LazyReaderModeService() = default;

std::unique_ptr<web_distiller::ViewerHandle> LazyReaderModeService::ViewUrl(
    web_distiller::ViewRequestDelegate* delegate,
    std::unique_ptr<web_distiller::DistillerPage> distiller_page,
    const GURL& url) {
  return GetImpl()->ViewUrl(delegate, std::move(distiller_page), url);
}

std::unique_ptr<web_distiller::ViewerHandle>
LazyReaderModeService::ViewUrlIgnoreCache(
    web_distiller::ViewRequestDelegate* delegate,
    std::unique_ptr<web_distiller::DistillerPage> distiller_page,
    const GURL& url) {
  return GetImpl()->ViewUrlIgnoreCache(delegate, std::move(distiller_page),
                                       url);
}

std::unique_ptr<web_distiller::DistillerPage>
LazyReaderModeService::CreateDefaultDistillerPage(
    const gfx::Size& render_view_size) {
  return GetImpl()->CreateDefaultDistillerPage(render_view_size);
}

std::unique_ptr<web_distiller::DistillerPage>
LazyReaderModeService::CreateDefaultDistillerPageWithHandle(
    std::unique_ptr<web_distiller::SourcePageHandle> handle) {
  return GetImpl()->CreateDefaultDistillerPageWithHandle(std::move(handle));
}

web_distiller::DistilledPagePrefs*
LazyReaderModeService::GetDistilledPagePrefs() {
  return GetImpl()->GetDistilledPagePrefs();
}

web_distiller::DistillerUIHandle*
LazyReaderModeService::GetDistillerUIHandle() {
  return GetImpl()->GetDistillerUIHandle();
}

LazyReaderModeService::LazyReaderModeService(Profile* profile)
    : profile_(profile) {}

// This will create an object and schedule work the first time it's called
// and just return an existing object after that.
web_distiller::WebDistillerServiceInterface* LazyReaderModeService::GetImpl()
    const {
  return ReaderModeServiceFactory::GetInstance()->GetForBrowserContext(
      profile_);
}

}  // namespace reader_mode

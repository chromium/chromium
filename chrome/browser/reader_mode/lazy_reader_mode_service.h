// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READER_MODE_LAZY_READER_MODE_SERVICE_H_
#define CHROME_BROWSER_READER_MODE_LAZY_READER_MODE_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/task_tracker.h"

class Profile;

namespace reader_mode {

// A class which helps with lazy instantiation of the distiller service, using
// the BrowserContextKeyedServiceFactory for it. This class is owned by Profile.
class LazyReaderModeService
    : public dom_distiller::DomDistillerServiceInterface,
      public base::SupportsUserData::Data {
 public:
  // Creates and returns an instance for |profile|. This does not pass ownership
  // of the returned pointer.
  static LazyReaderModeService* Create(Profile* profile);

  LazyReaderModeService(const LazyReaderModeService&) = delete;
  LazyReaderModeService& operator=(const LazyReaderModeService&) = delete;

  ~LazyReaderModeService() override;

  // dom_distiller::DomDistillerServiceInterface implementation:
  std::unique_ptr<dom_distiller::ViewerHandle> ViewUrl(
      dom_distiller::ViewRequestDelegate* delegate,
      std::unique_ptr<dom_distiller::DistillerPage> distiller_page,
      const GURL& url) override;
  std::unique_ptr<dom_distiller::ViewerHandle> ViewUrlIgnoreCache(
      dom_distiller::ViewRequestDelegate* delegate,
      std::unique_ptr<dom_distiller::DistillerPage> distiller_page,
      const GURL& url) override;
  std::unique_ptr<dom_distiller::DistillerPage> CreateDefaultDistillerPage(
      const gfx::Size& render_view_size) override;
  std::unique_ptr<dom_distiller::DistillerPage>
  CreateDefaultDistillerPageWithHandle(
      std::unique_ptr<dom_distiller::SourcePageHandle> handle) override;
  dom_distiller::DistilledPagePrefs* GetDistilledPagePrefs() override;
  dom_distiller::DistillerUIHandle* GetDistillerUIHandle() override;

 private:
  explicit LazyReaderModeService(Profile* profile);

  // Accessor method for the backing service instance.
  dom_distiller::DomDistillerServiceInterface* GetImpl() const;

  // The Profile to use when retrieving the service and also the profile to
  // listen for destruction of.
  raw_ptr<Profile> profile_;
};

}  // namespace reader_mode

#endif  // CHROME_BROWSER_READER_MODE_LAZY_READER_MODE_SERVICE_H_

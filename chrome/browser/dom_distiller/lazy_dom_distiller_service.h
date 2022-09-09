// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_DISTILLER_LAZY_DOM_DISTILLER_SERVICE_H_
#define CHROME_BROWSER_DOM_DISTILLER_LAZY_DOM_DISTILLER_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/task_tracker.h"

class Profile;

namespace dom_distiller {

// A class which helps with lazy instantiation of the DomDistillerService, using
// the BrowserContextKeyedServiceFactory for it. This class is owned by Profile.
class LazyDomDistillerService : public DomDistillerServiceInterface,
                                public base::SupportsUserData::Data {
 public:
  // Creates and returns an instance for |profile|. This does not pass ownership
  // of the returned pointer.
  static LazyDomDistillerService* Create(Profile* profile);

  LazyDomDistillerService(const LazyDomDistillerService&) = delete;
  LazyDomDistillerService& operator=(const LazyDomDistillerService&) = delete;

  ~LazyDomDistillerService() override;

  // DomDistillerServiceInterface implementation:
  std::unique_ptr<ViewerHandle> ViewUrl(
      ViewRequestDelegate* delegate,
      std::unique_ptr<DistillerPage> distiller_page,
      const GURL& url) override;
  std::unique_ptr<DistillerPage> CreateDefaultDistillerPage(
      const gfx::Size& render_view_size) override;
  std::unique_ptr<DistillerPage> CreateDefaultDistillerPageWithHandle(
      std::unique_ptr<SourcePageHandle> handle) override;
  DistilledPagePrefs* GetDistilledPagePrefs() override;
  DistillerUIHandle* GetDistillerUIHandle() override;

 private:
  explicit LazyDomDistillerService(Profile* profile);

  // Accessor method for the backing service instance.
  DomDistillerServiceInterface* GetImpl() const;

  // The Profile to use when retrieving the DomDistillerService and also the
  // profile to listen for destruction of.
  raw_ptr<Profile> profile_;
};

}  // namespace dom_distiller

#endif  // CHROME_BROWSER_DOM_DISTILLER_LAZY_DOM_DISTILLER_SERVICE_H_

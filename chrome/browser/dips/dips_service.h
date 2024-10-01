// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_SERVICE_H_
#define CHROME_BROWSER_DIPS_DIPS_SERVICE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_redirect_info.h"

class GURL;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

// When DIPS moves to //content, DIPSService will be exposed in the Content API,
// available to embedders such as Chrome.
class DIPSService : public base::SupportsUserData {
 public:
  using DeletedSitesCallback =
      base::OnceCallback<void(const std::vector<std::string>& sites)>;
  using CheckInteractionCallback = base::OnceCallback<void(bool)>;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnStatefulBounce(content::WebContents* web_contents) {}
    virtual void OnChainHandled(const DIPSRedirectChainInfoPtr& chain) {}
  };

  static DIPSService* Get(content::BrowserContext* context);

  virtual void RecordBrowserSignIn(std::string_view domain) = 0;

  virtual void DeleteEligibleSitesImmediately(
      DeletedSitesCallback callback) = 0;

  virtual void RecordInteractionForTesting(const GURL& url) = 0;

  virtual void DidSiteHaveInteractionSince(
      const GURL& url,
      base::Time bound,
      CheckInteractionCallback callback) const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(const Observer* observer) = 0;
};

#endif  // CHROME_BROWSER_DIPS_DIPS_SERVICE_H_

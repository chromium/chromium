// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHER_HOST_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHER_HOST_H_

namespace apps {

// Hosts many publishers and maintain the instances.
class PublisherHost {
 public:
  virtual ~PublisherHost() = default;

#if BUILDFLAG(IS_CHROMEOS)
  // Called when ArcApps is registered to AppServiceProxy.
  virtual void SetArcIsRegistered() = 0;

  // Shuts down the publishers.
  virtual void Shutdown() = 0;

  // Testing methods.
  virtual void ReInitializeCrostiniForTesting() = 0;
  virtual void RegisterPublishersForTesting() = 0;
#endif
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHER_HOST_H_

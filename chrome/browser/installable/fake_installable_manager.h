// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_FAKE_INSTALLABLE_MANAGER_H_
#define CHROME_BROWSER_INSTALLABLE_FAKE_INSTALLABLE_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/installable/installable_logging.h"
#include "chrome/browser/installable/installable_manager.h"
#include "url/gurl.h"

struct InstallableData;

namespace blink {
struct Manifest;
}

namespace content {
class WebContents;
}

class FakeInstallableManager : public InstallableManager {
 public:
  explicit FakeInstallableManager(content::WebContents* web_contents);
  ~FakeInstallableManager() override;

  // InstallableManager:
  void GetData(const InstallableParams& params,
               InstallableCallback callback) override;

  void RunCallback(InstallableCallback callback);

  // Create the manager and attach it to |web_contents|.
  static FakeInstallableManager* CreateForWebContents(
      content::WebContents* web_contents);

  // Create the manager and attach it to |web_contents|. It responds to
  // |GetData| with a blink |manifest| provided.
  static FakeInstallableManager* CreateForWebContentsWithManifest(
      content::WebContents* web_contents,
      InstallableStatusCode installable_code,
      const GURL& manifest_url,
      std::unique_ptr<blink::Manifest> manifest);

 private:
  GURL manifest_url_;
  std::unique_ptr<blink::Manifest> manifest_;
  std::unique_ptr<InstallableData> data_;

  base::WeakPtrFactory<FakeInstallableManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_INSTALLABLE_FAKE_INSTALLABLE_MANAGER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_MULTI_SCREEN_CAPTURE_MULTI_SCREEN_CAPTURE_POLICY_SERVICE_H_
#define CHROME_BROWSER_ASH_POLICY_MULTI_SCREEN_CAPTURE_MULTI_SCREEN_CAPTURE_POLICY_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
class Profile;

namespace policy {

// This keyed service serves as mechanism to prevent dynamic propagation for the
// "profile.managed_multi_screen_capture_allowed_for_urls" pref. On startup, it
// loads the initial value of the pref and backs it up. This backup is then used
// to check access to the getAllScreensMedia API instead of the latest version
// of the pref.
class MultiScreenCapturePolicyService : public KeyedService {
 public:
  ~MultiScreenCapturePolicyService() override;

  static std::unique_ptr<MultiScreenCapturePolicyService> Create(
      Profile* profile);

  bool IsMultiScreenCaptureAllowed(const GURL& url) const;
  size_t GetAllowListSize() const;

  // KeyedService:
  void Shutdown() override;

 private:
  explicit MultiScreenCapturePolicyService(Profile* profile);

  // Sets up watchers.
  void Init();

  // Keyed services are shut down from the embedder's destruction of the profile
  // and this pointer is reset in `ShutDown`. Therefore it is safe to use this
  // raw pointer.
  raw_ptr<Profile> profile_ = nullptr;

  base::Value::List multi_screen_capture_allow_list_on_login_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_MULTI_SCREEN_CAPTURE_MULTI_SCREEN_CAPTURE_POLICY_SERVICE_H_

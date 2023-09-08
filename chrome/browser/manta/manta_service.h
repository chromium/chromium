// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANTA_MANTA_SERVICE_H_
#define CHROME_BROWSER_MANTA_MANTA_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace manta {

class OrcaProvider;
class SnapperProvider;

// The MantaService class is a Profile keyed service for the chrome Manta
// project. It serves two main functions:
// 1. It hands clients instances to specific providers for calling and
// interacting with google services relevant to the Manta project.
// 2. It provides utility methods for clients to query profile specific
// information relevant to the Manta project.
class MantaService : public KeyedService {
 public:
  explicit MantaService(Profile* profile);

  MantaService(const MantaService&) = delete;
  MantaService& operator=(const MantaService&) = delete;

  // Returns a unique pointer to an instance of the Providers for the
  // profile associated with the MantaService instance from which this method
  // is called.
  // NOTE: The returned Provider instance is tied to the
  // IdentityManager and should not be called past its lifetime. See
  // `Provider` header for details.
  std::unique_ptr<OrcaProvider> CreateOrcaProvider();
  virtual std::unique_ptr<SnapperProvider> CreateSnapperProvider();

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace manta

#endif  // CHROME_BROWSER_MANTA_MANTA_SERVICE_H_

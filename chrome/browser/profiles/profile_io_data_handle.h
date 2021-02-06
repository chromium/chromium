// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_HANDLE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_HANDLE_H_

#include "base/macros.h"

namespace content {
class ResourceContext;
}  // namespace content

class Profile;
class ProfileIOData;

class ProfileIODataHandle {
 public:
  explicit ProfileIODataHandle(Profile* profile);
  ~ProfileIODataHandle();

  content::ResourceContext* GetResourceContext() const;

 private:
  // Lazily initialize ProfileParams.
  void LazyInitialize() const;

  // The getters will be invalidated on the IO thread before
  // ProfileIOData instance is deleted.
  ProfileIOData* const io_data_;

  Profile* const profile_;

  mutable bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIODataHandle);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_HANDLE_H_

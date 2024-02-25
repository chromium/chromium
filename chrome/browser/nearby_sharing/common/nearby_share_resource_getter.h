// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_RESOURCE_GETTER_H_
#define CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_RESOURCE_GETTER_H_

#include <string>

#include "base/no_destructor.h"

// Singleton class providing utility methods for Nearby Share resources.
class NearbyShareResourceGetter {
 public:
  static NearbyShareResourceGetter* GetInstance();

  NearbyShareResourceGetter(const NearbyShareResourceGetter&) = delete;
  NearbyShareResourceGetter& operator=(const NearbyShareResourceGetter&) =
      delete;

  std::u16string GetFeatureName();

  // Assumes that caller is passing a |message_id| with a placeholder for
  // the feature name at index 0 in the placeholder list.
  std::u16string GetStringWithFeatureName(int message_id);

 private:
  NearbyShareResourceGetter();
  ~NearbyShareResourceGetter() = default;

  // |base::NoDestructor| must be a friend to access private constructor.
  friend class base::NoDestructor<NearbyShareResourceGetter>;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_RESOURCE_GETTER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_DELEGATE_IMPL_H_

#include "components/enterprise/browser/identifiers/profile_id_delegate.h"

#include <string>

#include "base/memory/raw_ptr.h"

class Profile;

namespace enterprise {

// Implementation of the profile Id delegate.
class ProfileIdDelegateImpl : public ProfileIdDelegate {
 public:
  explicit ProfileIdDelegateImpl(Profile* profile);
  ~ProfileIdDelegateImpl() override;

  // ProfileIdDelegate
  std::string GetDeviceId() override;

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace enterprise

#endif  // CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_DELEGATE_IMPL_H_

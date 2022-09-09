// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_USER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_USER_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/device_signals/core/browser/user_delegate.h"

class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace enterprise_signals {

class UserDelegateImpl : public device_signals::UserDelegate {
 public:
  UserDelegateImpl(Profile* profile, signin::IdentityManager* identity_manager);
  ~UserDelegateImpl() override;

  UserDelegateImpl(const UserDelegateImpl&) = delete;
  UserDelegateImpl& operator=(const UserDelegateImpl&) = delete;

  // UserDelegate:
  bool IsAffiliated() const override;
  bool IsManaged() const override;
  bool IsSameUser(const std::string& gaia_id) const override;

 private:
  const base::raw_ptr<Profile> profile_;
  const base::raw_ptr<signin::IdentityManager> identity_manager_;
};

}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_USER_DELEGATE_IMPL_H_

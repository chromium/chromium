// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_SHARED_CRD_SESSION_WRAPPER_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_SHARED_CRD_SESSION_WRAPPER_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/boca/shared_crd_session_wrapper.h"

namespace policy {
class SharedCrdSessionProvider;
class SharedCrdSession;
}  // namespace policy

namespace ash::boca {

// This class is part of a framework for remotely initiated CRD sessions on
// managed ChromeOS devices. It is intended to be called only by Class Tools
// kiosk receiver code.
class SharedCrdSessionWrapperImpl : public SharedCrdSessionWrapper {
 public:
  explicit SharedCrdSessionWrapperImpl(
      std::unique_ptr<policy::SharedCrdSessionProvider> crd_session_provider);

  SharedCrdSessionWrapperImpl(const SharedCrdSessionWrapperImpl&) = delete;
  SharedCrdSessionWrapperImpl& operator=(const SharedCrdSessionWrapperImpl&) =
      delete;

  ~SharedCrdSessionWrapperImpl() override;

  void StartCrdHost(
      const std::string& receiver_email,
      base::OnceCallback<void(const std::string&)> success_callback,
      base::OnceClosure error_callback,
      base::OnceClosure session_finished_callback) override;

  void TerminateSession() override;

 private:
  const std::unique_ptr<policy::SharedCrdSessionProvider> crd_session_provider_;
  const std::unique_ptr<policy::SharedCrdSession> crd_session_;
};

}  // namespace ash::boca

#endif  // CHROME_BROWSER_ASH_BOCA_SHARED_CRD_SESSION_WRAPPER_IMPL_H_

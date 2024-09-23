// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXO_CHROME_SECURITY_DELEGATE_H_
#define CHROME_BROWSER_ASH_EXO_CHROME_SECURITY_DELEGATE_H_

#include "components/exo/security_delegate.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash {

// Translate paths from |source| VM to valid paths in the host. Invalid paths
// are ignored.
std::vector<base::FilePath> TranslateVMPathsToHost(
    const std::string& vm_name,
    const std::vector<ui::FileInfo>& vm_paths);

// Share |files| with |target| VM, and translate |files| to be "file://" URLs
// which can be used inside the vm. |callback| is invoked with translated
// "file://" URLs.
void ShareWithVMAndTranslateToFileUrls(
    const std::string& vm_name,
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(std::vector<std::string>)> callback);

class ChromeSecurityDelegate : public exo::SecurityDelegate {
 public:
  ChromeSecurityDelegate();
  ChromeSecurityDelegate(const ChromeSecurityDelegate&) = delete;
  ChromeSecurityDelegate& operator=(const ChromeSecurityDelegate&) = delete;
  ~ChromeSecurityDelegate() override;

  // exo::SecurityDelegate;
  bool CanSelfActivate(aura::Window* window) const override;
  bool CanLockPointer(aura::Window* window) const override;
  SetBoundsPolicy CanSetBounds(aura::Window* window) const override;
  std::vector<ui::FileInfo> GetFilenames(
      ui::EndpointType source,
      const std::vector<uint8_t>& data) const override;
  void SendFileInfo(ui::EndpointType target,
                    const std::vector<ui::FileInfo>& files,
                    SendDataCallback callback) const override;
  void SendPickle(ui::EndpointType target,
                  const base::Pickle& pickle,
                  SendDataCallback callback) override;

  virtual std::string GetVmName(ui::EndpointType target) const;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXO_CHROME_SECURITY_DELEGATE_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CONTENT_PROTECTION_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CONTENT_PROTECTION_ASH_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/attestation/platform_verification_flow.h"
#include "chromeos/crosapi/mojom/content_protection.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/aura/window_observer.h"

namespace ash {
class OutputProtectionDelegate;
}  //  namespace ash

namespace crosapi {

// Implements the crosapi interface for content protection. Lives in Ash-Chrome
// on the UI thread.
//
// This class bridge from Lacros to the existing Content Protection
// implementation. Currently it does a just-in-time window lookup from the
// window_id. A more nuanced implementation could wait a few seconds for the
// Window to appear (assuming it was just created) before erroring out.
class ContentProtectionAsh : public aura::WindowObserver,
                             public mojom::ContentProtection {
 public:
  ContentProtectionAsh();
  ContentProtectionAsh(const ContentProtectionAsh&) = delete;
  ContentProtectionAsh& operator=(const ContentProtectionAsh&) = delete;
  ~ContentProtectionAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::ContentProtection> receiver);

  // crosapi::mojom::ContentProtection:
  void EnableWindowProtection(const std::string& window_id,
                              uint32_t desired_protection_mask,
                              EnableWindowProtectionCallback callback) override;
  void QueryWindowStatus(const std::string& window_id,
                         QueryWindowStatusCallback callback) override;
  void GetSystemSalt(GetSystemSaltCallback callback) override;
  void ChallengePlatform(const std::string& service_id,
                         const std::string& challenge,
                         ChallengePlatformCallback callback) override;
  void IsVerifiedAccessEnabled(
      IsVerifiedAccessEnabledCallback callback) override;

 private:
  // If an OutputProtectionDelegate already exists, returns it. Otherwise
  // creates one and returns it.
  ash::OutputProtectionDelegate* FindOrCreateOutputProtectionDelegate(
      aura::Window* window);

  // Cleans up output_protection_delegates_.
  void OnWindowDestroyed(aura::Window* window) override;

  // Invokes |callback| converting the input parameters to the mojom-friendly
  // format.
  void ExecuteWindowStatusCallback(QueryWindowStatusCallback callback,
                                   bool success,
                                   uint32_t link_mask,
                                   uint32_t protection_mask);

  void OnChallengePlatform(
      ChallengePlatformCallback callback,
      ash::attestation::PlatformVerificationFlow::Result result,
      const std::string& signed_data,
      const std::string& signature,
      const std::string& platform_key_certificate);

  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::ContentProtection> receivers_;

  // This map holds an OutputProtectionDelegate for each associated
  // aura::Window. This class uses an aura::Window listener to ensure that
  // entries are removed when the corresponding aura::Window is destroyed.
  // Elements are lazily added as Crosapi clients call
  // EnableWindowProtection and QueryWindowStatus.
  std::map<aura::Window*, std::unique_ptr<ash::OutputProtectionDelegate>>
      output_protection_delegates_;

  // Platform verification is a stateful operation. This instance holds all
  // state associated with (possibly multiple) ongoing operations.
  scoped_refptr<ash::attestation::PlatformVerificationFlow>
      platform_verification_flow_;

  base::WeakPtrFactory<ContentProtectionAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CONTENT_PROTECTION_ASH_H_

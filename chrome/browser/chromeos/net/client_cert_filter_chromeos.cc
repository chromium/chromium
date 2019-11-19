// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/client_cert_filter_chromeos.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/nss_profile_filter_chromeos.h"

namespace chromeos {

class ClientCertFilterChromeOS::CertFilterIO {
 public:
  CertFilterIO(bool use_system_slot, const std::string& username_hash)
      : use_system_slot_(use_system_slot), username_hash_(username_hash) {}

  // Must be called on the IO thread. Calls |callback| on the UI thread.
  void Init(base::OnceClosure callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    DCHECK(!init_called_);
    init_called_ = true;

    waiting_for_private_slot_ = true;

    if (use_system_slot_) {
      system_slot_ = crypto::GetSystemNSSKeySlot(base::BindOnce(
          &CertFilterIO::GotSystemSlot, weak_ptr_factory_.GetWeakPtr()));
    }

    private_slot_ = crypto::GetPrivateSlotForChromeOSUser(
        username_hash_, base::BindOnce(&CertFilterIO::GotPrivateSlot,
                                       weak_ptr_factory_.GetWeakPtr()));

    // If the returned slot is null, GotPrivateSlot will be called back
    // eventually. If it is not null, the private slot was available
    // synchronously and the callback will not be called.
    if (private_slot_)
      waiting_for_private_slot_ = false;

    init_callback_ = std::move(callback);
    InitIfSlotsAvailable();
  }

  // May be called on any thread, after Init()'s callback has run.
  bool IsCertAllowed(CERTCertificate* cert) {
    return nss_profile_filter_.IsCertAllowed(cert);
  }

 private:
  // Called back if the system slot was retrieved asynchronously. Continues the
  // initialization.
  void GotSystemSlot(crypto::ScopedPK11Slot system_slot) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    system_slot_ = std::move(system_slot);
    InitIfSlotsAvailable();
  }

  // Called back if the private slot was retrieved asynchronously. Continues the
  // initialization.
  void GotPrivateSlot(crypto::ScopedPK11Slot private_slot) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    waiting_for_private_slot_ = false;
    private_slot_ = std::move(private_slot);
    InitIfSlotsAvailable();
  }

  // If the required slots (|private_slot_| and conditionally |system_slot_|)
  // are available, initializes |nss_profile_filter_| and posts |init_callback_|
  // to the UI thread.
  void InitIfSlotsAvailable() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if ((use_system_slot_ && !system_slot_) || waiting_for_private_slot_)
      return;
    nss_profile_filter_.Init(
        crypto::GetPublicSlotForChromeOSUser(username_hash_),
        std::move(private_slot_), std::move(system_slot_));
    if (!init_callback_.is_null()) {
      base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                     std::move(init_callback_));
    }
  }

  // True once Init() was called.
  bool init_called_ = false;

  // The callback provided to Init(). Called on the UI thread.
  base::OnceClosure init_callback_;

  bool use_system_slot_;
  std::string username_hash_;

  // Used to store the system slot, if required, for initialization. Will be
  // null after the filter is initialized.
  crypto::ScopedPK11Slot system_slot_;

  // Used to store the private slot for initialization. Will be null after the
  // filter is initialized.
  crypto::ScopedPK11Slot private_slot_;

  // If a private slot is requested but the slot, maybe null, is not obtained
  // yet, this is equal true. As long as this is true, the NSSProfileFilter will
  // not be initialized.
  bool waiting_for_private_slot_ = false;

  net::NSSProfileFilterChromeOS nss_profile_filter_;
  base::WeakPtrFactory<CertFilterIO> weak_ptr_factory_{this};
};

ClientCertFilterChromeOS::ClientCertFilterChromeOS(
    bool use_system_slot,
    const std::string& username_hash)
    : cert_filter_io_(new CertFilterIO(use_system_slot, username_hash)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ClientCertFilterChromeOS::~ClientCertFilterChromeOS() {}

bool ClientCertFilterChromeOS::Init(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // base::Unretained() is safe here because |cert_filter_io_| is destroyed on
  // a post to the IO thread.
  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &CertFilterIO::Init, base::Unretained(cert_filter_io_.get()),
          // Wrap |callback| in OnInitComplete so it is cancelled if the
          // ClientCertFilterChromeOS is destroyed earlier.
          base::BindOnce(&ClientCertFilterChromeOS::OnInitComplete,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
  return false;
}

bool ClientCertFilterChromeOS::IsCertAllowed(CERTCertificate* cert) const {
  return cert_filter_io_->IsCertAllowed(cert);
}

void ClientCertFilterChromeOS::OnInitComplete(base::OnceClosure callback) {
  std::move(callback).Run();
}

}  // namespace chromeos

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_CLIENT_CERT_FILTER_H_
#define CHROME_BROWSER_ASH_NET_CLIENT_CERT_FILTER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"

typedef struct CERTCertificateStr CERTCertificate;

namespace ash {

// A client certificate filter that filters by applying a
// NSSProfileFilterChromeOS.
//
// TODO(davidben): Fold this class back into ClientCertStoreAsh.
class ClientCertFilter : public base::RefCountedThreadSafe<ClientCertFilter> {
 public:
  // The internal NSSProfileFilterChromeOS will be initialized with the public
  // and private slot of the user with |username_hash| and with the system slot
  // if |use_system_slot| is true.
  // If |username_hash| is empty, no public and no private slot will be used.
  ClientCertFilter(bool use_system_slot, const std::string& username_hash);

  // Initializes this filter. Returns true if it finished initialization,
  // otherwise returns false and calls |callback| once the initialization is
  // completed.
  // Must be called at most once.
  bool Init(base::OnceClosure callback);

  // Returns true if |cert| is allowed to be used as a client certificate (e.g.
  // for a certain browser context or user). This is only called once
  // initialization is finished, see Init().
  bool IsCertAllowed(CERTCertificate* cert) const;

 private:
  class CertFilterIO;
  friend class base::RefCountedThreadSafe<ClientCertFilter>;

  ~ClientCertFilter();

  void OnInitComplete(base::OnceClosure callback);

  // This class lives on the UI thread but the NSS ChromeOS user integration
  // must be called from the IO thread.
  std::unique_ptr<CertFilterIO, content::BrowserThread::DeleteOnIOThread>
      cert_filter_io_;
  base::WeakPtrFactory<ClientCertFilter> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_CLIENT_CERT_FILTER_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_KERBEROS_KERBEROS_FILES_HANDLER_H_
#define CHROME_BROWSER_ASH_KERBEROS_KERBEROS_FILES_HANDLER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_member.h"

namespace ash {

// Kerberos defaults for canonicalization SPN. (see
// https://web.mit.edu/kerberos/krb5-1.12/doc/admin/conf_files/krb5_conf.html)
// Exported for browsertests.

// Directory in the user's cryptohome where Kerberos files are stored.
extern const char kKrb5Directory[];
// Kerberos Credential cache file name.
extern const char kKrb5CCFile[];
// Kerberos config file name.
extern const char kKrb5ConfFile[];

// Helper class to update Kerberos credential cache and config files used by
// Chrome for Kerberos authentication.
class KerberosFilesHandler {
 public:
  explicit KerberosFilesHandler(base::RepeatingClosure get_kerberos_files);

  KerberosFilesHandler(const KerberosFilesHandler&) = delete;
  KerberosFilesHandler& operator=(const KerberosFilesHandler&) = delete;

  virtual ~KerberosFilesHandler();

  // Writes the Kerberos credentials to disk asynchronously.
  void SetFiles(std::optional<std::string> krb5cc,
                std::optional<std::string> krb5conf);

  // Deletes the Kerberos credentials from disk asynchronously.
  virtual void DeleteFiles();

  // Sets a callback for when disk IO task posted by SetFiles has finished.
  void SetFilesChangedForTesting(base::OnceClosure callback);

 private:
  // Called whenever prefs::kDisableAuthNegotiateCnameLookup is changed.
  void OnDisabledAuthNegotiateCnameLookupChanged();

  // Forwards to |files_changed_for_testing_| if set.
  void OnFilesChanged();

  PrefMember<bool> negotiate_disable_cname_lookup_;

  // Triggers a fetch of Kerberos files. Called when the watched pref changes.
  base::RepeatingClosure get_kerberos_files_;

  // Called when disk IO queued by SetFiles has finished.
  base::OnceClosure files_changed_for_testing_;

  base::WeakPtrFactory<KerberosFilesHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_KERBEROS_KERBEROS_FILES_HANDLER_H_

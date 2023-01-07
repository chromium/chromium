// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_INFO_H_
#define CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_INFO_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/version.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "url/gurl.h"

namespace extensions {
FORWARD_DECLARE_TEST(ExtensionServiceTest, AddPendingExtensionFromSync);

class Extension;

// A pending extension is an extension that hasn't been installed yet
// and is intended to be installed in the next auto-update cycle.  The
// update URL of a pending extension may be blank, in which case a
// default one is assumed.
// TODO(skerner): Make this class an implementation detail of
// PendingExtensionManager, and remove all other users.
class PendingExtensionInfo {
 public:
  typedef bool (*ShouldAllowInstallPredicate)(const Extension*,
                                              content::BrowserContext* context);

  PendingExtensionInfo(const std::string& id,
                       const std::string& install_parameter,
                       const GURL& update_url,
                       const base::Version& version,
                       ShouldAllowInstallPredicate should_allow_install,
                       bool is_from_sync,
                       mojom::ManifestLocation install_source,
                       int creation_flags,
                       bool mark_acknowledged,
                       bool remote_install);

  PendingExtensionInfo(PendingExtensionInfo&& other);
  PendingExtensionInfo& operator=(PendingExtensionInfo&& other);

  PendingExtensionInfo(const PendingExtensionInfo& other) = delete;
  PendingExtensionInfo& operator=(const PendingExtensionInfo& other) = delete;

  ~PendingExtensionInfo();

  // Consider two PendingExtensionInfos equal if their ids are equal.
  bool operator==(const PendingExtensionInfo& rhs) const;

  const std::string& id() const { return id_; }
  const GURL& update_url() const { return update_url_; }
  const base::Version& version() const { return version_; }
  const std::string& install_parameter() const { return install_parameter_; }

  // ShouldAllowInstall() returns the result of running constructor argument
  // |should_allow_install| on an extension. After an extension is unpacked,
  // this function is run. If it returns true, the extension is installed.
  // If not, the extension is discarded. This allows creators of
  // PendingExtensionInfo objects to ensure that extensions meet some criteria
  // that can only be tested once the extension is unpacked.
  bool ShouldAllowInstall(const Extension* extension,
                          content::BrowserContext* context) const {
    return should_allow_install_(extension, context);
  }
  bool is_from_sync() const { return is_from_sync_; }
  mojom::ManifestLocation install_source() const { return install_source_; }
  int creation_flags() const { return creation_flags_; }
  bool mark_acknowledged() const { return mark_acknowledged_; }
  bool remote_install() const { return remote_install_; }

  // Returns -1, 0 or 1 if |this| has lower, equal or higher precedence than
  // |other|, respectively. "Equal" precedence means that the version and the
  // install source match. "Higher" precedence means that the version is newer,
  // or the version matches but the install source has higher priority.
  // It is only valid to invoke this when the ids match.
  int CompareTo(const PendingExtensionInfo& other) const;

 private:
  std::string id_;

  GURL update_url_;
  base::Version version_;
  std::string install_parameter_;

  // When the extension is about to be installed, this function is
  // called.  If this function returns true, the install proceeds.  If
  // this function returns false, the install is aborted.
  ShouldAllowInstallPredicate should_allow_install_;

  bool is_from_sync_;  // This update check was initiated from sync.
  mojom::ManifestLocation install_source_;
  int creation_flags_;
  bool mark_acknowledged_;
  bool remote_install_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, AddPendingExtensionFromSync);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_INFO_H_

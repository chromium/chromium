// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_FAILURES_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_FAILURES_H_

#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {

// Encapsulates reasons for installation failures, saving and getting them.
// Guaranteed reasons only for force-installed extensions.
class InstallationFailures {
 public:
  // Enum used for UMA. Do NOT reorder or remove entry. Don't forget to
  // update enums.xml when adding new entries.
  enum class Reason {
    // Reason for the failure is not reported.
    UNKNOWN = 0,

    // Invalid id of the extension.
    INVALID_ID = 1,

    // Error during parsing extension individual settings.
    MALFORMED_EXTENSION_SETTINGS = 2,

    // The extension is marked as replaced by ARC app.
    REPLACED_BY_ARC_APP = 3,

    // Malformed extension dictionary for the extension.
    MALFORMED_EXTENSION_DICT = 4,

    // The extension format from extension dict is not supported.
    NOT_SUPPORTED_EXTENSION_DICT = 5,

    // Invalid file path in the extension dict.
    MALFORMED_EXTENSION_DICT_FILE_PATH = 6,

    // Invalid version in the extension dict.
    MALFORMED_EXTENSION_DICT_VERSION = 7,

    // Invalid updated URL in the extension dict.
    MALFORMED_EXTENSION_DICT_UPDATE_URL = 8,

    // The extension doesn't support browser locale.
    LOCALE_NOT_SUPPORTED = 9,

    // The extension marked as it shouldn't be installed.
    NOT_PERFORMING_NEW_INSTALL = 10,

    // Profile is older than supported by the extension.
    TOO_OLD_PROFILE = 11,

    // The extension can't be installed for enterprise.
    DO_NOT_INSTALL_FOR_ENTERPRISE = 12,

    // The extension is already installed.
    ALREADY_INSTALLED = 13,

    // The download of the crx failed.
    CRX_FETCH_FAILED = 14,

    // Failed to fetch the manifest for this extension.
    MANIFEST_FETCH_FAILED = 15,

    // The manifest couldn't be parsed.
    MANIFEST_INVALID = 16,

    // The manifest was fetched and parsed, and there are no updates for this
    // extension.
    NO_UPDATE = 17,

    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = NO_UPDATE
  };

  // Remembers failure reason in memory.
  static void ReportFailure(const Profile* profile,
                            const ExtensionId& id,
                            Reason reason);

  // Retrieves reason for installation failure.
  // Returns UNKNOWN if not found.
  static Reason Get(const Profile* profile, const ExtensionId& id);

  // Clears all failures for the given profile.
  static void Clear(const Profile* profile);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_FAILURES_H_

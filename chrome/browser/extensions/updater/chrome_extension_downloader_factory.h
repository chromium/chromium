// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_CHROME_EXTENSION_DOWNLOADER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_CHROME_EXTENSION_DOWNLOADER_FACTORY_H_

#include <memory>
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "extensions/common/verifier_formats.h"

class Profile;

namespace crx_file {
enum class VerifierFormat;
}

namespace extensions {
class ExtensionDownloader;
class ExtensionDownloaderDelegate;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

// This provides a simple static interface for constructing an
// ExtensionDownloader suitable for use from within Chrome.
class ChromeExtensionDownloaderFactory {
 public:
  // Creates a downloader with a given "global" url loader factory instance.
  // No profile identity is associated with this downloader, which means:
  //
  // - when this method is called directly, |file_path| is empty.
  // - when this method is called through CreateForProfile, |profile_path| is
  //   non-empty.
  //
  // |profile_path| is used exclusely to support download of extensions through
  // the file:// protocol. In practice, it whitelists specific directories the
  // the browser has access to.
  static std::unique_ptr<extensions::ExtensionDownloader>
  CreateForURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      extensions::ExtensionDownloaderDelegate* delegate,
      crx_file::VerifierFormat required_verifier_format,
      const base::FilePath& profile_path = base::FilePath());

  // Creates a downloader for a given Profile. This downloader will be able
  // to authenticate as the signed-in user in the event that it's asked to
  // fetch a protected download.
  static std::unique_ptr<extensions::ExtensionDownloader> CreateForProfile(
      Profile* profile,
      extensions::ExtensionDownloaderDelegate* delegate);
};

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_CHROME_EXTENSION_DOWNLOADER_FACTORY_H_

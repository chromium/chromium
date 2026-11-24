// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_LIBRARY_SOURCE_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_LIBRARY_SOURCE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/on_device_translation/installer.h"

namespace on_device_translation {

// Provides the TranslateKit library to `OnDeviceTranslationInstallerImpl`.
//
// The library comes from a different place on each platform. Desktop
// platforms download it as a component, so that it updates on its own.
// Language packs don't depend on the library source.
class TranslateKitLibrarySource {
 public:
  // Gets updates about the library. Methods run on the UI thread.
  class Client {
   public:
    // Called when the value of `GetLibraryPath()` changed, for example after
    // an install or an update.
    virtual void OnLibraryChanged() = 0;
    // Called when an install that `Install()` started failed.
    virtual void OnLibraryInstallFailed(InstallError error) = 0;

   protected:
    virtual ~Client() = default;
  };

  // Returns the source for this platform. If `--translate-kit-binary-path` is
  // set, returns a source that always provides that path.
  static std::unique_ptr<TranslateKitLibrarySource> Create();

  TranslateKitLibrarySource() = default;
  TranslateKitLibrarySource(const TranslateKitLibrarySource&) = delete;
  TranslateKitLibrarySource& operator=(const TranslateKitLibrarySource&) =
      delete;
  virtual ~TranslateKitLibrarySource() = default;

  // Sets the client. Must be called once, before `Install()`. `client` must
  // outlive this object.
  void SetClient(Client* client);

  // Returns whether this source can provide a library on this device. If not,
  // translation is unavailable.
  virtual bool IsSupported() const;

  // Returns the absolute path of the library, or an empty path if it isn't
  // installed. Callers use this on every availability check, so it must not
  // block.
  virtual base::FilePath GetLibraryPath() const = 0;

  // Starts installing the library if it isn't installed yet. The client gets
  // `OnLibraryChanged()` when the library is ready, or
  // `OnLibraryInstallFailed()` if the install fails. Calling this again while
  // an install is in progress doesn't start a second one.
  virtual void Install() = 0;

 protected:
  Client* client() const { return client_; }

 private:
  raw_ptr<Client> client_ = nullptr;
};

// A source that provides a library at a fixed path. The path comes from the
// `--translate-kit-binary-path` switch, which developers and tests use to load
// a local build of the library. With an empty path, the source stands for a
// build that has no library, and translation is unavailable.
class FixedPathLibrarySource : public TranslateKitLibrarySource {
 public:
  explicit FixedPathLibrarySource(base::FilePath library_path);
  ~FixedPathLibrarySource() override;

  // TranslateKitLibrarySource:
  bool IsSupported() const override;
  base::FilePath GetLibraryPath() const override;
  void Install() override;

 private:
  const base::FilePath library_path_;
};

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_LIBRARY_SOURCE_H_

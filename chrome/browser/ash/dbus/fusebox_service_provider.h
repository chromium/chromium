// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_FUSEBOX_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_FUSEBOX_SERVICE_PROVIDER_H_

#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash {

// FuseBoxServiceProvider implements the org.chromium.FuseBoxService D-Bus
// interface.
class FuseBoxServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  FuseBoxServiceProvider();
  FuseBoxServiceProvider(const FuseBoxServiceProvider&) = delete;
  FuseBoxServiceProvider& operator=(const FuseBoxServiceProvider&) = delete;
  ~FuseBoxServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> object) override;

 private:
  // ParseResult is the type returned by ParseCommonDBusMethodArguments. It is
  // a result type (see https://en.wikipedia.org/wiki/Result_type), being
  // either an error or a value. In this case, the error type is a
  // base::File::Error (a numeric code) and the value type is a pair of
  // storage::FileSystemContext and storage::FileSystemURL.
  struct ParseResult {
    explicit ParseResult(base::File::Error error_code_arg);
    ParseResult(scoped_refptr<storage::FileSystemContext> fs_context_arg,
                storage::FileSystemURL fs_url_arg);
    ~ParseResult();

    base::File::Error error_code;
    scoped_refptr<storage::FileSystemContext> fs_context;
    storage::FileSystemURL fs_url;
  };

  // All of the D-Bus methods' arguments start with a FileSystemURL (as a
  // string). This method consumes and parses that first argument, as well as
  // finding the FileSystemContext we will need to serve those methods.
  ParseResult ParseCommonDBusMethodArguments(dbus::MessageReader* reader);

  // D-Bus methods.
  //
  // In terms of semantics, they're roughly equivalent to the C standard
  // library functions of the same name. For example, the Stat method here
  // corresponds to the standard stat function described by "man 2 stat".
  //
  // TODO(nigeltao): add Open, Read, ReadDir, etc.
  void Stat(dbus::MethodCall* method,
            dbus::ExportedObject::ResponseSender sender);

  // True if the FuseBoxService is enabled.
  bool const enabled_;

  // base::WeakPtr{this} factory.
  base::WeakPtrFactory<FuseBoxServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_FUSEBOX_SERVICE_PROVIDER_H_

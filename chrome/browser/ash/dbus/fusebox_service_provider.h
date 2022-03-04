// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_FUSEBOX_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_FUSEBOX_SERVICE_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"
#include "storage/browser/file_system/file_system_context.h"

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
  // storage::FileSystemOperationRunner::OpenFile gives us an
  // "on_close_calback" that we are supposed to run after the file is closed.
  // To complicate matters, we duplicate the underlying FD (file descriptor)
  // and pass it over D-Bus to the fusebox::kFuseBoxServiceInterface client for
  // which we are the fusebox::kFuseBoxServiceInterface server.
  //
  // This struct tracks when the underlying FD is closed in both client and
  // server. When its ref-count hits zero, the on_close_callback_runner
  // destructor will run the on_close_calback and the "Sequence" in
  // "RefCountedDeleteOnSequence" means that this happens on the IO thread.
  class OnCloseCallbackTracker
      : public base::RefCountedDeleteOnSequence<OnCloseCallbackTracker> {
   public:
    explicit OnCloseCallbackTracker(base::OnceClosure on_close_callback);
    OnCloseCallbackTracker(const OnCloseCallbackTracker&) = delete;
    OnCloseCallbackTracker& operator=(const OnCloseCallbackTracker&) = delete;

   private:
    ~OnCloseCallbackTracker();
    friend class base::DeleteHelper<OnCloseCallbackTracker>;
    friend class base::RefCountedDeleteOnSequence<OnCloseCallbackTracker>;

    base::ScopedClosureRunner on_close_callback_runner;
  };

  // trackers_ and next_tracker_key_ should only be accessed on the UI thread.
  base::flat_map<uint64_t, scoped_refptr<OnCloseCallbackTracker>> trackers_;
  uint64_t next_tracker_key_;

  // D-Bus methods.
  //
  // In terms of semantics, they're roughly equivalent to the C standard
  // library functions of the same name. For example, the Stat method here
  // corresponds to the standard stat function described by "man 2 stat".
  void Close(dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender sender);
  void Open(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender sender);
  void Read(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender sender);
  void ReadDir(dbus::MethodCall* method_call,
               dbus::ExportedObject::ResponseSender sender);
  void Stat(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender sender);

  void ReplyToOpenTypical(scoped_refptr<storage::FileSystemContext> fs_context,
                          dbus::MethodCall* method_call,
                          dbus::ExportedObject::ResponseSender sender,
                          base::File file,
                          base::OnceClosure on_close_callback);

  // base::WeakPtr{this} factory.
  base::WeakPtrFactory<FuseBoxServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_FUSEBOX_SERVICE_PROVIDER_H_

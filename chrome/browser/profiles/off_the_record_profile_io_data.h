// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IO_DATA_H_
#define CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IO_DATA_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/storage_partition_descriptor.h"

class Profile;

// OffTheRecordProfile owns a OffTheRecordProfileIOData::Handle, which holds a
// reference to the OffTheRecordProfileIOData. OffTheRecordProfileIOData is
// intended to own all the objects owned by OffTheRecordProfile which live on
// the IO thread, such as, but not limited to, network objects like
// CookieMonster, HttpTransactionFactory, etc. OffTheRecordProfileIOData is
// owned by the OffTheRecordProfile and OffTheRecordProfileIOData's
// ChromeURLRequestContexts. When all of them go away, then ProfileIOData will
// be deleted. Note that the OffTheRecordProfileIOData will typically outlive
// the Profile it is "owned" by, so it's important for OffTheRecordProfileIOData
// not to hold any references to the Profile beyond what's used by LazyParams
// (which should be deleted after lazy initialization).

class OffTheRecordProfileIOData : public ProfileIOData {
 public:
  class Handle {
   public:
    explicit Handle(Profile* profile);
    ~Handle();

    content::ResourceContext* GetResourceContext() const;
    // GetResourceContextNoInit() does not call LazyInitialize() so it can be
    // safely be used during initialization.
    content::ResourceContext* GetResourceContextNoInit() const;

   private:
    // Lazily initialize ProfileParams. We do this on the calls to
    // Get*RequestContextGetter(), so we only initialize ProfileParams right
    // before posting a task to the IO thread to start using them. This prevents
    // objects that are supposed to be deleted on the IO thread, but are created
    // on the UI thread from being unnecessarily initialized.
    void LazyInitialize() const;

    OffTheRecordProfileIOData* const io_data_;

    Profile* const profile_;

    mutable bool initialized_;

    DISALLOW_COPY_AND_ASSIGN(Handle);
  };

 private:
  OffTheRecordProfileIOData();
  ~OffTheRecordProfileIOData() override;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordProfileIOData);
};

#endif  // CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IO_DATA_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_io_data_handle.h"

#include "chrome/browser/profiles/profile_io_data.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

ProfileIODataHandle::ProfileIODataHandle(Profile* profile)
    : io_data_(std::make_unique<ProfileIOData>()),
      profile_(profile),
      initialized_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile);
}

ProfileIODataHandle::~ProfileIODataHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

content::ResourceContext* ProfileIODataHandle::GetResourceContext() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize();
  return io_data_->GetResourceContext();
}

void ProfileIODataHandle::LazyInitialize() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialized_)
    return;

  // Set initialized_ to true at the beginning in case any of the objects
  // below try to get the ResourceContext pointer.
  initialized_ = true;
  io_data_->InitializeOnUIThread(profile_);
}

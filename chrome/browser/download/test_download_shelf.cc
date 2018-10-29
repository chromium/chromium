// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/test_download_shelf.h"

#include "content/public/browser/download_manager.h"

TestDownloadShelf::TestDownloadShelf()
    : is_showing_(false), did_add_download_(false), profile_(nullptr) {}

TestDownloadShelf::~TestDownloadShelf() {
}

bool TestDownloadShelf::IsShowing() const {
  return is_showing_;
}

bool TestDownloadShelf::IsClosing() const {
  return false;
}

Browser* TestDownloadShelf::browser() const {
  return NULL;
}

void TestDownloadShelf::DoAddDownload(DownloadUIModelPtr download) {
  did_add_download_ = true;
}

void TestDownloadShelf::DoOpen() {
  is_showing_ = true;
}

void TestDownloadShelf::DoClose(CloseReason reason) {
  is_showing_ = false;
}

void TestDownloadShelf::DoHide() {
  is_showing_ = false;
}

void TestDownloadShelf::DoUnhide() {
  is_showing_ = true;
}

base::TimeDelta TestDownloadShelf::GetTransientDownloadShowDelay() {
  return base::TimeDelta();
}

Profile* TestDownloadShelf::profile() const {
  return profile_;
}
